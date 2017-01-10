/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/AMQChannelManager.h"
#include "amqpClient/CAmqpConnection.h"
#include "amqpClient/ConnectionWeakReference.h"
#include "amqpClient/api/Address.h"
#include "amqpClient/api/CertInfo.h"
#include "amqpClient/api/Channel.h"
#include "amqpClient/AMQConnection.h"
#include "Exception/CCafException.h"
#include "AMQUtil.h"

using namespace Caf::AmqpClient;

AMQConnection::AMQConnection() :
	_isInitialized(false),
	_isRunning(false),
	_shouldShutdown(false),
	_wasCloseCalled(false),
	_thread(NULL),
	_connectionTimeout(0),
	_requestedFrameMax(0),
	_requestedChannelMax(0),
	_requestedHeartbeat(0),
	_retries(0),
	_secondsToWait(0),
	CAF_CM_INIT_LOG("AMQConnection") {
	CAF_CM_FUNCNAME("AMQPConnection()");
	CAF_CM_INIT_THREADSAFE;
	try {
		CAF_THREADSIGNAL_INIT;
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

AMQConnection::~AMQConnection() {
	CAF_CM_FUNCNAME_VALIDATE("~AMQConnection()");
	if (_weakReferenceSelf) {
		_weakReferenceSelf->clearReference();
		_weakReferenceSelf = NULL;
	}

	if (_connectionHandle && !_wasCloseCalled) {
		CAF_CM_LOG_CRIT_VA0(
				"close() has not been called on this connection. "
				"You *** MUST *** call close() on a connection before releasing it.");
	}

	if (_connectionHandle) {
		AmqpConnection::AMQP_ConnectionClose(_connectionHandle);
	}
}

void AMQConnection::init(
		const std::string username,
		const std::string password,
		const SmartPtrAddress& address,
		const SmartPtrCertInfo& certInfo,
		const uint32 requestedFrameMax,
		const uint32 requestedChannelMax,
		const uint32 requestedHeartbeat,
		const uint32 connectionTimeout,
		const uint32 consumerThreadCount,
		const uint16 retries,
		const uint16 secondsToWait) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(username);
	// password is optional (e.g. AMQP Tunnel)
	CAF_CM_VALIDATE_SMARTPTR(address);
	// other parameters do not require validation

	CAF_CM_LOG_DEBUG_VA1(
			"Creating AuthPlain credential for user %s",
			username.c_str());
	AMQPStatus status = AmqpAuthPlain::AMQP_AuthPlainCreateClient(
			_authMechanism,
			username,
			password);
	AMQUtil::checkAmqpStatus(status, "AmqpAuthPlain::AMQP_AuthPlainCreateClient");

	_address = address;
	_certInfo = certInfo;
	_requestedFrameMax = requestedFrameMax;
	_requestedChannelMax = requestedChannelMax;
	_requestedHeartbeat = requestedHeartbeat;
	_retries = retries;
	_secondsToWait = secondsToWait;

	initConnection();
	_connectionTimeout = connectionTimeout;
	_connectionSignal.initialize("connectionSignal");
	_weakReferenceSelf.CreateInstance();
	_weakReferenceSelf->setReference(this);

	_threadPool.CreateInstance();
	_threadPool->init(address->toString(), consumerThreadCount, 100);

	_workService.CreateInstance();
	_workService->init(_threadPool);
	_isInitialized = true;
}

void AMQConnection::start() {
	CAF_CM_FUNCNAME("start");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_BOOL(!_isRunning);

	bool rc = false;
	GThread* thread = _thread;
	uint32 connectionTimeout = _connectionTimeout;
	{
		CAF_CM_UNLOCK_LOCK;
		CAF_THREADSIGNAL_LOCK_UNLOCK;
		thread = CThreadUtils::startJoinable(threadFunc, this);
		rc = _connectionSignal.waitOrTimeout(CAF_THREADSIGNAL_MUTEX, connectionTimeout);
	}
	_thread = thread;

	if (rc) {
		try {
			uint16 channelMax = 0;
			AMQUtil::checkAmqpStatus(
					AmqpConnection::AMQP_ConnectionGetMaxChannels(_connectionHandle, &channelMax),
					"AmqpConnection::AMQP_ConnectionGetMaxChannels");
			uint32 frameMax = 0;
			AMQUtil::checkAmqpStatus(
					AmqpConnection::AMQP_ConnectionGetMaxFrameSize(_connectionHandle, &frameMax),
					"AmqpConnection::AMQP_ConnectionGetMaxFrameSize");
			uint16 heartbeat = 0;
			AMQUtil::checkAmqpStatus(
					AmqpConnection::AMQP_ConnectionGetHeartbeatInterval(_connectionHandle, &heartbeat),
					"AMQUtil::checkAmqpStatus");
			CAF_CM_LOG_DEBUG_VA3(
					"Tuned connection [chMax=%d][frameMax=%d][hbeat=%d]",
					channelMax,
					frameMax,
					heartbeat);
			_channelManager.CreateInstance();
			_channelManager->init(_workService);
		}
		CAF_CM_CATCH_ALL
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_THROWEXCEPTION;
	} else {
		CAF_CM_LOG_DEBUG_VA1("Need to shutdown due to start issue: %d", rc);
		_shouldShutdown = true;

		SmartPtrCCafException threadException;
		GThread* thread = _thread;
		{
			CAF_CM_UNLOCK_LOCK;
			threadException =
					static_cast<CCafException*>(g_thread_join(thread));
		}

		_thread = NULL;

		if (threadException) {
			threadException->throwAddRefedSelf();
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					AmqpExceptions::AmqpTimeoutException,
					0,
					"Timed out trying to connect to %s",
					_address->toString().c_str());
		}
	}
}

SmartPtrChannel AMQConnection::createChannel() {
	CAF_CM_FUNCNAME_VALIDATE("createChannel");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	bool isRunning;
	SmartPtrAMQChannelManager channelManager;
	SmartPtrConnectionWeakReference weakReferenceSelf;
	{
		CAF_CM_LOCK_UNLOCK;
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		isRunning = _isRunning;
		channelManager = _channelManager;
		weakReferenceSelf = _weakReferenceSelf;
	}

	return createChannel(isRunning, channelManager, weakReferenceSelf);
}

void AMQConnection::closeChannel(const SmartPtrChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("closeChannel");
	CAF_CM_VALIDATE_SMARTPTR(channel);

	bool isRunning;
	SmartPtrAMQChannelManager channelManager;
	{
		CAF_CM_LOCK_UNLOCK;
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		isRunning = _isRunning;
		channelManager = _channelManager;
	}

	closeChannel(isRunning, channelManager, channel);
}

void AMQConnection::close() {
	CAF_CM_FUNCNAME_VALIDATE("close");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_wasCloseCalled = true;
	if (_isRunning) {
		CAF_CM_LOG_DEBUG_VA0("Need to shutdown becaue the connection is closing");
		_shouldShutdown = true;
		_weakReferenceSelf->clearReference();
		_weakReferenceSelf = NULL;

		_workService->notifyConnectionClosed();

		if (NULL != _thread) {
			GThread* thread = _thread;
			{
				CAF_CM_UNLOCK_LOCK;
				g_thread_join(thread);
			}

			_thread = NULL;
		}

		{
			CAF_CM_UNLOCK_LOCK;
			while (_channelManager->getOpenChannelCount()) {
				CThreadUtils::sleep(100);
			}
		}
	}
}

bool AMQConnection::isOpen() {
	CAF_CM_FUNCNAME_VALIDATE("isOpen");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return (_isRunning && !_shouldShutdown);
}

AMQPStatus AMQConnection::amqpConnectionOpenChannel(SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("amqpConnectionOpenChannel");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_LOG_DEBUG_VA0("calling AmqpConnection::AMQP_ConnectionOpenChannel");
	AMQPStatus status = AmqpConnection::AMQP_ConnectionOpenChannel(_connectionHandle, channel);
	if (status == AMQP_ERROR_OK) {
		uint16 id = 0;
		AmqpChannel::AMQP_ChannelGetId(channel, &id);
		CAF_CM_LOG_DEBUG_VA1("created channel #%d", id);
	} else {
		CAF_CM_LOG_DEBUG_VA1("failed to create channel. status=%d", status);
	}
	return status;
}

void AMQConnection::notifyChannelClosedByServer(const uint16 channelNumber) {
	CAF_CM_LOCK_UNLOCK;
	_channelManager->removeChannel(channelNumber);
}

void AMQConnection::channelCloseChannel(Channel *channel) {
	CAF_CM_FUNCNAME_VALIDATE("channelCloseChannel");
	CAF_CM_VALIDATE_PTR(channel);
	closeChannel(channel);
}

void AMQConnection::initConnection() {
	CAF_CM_FUNCNAME_VALIDATE("initConnection");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_LOG_DEBUG_VA3(
			"Creating connection "
			"[reqChMax=%d][reqFrameMax=%d][reqHB=%d]",
			_requestedChannelMax,
			_requestedFrameMax,
			_requestedHeartbeat);
	AMQPStatus status = AmqpConnection::AMQP_ConnectionCreate(
			_connectionHandle,
			_address,
			_authMechanism,
			_certInfo,
			_requestedChannelMax,
			_requestedFrameMax,
			_requestedHeartbeat,
			_retries,
			_secondsToWait);
	AMQUtil::checkAmqpStatus(status, "AmqpConnection::AMQP_ConnectionCreate");
}

void AMQConnection::closeChannel(
	const bool isRunning,
	const SmartPtrAMQChannelManager channelManager,
	const SmartPtrChannel& channel) {
	CAF_CM_STATIC_FUNC("AMQConnection", "closeChannel");
	CAF_CM_VALIDATE_SMARTPTR(channelManager);
	CAF_CM_VALIDATE_SMARTPTR(channel);

	if (isRunning) {
		if (channel->isOpen()) {
			const uint16 channelNumber = channel->getChannelNumber();
			SmartPtrCCafException reason;
			try {
				AmqpExceptions::SmartPtrChannelClosedByUserException exception;
				exception.CreateInstance();
				reason = exception.GetNonAddRefedInterface();
				reason->populate(
						"Closed by user",
						0,
						_cm_className_,
						_cm_funcName_);

				channelManager->closeChannel(channelNumber, reason);
			}
			CAF_CM_CATCH_ALL;
		}
	} else {
		CAF_CM_EXCEPTIONEX_VA0(
				IllegalStateException,
				0,
				"The connection is closed");
	}
}

SmartPtrChannel AMQConnection::createChannel(
	const bool isRunning,
	const SmartPtrAMQChannelManager& channelManager,
	const SmartPtrConnectionWeakReference& weakReferenceSelf) {
	CAF_CM_STATIC_FUNC("AMQConnection", "createChannel");

	if (!isRunning) {
		CAF_CM_EXCEPTIONEX_VA0(
				AmqpExceptions::ConnectionClosedException,
				0,
				"The connection is closed");
	}

	return channelManager->createChannel(weakReferenceSelf);
}

void* AMQConnection::threadFunc(void* context) {
	CAF_CM_STATIC_FUNC("AMQConnection", "threadFunc");
	try {
		CAF_CM_VALIDATE_PTR(context);
		SmartPtrAMQConnection self = (AMQConnection*)context;
		self->threadWorker();
	}
	CAF_CM_CATCH_ALL;

	return CAF_CM_GETEXCEPTION;
}

void AMQConnection::threadWorker() {
	CAF_CM_FUNCNAME("threadWorker");
	CAF_CM_LOCK_UNLOCK;

	try {
		CAF_CM_LOG_DEBUG_VA1(
				"Connecting to %s",
				_address->toString().c_str());

		AMQPStatus status = AMQP_ERROR_OK;
		SmartPtrCAmqpConnection connectionHandle = _connectionHandle;
		{
			CAF_CM_UNLOCK_LOCK;
			status = AmqpConnection::AMQP_ConnectionConnect(
					connectionHandle,
					AMQP_CONNECTION_FLAG_CLOSE_SOCKET);
		}
		AMQUtil::checkAmqpStatus(status, "AmqpConnection::AMQP_ConnectionConnect");

		AMQPConnectionState state;
		status = AmqpConnection::AMQP_ConnectionGetState(
				_connectionHandle,
				&state);
		AMQUtil::checkAmqpStatus(status, "AmqpConnection::AMQP_ConnectionGetState");

		while (!_shouldShutdown && (AMQP_STATE_CONNECTED != state)) {
			status = AmqpConnection::AMQP_ConnectionProcessIO(_connectionHandle);
			switch (status) {
			case AMQP_ERROR_OK:
				break;

			case AMQP_ERROR_IO_ERROR:
				{
					const char *err = NULL;
					AmqpConnection::AMQP_ConnectionGetLastError(_connectionHandle, &err);
					CAF_CM_LOG_WARN_VA2(
							"Connection to [%s] failed: %s",
							_address->toString().c_str(),
							err);
					_connectionHandle = AMQP_HANDLE_INVALID;
					CThreadUtils::sleep(250);
					initConnection();
					AMQPStatus status = AmqpConnection::AMQP_ConnectionConnect(
							_connectionHandle,
							AMQP_CONNECTION_FLAG_CLOSE_SOCKET);
					AMQUtil::checkAmqpStatus(status, "AmqpConnection::AMQP_ConnectionConnect");
				}
				break;

			default:
				{
					const char *err = NULL;
					AmqpConnection::AMQP_ConnectionGetLastError(_connectionHandle, &err);
					AMQUtil::checkAmqpStatus(status, err);
				}
				break;
			}

			status = AmqpConnection::AMQP_ConnectionGetState(
					_connectionHandle,
					&state);
			AMQUtil::checkAmqpStatus(
					status,
					"AmqpConnection::AMQP_ConnectionGetState");
		}

		if (!_shouldShutdown && (AMQP_STATE_CONNECTED == state)) {
			{
				CAF_THREADSIGNAL_LOCK_UNLOCK;
				_isRunning = true;
				_connectionSignal.signal();
			}

			while (!_shouldShutdown && AMQP_STATE_CONNECTED == state) {
				SmartPtrCAmqpConnection connectionHandle = _connectionHandle;
				{
					CAF_CM_UNLOCK_LOCK;
					status = AmqpConnection::AMQP_ConnectionWaitForIO(connectionHandle, 200);
					status = AmqpConnection::AMQP_ConnectionProcessIO(connectionHandle);
					status = AmqpConnection::AMQP_ConnectionGetState(connectionHandle, &state);
				}
			}

			_isRunning = false;
			CAF_CM_LOG_DEBUG_VA2(
					"IO loop has stopped. "
					"[_shouldShutdown=%d][state=%d]",
					_shouldShutdown,
					state);

			SmartPtrCCafException shutdownException;
			try {
				if (_shouldShutdown) {
					AmqpExceptions::SmartPtrChannelClosedByShutdownException exception;
					exception.CreateInstance();
					shutdownException = exception.GetNonAddRefedInterface();
					shutdownException->populate(
							"Normal application shutdown",
							0,
							_cm_className_,
							_cm_funcName_);
				} else {
					const char* error = NULL;
					AmqpConnection::AMQP_ConnectionGetLastError(_connectionHandle, &error);
					AmqpExceptions::SmartPtrConnectionClosedByIOException exception;
					exception.CreateInstance();
					shutdownException = exception.GetNonAddRefedInterface();
					shutdownException->populate(
							error,
							0,
							_cm_className_,
							_cm_funcName_);
				}
				{
					CAF_CM_UNLOCK_LOCK;
					_channelManager->notifyConnectionClose(shutdownException);
				}
				AmqpConnection::AMQP_ConnectionClose(_connectionHandle);
				_connectionHandle = AMQP_HANDLE_INVALID;
			}
			CAF_CM_CATCH_ALL;
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_THROWEXCEPTION;
}
