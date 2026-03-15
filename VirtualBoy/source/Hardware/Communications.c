/*
 * VUEngine Core
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// INCLUDES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#ifdef __DEBUG_TOOL
#include <Debug.h>
#endif
#include <DebugConfig.h>
#include <Mem.h>
#include <Printer.h>
#include <Profiler.h>
#include <Telegram.h>
#include <Singleton.h>
#include <DisplayUnit.h>
#include <VUEngine.h>

#include <Communications.h>


//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' MACROS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define	__CCR							0x00	// Communication Control Register	(0x0200 0000)
#define	__CCSR							0x04	// COMCNT Control Register			(0x0200 0004)
#define	__CDTR							0x08	// Transmitted Data Register		(0x0200 0008)
#define	__CDRR							0x0C	// Received Data Register			(0x0200 000C)

// communicating penging flag
#define	__COM_PENDING					0x02
// start communication
#define	__COM_START						0x04
// use external clock (remote)
#define	__COM_USE_EXTERNAL_CLOCK		0x10
// disable interrupt
#define	__COM_DISABLE_INTERRUPT			0x80

// start communication as remote
#define	__COM_AS_REMOTE					(__COM_USE_EXTERNAL_CLOCK)
// start communication as master
#define	__COM_AS_MASTER					(0x00)

#define __COM_HANDSHAKE					0x34
#define __COM_CHECKSUM					0x44

#define	__MESSAGE_SIZE					sizeof(uint32)

#define __REMOTE_READY_MESSAGE			0x43873AD1
#define __MASTER_FRMCYC_SET_MESSAGE		0x5DC289F4

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DATA
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

enum CommunicationsBroadcastStates
{
	kCommunicationsBroadcastNone = 0,
	kCommunicationsBroadcastSync,
	kCommunicationsBroadcastAsync
};

enum CommunicationsStatus
{
	kCommunicationsStatusReset = 0,
	kCommunicationsStatusIdle,
	kCommunicationsStatusSendingHandshake,
	kCommunicationsStatusSendingPayload,
	kCommunicationsStatusWaitingPayload,
	kCommunicationsStatusSendingAndReceivingPayload,
};

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' ATTRIBUTES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static volatile uint8* _communicationRegisters 				= (uint8*)0x02000000;

/// Status of the communications
static volatile int32 _status  								= kCommunicationsStatusReset;

/// Data sent over the EXT port
static volatile uint8* _sentData 							= NULL;

/// Data received over the EXT port
static volatile uint8* _receivedData 						= NULL;

/// Last byte sent synchronously over the EXT port
static volatile uint8* _syncSentByte 						= NULL;

/// Last byte received synchronously over the EXT port
static volatile uint8* _syncReceivedByte 					= NULL;

/// Last byte sent asynchronously over the EXT port
static volatile uint8* _asyncSentByte 						= NULL;

/// Last byte received asynchronously over the EXT port
static volatile uint8* _asyncReceivedByte 					= NULL;

/// Number of bytes pending transmission over the EXT port
static volatile int32 _numberOfBytesPendingTransmission 	= 0;

/// Number of bytes already transmitted over the EXT port
static int32 _numberOfBytesPreviouslySent 					= 0;

/// Status of broadcast communications
static volatile uint32 _broadcast 							= kCommunicationsBroadcastNone;

/// Flag that indicates if there is something connected to the EXT port
static volatile bool _connected 							= false;

/// Keeps track of the role as master or slave that the system holds in data transmissions
static volatile uint8 _communicationMode 					= __COM_AS_REMOTE;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::interruptHandler()
{
#ifdef __ENABLE_PROFILER
	Profiler::lap(kProfilerLapTypeStartInterrupt, NULL);
#endif

	if(kCommunicationsBroadcastNone == _broadcast && Communications::isMaster())
	{
		_communicationMode = __COM_AS_REMOTE;
	}
	else
	{
		_communicationMode = __COM_AS_MASTER;
	}

	Communications::endCommunications();

	Communications::processInterrupt();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

bool Communications::handleMessage(Telegram telegram)
{
	switch(Telegram::getMessage(telegram))
	{
		case kMessageCheckIfRemoteIsReady:
		{
			if(Communications::isRemoteReady())
			{
				Communications::startClockSignal();
			}
			else
			{
				Communications::waitForRemote();
			}

			return true;
		}
	}

	return false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::reset()
{
	Communications::getInstance();
	Communications::cancelCommunications();
	
	_status = kCommunicationsStatusReset;
	_broadcast = kCommunicationsBroadcastNone;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::enableCommunications(ListenerObject scope)
{
	if(_connected || kCommunicationsStatusReset != _status)
	{
		return;
	}

	Communications::endCommunications();

	if(!isDeleted(scope))
	{
		Communications::addEventListener(Communications::getInstance(), scope, kEventCommunicationsConnected);
	}

	// If handshake is taking place
	if(Communications::isHandshakeIncoming())
	{
		// There is another system attached already managing the channel
		_communicationMode = __COM_AS_MASTER;

		// Send dummy payload to verify communication
		Communications::sendHandshake();
	}
	else
	{
		// I'm alone since a previously turned on
		// System would had already closed the channel
		_communicationMode = __COM_AS_REMOTE;

		// Wait for incoming clock
		Communications::sendHandshake();
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::disableCommunications()
{
	Communications::reset();	
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::cancelCommunications()
{
	Communications::endCommunications();

	if(NULL != _sentData)
	{
		delete _sentData;
	}

	if(NULL != _receivedData)
	{
		delete _receivedData;
	}

	_sentData = NULL;
	_syncSentByte = NULL;
	_asyncSentByte = NULL;
	_receivedData = NULL;
	_syncReceivedByte = NULL;
	_asyncReceivedByte = NULL;
	_status = kCommunicationsStatusIdle;
	_broadcast = kCommunicationsBroadcastNone;
	_numberOfBytesPendingTransmission = 0;
	_numberOfBytesPreviouslySent = 0;
	
	Communications::removeEventListeners(Communications::getInstance(), NULL, kEventEngineFirst);
	Communications::discardAllMessages(Communications::getInstance());

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::startSyncCycle()
{
	if(!_connected)
	{
		return;
	}

	Communications::cancelCommunications();

	DisplayUnit::startMemoryRefresh();

	Communications::correctDataDrift();

	if(Communications::isMaster())
	{
		uint32 message = 0;
		do
		{
			Communications::receiveData((uint8*)&message, sizeof(message));
		}
		while(__REMOTE_READY_MESSAGE != message);

		message = __MASTER_FRMCYC_SET_MESSAGE;

		DisplayUnit::waitForFrame();

		Communications::sendData((uint8*)&message, sizeof(message));
	}
	else
	{
		uint32 message = __REMOTE_READY_MESSAGE;
		Communications::sendData((uint8*)&message, sizeof(message));

		do
		{
			Communications::receiveData((uint8*)&message, sizeof(message));
		}
		while(__MASTER_FRMCYC_SET_MESSAGE != message);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::isConnected()
{
	return _connected;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::isMaster()
{
	return __COM_AS_MASTER == _communicationMode;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::broadcastData(uint8* data, int32 numberOfBytes)
{
	if(!Communications::getReadyToBroadcast())
	{
		return false;
	}

	_broadcast = kCommunicationsBroadcastSync;

	while(0 < numberOfBytes)
	{
		Communications::sendPayload(*data, false);

		while(Communications::isTransmitting());
		_status = kCommunicationsStatusIdle;

		data++;

		numberOfBytes--;
	}
	
	_broadcast = kCommunicationsBroadcastNone;

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::broadcastDataAsync(uint8* data, int32 numberOfBytes, ListenerObject scope)
{
	if(!Communications::getReadyToBroadcast())
	{
		return false;
	}

	_broadcast = kCommunicationsBroadcastAsync;

	Communications::sendDataAsync(data, numberOfBytes, scope);

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::sendAndReceiveData(uint32 message, uint8* data, int32 numberOfBytes)
{
	if(kCommunicationsStatusReset == _status)
	{
		Communications::startSyncCycle();
	}

	return Communications::startBidirectionalDataTransmission(message, data, numberOfBytes);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::sendAndReceiveDataAsync
(
	uint32 message, uint8* data, int32 numberOfBytes, ListenerObject scope
)
{
	if(kCommunicationsStatusReset == _status)
	{
		Communications::startSyncCycle();
	}

	return Communications::startBidirectionalDataTransmissionAsync(message, data, numberOfBytes, scope);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint32 Communications::getSentMessage()
{
	return *(uint32*)_sentData;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint32 Communications::getReceivedMessage()
{
	return *(uint32*)_receivedData;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

const uint8* Communications::getSentData()
{
	return (const uint8*)_sentData + __MESSAGE_SIZE;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static const uint8* Communications::getReceivedData()
{
	return (const uint8*)_receivedData + __MESSAGE_SIZE;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#ifndef __SHIPPING
static void Communications::print(int32 x, int32 y)
{
	PRINT_TEXT(Communications::isConnected() ? "Connected   " : "Disconnected", x, y);

	char* helper = "";
	int32 valueDisplacement = 20;

	PRINT_TIME(x + valueDisplacement, y++);

	switch(_status)
	{
		case kCommunicationsStatusIdle:
		{
			helper = "Idle             ";
			break;
		}

		case kCommunicationsStatusSendingHandshake:
		{
			helper = "Sending handshake         ";
			break;
		}

		case kCommunicationsStatusWaitingPayload:
		{
			helper = "Waiting payload            ";
			break;
		}

		case kCommunicationsStatusSendingPayload:
		{
			helper = "Sending payload            ";
			break;
		}

		case kCommunicationsStatusSendingAndReceivingPayload:
		{
			helper = "Send / Recv payload        ";
			break;
		}

		default:
		{
			helper = "Error            ";
			break;
		}
	}

	PRINT_TEXT("Status:", x, y);
	PRINT_INT(_status, x + valueDisplacement * 2, y);
	PRINT_TEXT(helper, x + valueDisplacement, y++);

	PRINT_TEXT("Mode: ", x, y);
	PRINT_TEXT(__COM_AS_REMOTE == _communicationMode ? "Remote" : "Master ", x + valueDisplacement, y++);

	PRINT_TEXT("Channel: ", x, y);
	PRINT_TEXT(Communications::isAuxChannelOpen() ? "Aux Open    " : "Aux Closed ", x + valueDisplacement, y++);
	PRINT_TEXT(Communications::isRemoteReady() ? "Comms Open    " : "Comms Closed ", x + valueDisplacement, y++);

	PRINT_TEXT("Transmission: ", x, y);
	PRINT_TEXT(_communicationRegisters[__CCR] & __COM_PENDING ? "Pending" : "Done  ", x + valueDisplacement, y++);
	PRINT_TEXT("CCR: ", x, y);
	PRINT_HEX(_communicationRegisters[__CCR], x + valueDisplacement, y++);
	PRINT_TEXT("CCSR: ", x, y);
	PRINT_HEX(_communicationRegisters[__CCSR], x + valueDisplacement, y++);

	PRINT_TEXT("Sig: ", x, y);
	PRINT_INT((_communicationRegisters[__CCSR] >> 3) & 0x01, x + valueDisplacement, y++);
	PRINT_TEXT("Smp: ", x, y);
	PRINT_INT((_communicationRegisters[__CCSR] >> 2) & 0x01, x + valueDisplacement, y++);

	PRINT_TEXT("Write: ", x, y);
	PRINT_INT((_communicationRegisters[__CCSR] >> 1) & 0x01, x + valueDisplacement, y++);
	PRINT_TEXT("Read: ", x, y);
	PRINT_INT(_communicationRegisters[__CCSR] & 0x01, x + valueDisplacement, y++);
}
#endif

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::isTransmitting()
{
	return _communicationRegisters[__CCR] & __COM_PENDING ? true : false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::managesChannel()
{
	return !Communications::isMaster();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::isHandshakeIncoming()
{
	// Try to close the communication channel
	Communications::setReady(false);

	// If channel was unsuccessfully closed,
	// Then there is a handshake taking place
	return Communications::isRemoteReady();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::isRemoteReady()
{
	return 0 == (_communicationRegisters[__CCSR] & 0x01) ? true : false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::isCommunicationControlInterrupt()
{
	return 0 == (_communicationRegisters[__CCSR] & __COM_DISABLE_INTERRUPT);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::isAuxChannelOpen()
{
	return _communicationRegisters[__CCSR] & 0x01 ? true : false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::isFreeForTransmissions()
{
	return (
		(_connected || _broadcast) &&
		NULL == _syncSentByte &&
		NULL == _syncReceivedByte &&
		NULL == _asyncSentByte &&
		NULL == _asyncReceivedByte &&
		kCommunicationsStatusIdle >= _status
	);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::endCommunications()
{
	Communications::setReady(false);

	_communicationRegisters[__CCR] = __COM_DISABLE_INTERRUPT;
	_communicationRegisters[__CDTR] = 0;
	_communicationRegisters[__CDRR] = 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::waitForRemote()
{
	Communications::sendMessageToSelf(Communications::getInstance(), kMessageCheckIfRemoteIsReady, 1, 0);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::sendHandshake()
{
	_status = kCommunicationsStatusSendingHandshake;
	Communications::startTransmissions(__COM_HANDSHAKE, false);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::startClockSignal()
{
	// Make sure to disable COMCNT interrupts
	// _communicationRegisters[__CCSR] |= __COM_DISABLE_INTERRUPT;

	switch(_broadcast)
	{
		case kCommunicationsBroadcastSync:
		{
			_communicationRegisters[__CCR] = __COM_DISABLE_INTERRUPT | _communicationMode | __COM_START;

			_numberOfBytesPendingTransmission = 0;
			_status = kCommunicationsStatusIdle;
			break;
		}

		default:
		{
			// Start communications
			_communicationRegisters[__CCR] = _communicationMode | __COM_START;
			break;
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::correctDataDrift()
{
	Communications::cancelCommunications();

	// The received message and data can be shifted due to the chance of this method being called out of sync
	// So, we create composite value to circle shift the result and compare with it
	uint64 target = (((uint64)(uint32)Communications::getClass()) << 32) | (*(uint32*)Communications::getClass());

	do
	{
		// Data transmission can fail if there was already a request to send data.
		if
		(
			!Communications::startBidirectionalDataTransmission
			(
				(uint32)Communications::getClass(), 
				(uint8*)Communications::getClass(), sizeof((uint32)Communications::getClass())
			)
		)
		{
			// In this case, simply cancel all communications and try again. This supposes
			// that there are no other calls that could cause a race condition.
			Communications::cancelCommunications();
			continue;
		}

		// Create the composite result to circle shift the results
		uint64 result = (((uint64)Communications::getReceivedMessage()) << 32) | (*(uint32*)Communications::getReceivedData());

		if(target == result)
		{
			return;
		}

		// Shifts be of 4 bits each
		int16 shifts = 64;
		
		while(0 < shifts)
		{
			// Circle shift the result fo the left
			result = ((result & 0xFF00000000000000) >> 60) | (result << 4);

			// If the shifted result is equal to the target
			if(result == target)
			{
				// Performe a dummy transmission to shift the send data by a byte
				Communications::startBidirectionalDataTransmission((uint32)result, (uint8*)&result, 1);

				break;
			}

			shifts -= 4;
		}
	}
	while(true);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::getReadyToBroadcast()
{
	if(Communications::isConnected())
	{
		return false;
	}

	switch (_status)
	{
		case kCommunicationsStatusSendingHandshake:
		{
			Communications::cancelCommunications();			
		}
		// Intended fallthrough
		case kCommunicationsStatusIdle:
		case kCommunicationsStatusReset:
		{
			_status = kCommunicationsStatusIdle;
			break;
		}

		default:
		{
			return false;
		}
	}

	// Always start comms as master when broadcasting
	_communicationMode = __COM_AS_MASTER;

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::startTransmissions(uint8 payload, bool async)
{
	// Set transmission data
	_communicationRegisters[__CDTR] = payload;

	Hardware::enableInterrupts();

	// Master must wait for slave to open the channel
	if(Communications::isMaster())
	{
		Communications::setReady(true);

		if(kCommunicationsBroadcastNone != _broadcast)
		{
			Communications::startClockSignal();
			return;
		}

		if(async)
		{
			if(Communications::isRemoteReady())
			{
				Communications::startClockSignal();
			}
			else
			{
				Communications::waitForRemote();
			}
		}
		else
		{
			while(!Communications::isRemoteReady());

			Communications::startClockSignal();
		}
	}
	else
	{
		// Set Start flag
		Communications::startClockSignal();

		// Open communications channel
		Communications::setReady(true);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::setReady(bool ready)
{
	if(ready)
	{
		if(Communications::isMaster())
		{
			if(_broadcast)
			{
				_communicationRegisters[__CCSR] &= (~0x02);
			}
			else
			{
				_communicationRegisters[__CCSR] |= 0x02;
			}
		}
		else
		{
			_communicationRegisters[__CCSR] &= (~0x02);
		}
	}
	else
	{
		_communicationRegisters[__CCSR] |= 0x02;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::sendPayload(uint8 payload, bool async)
{
	if(kCommunicationsStatusIdle == _status)
	{
		_status = kCommunicationsStatusSendingPayload;
		Communications::startTransmissions(payload, async);
		return true;
	}

	return false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::receivePayload(bool async)
{
	if(kCommunicationsStatusIdle == _status)
	{
		_status = kCommunicationsStatusWaitingPayload;
		Communications::startTransmissions(__COM_CHECKSUM, async);
		return true;
	}

	return false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::sendAndReceivePayload(uint8 payload, bool async)
{
	if(kCommunicationsStatusIdle == _status)
	{
		_status = kCommunicationsStatusSendingAndReceivingPayload;
		Communications::startTransmissions(payload, async);
		return true;
	}

	return false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::startDataTransmission(uint8* data, int32 numberOfBytes, bool sendingData)
{
	if((sendingData && NULL == data) || 0 >= numberOfBytes || !Communications::isFreeForTransmissions())
	{
		return false;
	}

	if(NULL != _sentData)
	{
		delete _sentData;
	}

	if(NULL != _receivedData)
	{
		delete _receivedData;
	}

	_sentData = NULL;
	_receivedData = NULL;
	_syncSentByte = NULL;
	_syncReceivedByte = NULL;
	_asyncSentByte = NULL;
	_asyncReceivedByte = NULL;

	if(sendingData)
	{
		_syncSentByte = data;
	}
	else
	{
		_syncReceivedByte = data;
	}

	_numberOfBytesPendingTransmission = numberOfBytes;

	while(0 < _numberOfBytesPendingTransmission)
	{
		if(sendingData)
		{
			Communications::sendPayload(*_syncSentByte, false);
		}
		else
		{
			Communications::receivePayload(false);
		}

		while(kCommunicationsStatusIdle != _status);
	}

	_status = kCommunicationsStatusIdle;
	_syncSentByte = _syncReceivedByte = NULL;

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::sendDataAsync(uint8* data, int32 numberOfBytes, ListenerObject scope)
{
	if(NULL == data || 0 >= numberOfBytes || !Communications::isFreeForTransmissions())
	{
		return false;
	}

	if(!isDeleted(scope))
	{
		Communications::removeEventListeners(Communications::getInstance(), NULL, kEventCommunicationsTransmissionCompleted);
		Communications::addEventListener(Communications::getInstance(), scope, kEventCommunicationsTransmissionCompleted);
	}

	if(_numberOfBytesPreviouslySent < numberOfBytes)
	{
		if(!isDeleted(_sentData))
		{
			delete _sentData;
			_sentData = NULL;
		}
	}

	if(isDeleted(_sentData))
	{
		// Allocate memory to hold both the message and the data
		_sentData = (uint8*)((uint32)MemoryPool::allocate(numberOfBytes + __DYNAMIC_STRUCT_PAD) + __DYNAMIC_STRUCT_PAD);
	}

	if(!isDeleted(_receivedData))
	{
		delete _receivedData;
		_receivedData = NULL;
	}

	_syncSentByte = _syncReceivedByte = NULL;
	_asyncSentByte = _sentData;
	_asyncReceivedByte = _receivedData;
	_numberOfBytesPreviouslySent = numberOfBytes;
	_numberOfBytesPendingTransmission = numberOfBytes;

	// Copy the data
	Mem::copyBYTE((uint8*)_sentData, data, numberOfBytes);

	Communications::sendPayload(*_asyncSentByte, true);

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::sendData(uint8* data, int32 numberOfBytes)
{
	return Communications::startDataTransmission(data, numberOfBytes, true);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::receiveData(uint8* data, int32 numberOfBytes)
{
	return Communications::startDataTransmission(data, numberOfBytes, false);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::startBidirectionalDataTransmission(uint32 message, uint8* data, int32 numberOfBytes)
{
	if((NULL == data) || 0 >= numberOfBytes || !Communications::isFreeForTransmissions())
	{
		return false;
	}

	if(_numberOfBytesPreviouslySent < numberOfBytes)
	{
		if(!isDeleted(_sentData))
		{
			delete _sentData;
			_sentData = NULL;
		}

		if(!isDeleted(_receivedData))
		{
			delete _receivedData;
			_receivedData = NULL;
		}
	}

	if(isDeleted(_sentData))
	{
		_sentData = 
			(uint8*)((uint32)MemoryPool::allocate(numberOfBytes + __DYNAMIC_STRUCT_PAD + __MESSAGE_SIZE) + __DYNAMIC_STRUCT_PAD);
	}

	if(isDeleted(_receivedData))
	{
		_receivedData = 
			(uint8*)((uint32)MemoryPool::allocate(numberOfBytes + __DYNAMIC_STRUCT_PAD + __MESSAGE_SIZE) + __DYNAMIC_STRUCT_PAD);
	}

	_asyncSentByte = _asyncReceivedByte = NULL;
	_syncSentByte = _sentData;
	_syncReceivedByte = _receivedData;
	_numberOfBytesPreviouslySent = numberOfBytes;	
	_numberOfBytesPendingTransmission = numberOfBytes + __MESSAGE_SIZE;

	// Save the message
	*(uint32*)_sentData = message;

	// Copy the data
	Mem::copyBYTE((uint8*)_sentData + __MESSAGE_SIZE, data, numberOfBytes);

	while(0 < _numberOfBytesPendingTransmission)
	{
		Communications::sendAndReceivePayload(*_syncSentByte, false);

		while(kCommunicationsStatusIdle != _status)
		{
			Hardware::halt();
		}
	}

	_status = kCommunicationsStatusIdle;
	_syncSentByte = _syncReceivedByte = NULL;

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Communications::startBidirectionalDataTransmissionAsync
(
	uint32 message, uint8* data, int32 numberOfBytes, ListenerObject scope
)
{
	if(NULL == data || 0 >= numberOfBytes || !Communications::isFreeForTransmissions())
	{
		return false;
	}

	if(!isDeleted(scope))
	{
		Communications::removeEventListeners(Communications::getInstance(), NULL, kEventCommunicationsTransmissionCompleted);
		Communications::addEventListener(Communications::getInstance(), scope, kEventCommunicationsTransmissionCompleted);
	}

	if(_numberOfBytesPreviouslySent < numberOfBytes)
	{
		if(!isDeleted(_sentData))
		{
			delete _sentData;
			_sentData = NULL;
		}

		if(!isDeleted(_receivedData))
		{
			delete _receivedData;
			_receivedData = NULL;
		}
	}

	if(isDeleted(_sentData))
	{
		// Allocate memory to hold both the message and the data
		_sentData = 
			(uint8*)((uint32)MemoryPool::allocate(numberOfBytes + __DYNAMIC_STRUCT_PAD + __MESSAGE_SIZE) + __DYNAMIC_STRUCT_PAD);
	}

	if(isDeleted(_receivedData))
	{
		_receivedData = 
			(uint8*)((uint32)MemoryPool::allocate(numberOfBytes + __DYNAMIC_STRUCT_PAD + __MESSAGE_SIZE) + __DYNAMIC_STRUCT_PAD);
	}

	_syncSentByte = _syncReceivedByte = NULL;
	_asyncSentByte = _sentData;
	_asyncReceivedByte = _receivedData;
	_numberOfBytesPreviouslySent = numberOfBytes;	
	_numberOfBytesPendingTransmission = numberOfBytes + __MESSAGE_SIZE;

	// Save the message
	*(uint32*)_sentData = message;

	// Copy the data
	Mem::copyBYTE((uint8*)_sentData + __MESSAGE_SIZE, data, numberOfBytes);
	Communications::sendAndReceivePayload(*_asyncSentByte, true);

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Communications::processInterrupt()
{
	int32 status = _status;
	_status = kCommunicationsStatusIdle;

	switch(status)
	{
		case kCommunicationsStatusSendingHandshake:
		{
			if(__COM_HANDSHAKE != _communicationRegisters[__CDRR])
			{
				Communications::sendHandshake();
				break;
			}

			_connected = true;
			
			Communications::fireEvent(Communications::getInstance(), kEventCommunicationsConnected);
			NM_ASSERT(!isDeleted(Communications::getInstance()), "Communications::processInterrupt: deleted this during kEventCommunicationsConnected");
			Communications::removeEventListeners(Communications::getInstance(), NULL, kEventCommunicationsConnected);
		}

		default:
		{
			_connected = !_broadcast;
			break;
		}
	}

	switch(status)
	{
		case kCommunicationsStatusWaitingPayload:
		{
			if(_syncReceivedByte)
			{
				*_syncReceivedByte = _communicationRegisters[__CDRR];
				_syncReceivedByte++;
				--_numberOfBytesPendingTransmission;
			}
			else if(_asyncReceivedByte)
			{
				*_asyncReceivedByte = _communicationRegisters[__CDRR];
				_asyncReceivedByte++;

				if(0 < --_numberOfBytesPendingTransmission)
				{
					Communications::receivePayload(false);
				}
				else
				{
					Communications::fireEvent(Communications::getInstance(), kEventCommunicationsTransmissionCompleted);

					NM_ASSERT
					(
						!isDeleted(Communications::getInstance()), 
						"Communications::processInterrupt: deleted this during kEventCommunicationsTransmissionCompleted"
					);

					Communications::removeEventListeners(Communications::getInstance(), NULL, kEventCommunicationsTransmissionCompleted);
				}
			}

			break;
		}

		case kCommunicationsStatusSendingPayload:
		{
			if(_syncSentByte)
			{
				_syncSentByte++;
				--_numberOfBytesPendingTransmission;
			}
			else if(_asyncSentByte)
			{
				_asyncSentByte++;

				if(0 < --_numberOfBytesPendingTransmission)
				{
					Communications::sendPayload(*_asyncSentByte, true);
				}
				else
				{
					Communications::fireEvent(Communications::getInstance(), kEventCommunicationsTransmissionCompleted);
					
					NM_ASSERT
					(
						!isDeleted(Communications::getInstance()), 
						"Communications::processInterrupt: deleted this during kEventCommunicationsTransmissionCompleted"
					);

					Communications::removeEventListeners(Communications::getInstance(), NULL, kEventCommunicationsTransmissionCompleted);
					_asyncSentByte = NULL;
					_broadcast = kCommunicationsBroadcastNone;
				}
			}

			break;
		}

		case kCommunicationsStatusSendingAndReceivingPayload:
		{
			if(_syncSentByte && _syncReceivedByte)
			{
				*_syncReceivedByte = _communicationRegisters[__CDRR];
				_syncReceivedByte++;
				_syncSentByte++;
				--_numberOfBytesPendingTransmission;
			}
			else if(_asyncSentByte && _asyncReceivedByte)
			{
				*_asyncReceivedByte = _communicationRegisters[__CDRR];
				_asyncReceivedByte++;
				_asyncSentByte++;

				if(0 < --_numberOfBytesPendingTransmission)
				{
					Communications::sendAndReceivePayload(*_asyncSentByte, false);
				}
				else
				{
					Communications::fireEvent(Communications::getInstance(), kEventCommunicationsTransmissionCompleted);
					
					NM_ASSERT
					(
						!isDeleted(Communications::getInstance()), 
						"Communications::processInterrupt: deleted this during kEventCommunicationsTransmissionCompleted"
					);

					Communications::removeEventListeners(Communications::getInstance(), NULL, kEventCommunicationsTransmissionCompleted);
					_asyncSentByte = NULL;
					_asyncReceivedByte = NULL;
				}
			}

			break;
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void Communications::constructor()
{
	// Always explicitly call the base's constructor 
	Base::constructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void Communications::destructor()
{
	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
