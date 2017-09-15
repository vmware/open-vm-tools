/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/AMQChannel.h"
#include "amqpClient/ConsumerWorkService.h"
#include "amqpClient/api/Channel.h"
#include "amqpClient/AMQChannelManager.h"
#include "Exception/CCafException.h"

using namespace Caf::AmqpClient;

AMQChannelManager::AMQChannelManager() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("AMQChannelManager") {
	CAF_CM_INIT_THREADSAFE;
}

AMQChannelManager::~AMQChannelManager() {
}

void AMQChannelManager::init(const SmartPtrConsumerWorkService& workService) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_PTR(workService);
	_workService = workService;
	_isInitialized = true;
}

SmartPtrChannel AMQChannelManager::createChannel(const SmartPtrIConnectionInt& connection) {
	CAF_CM_FUNCNAME("createChannel");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(connection);
	SmartPtrAMQChannel channel;

	channel.CreateInstance();

	SmartPtrConsumerWorkService workService = _workService;
	{
		CAF_CM_UNLOCK_LOCK;
		channel->init(connection, workService);
	}

	const uint16 channelNumber = channel->getChannelNumber();
	if (_channelMap.insert(ChannelMap::value_type(channelNumber, channel)).second) {

	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				DuplicateElementException,
				0,
				"Channel number %d is already in use. This should never happen. "
				"Please report this as a bug.",
				channelNumber);
	}
	return channel;
}

SmartPtrChannel AMQChannelManager::getChannel(const uint16 channelNumber) {
	CAF_CM_FUNCNAME("getChannel");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	SmartPtrChannel channel;
	ChannelMap::const_iterator channelIter = _channelMap.find(channelNumber);
	if (channelIter != _channelMap.end()) {
		channel = (*channelIter).second;
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"Channel #%d does not exist.",
				channelNumber);
	}
	return channel;
}

size_t AMQChannelManager::getOpenChannelCount() {
	CAF_CM_FUNCNAME_VALIDATE("getOpenChannelCount");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _channelMap.size();
}

void AMQChannelManager::notifyConnectionClose(SmartPtrCCafException& shutdownException) {
	CAF_CM_FUNCNAME("notifyConnectionClose");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	try {
		_workService->notifyConnectionClosed();
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	for (TSmartMapIterator<ChannelMap> channel(_channelMap); channel; channel++) {
		try {
			CAF_CM_UNLOCK_LOCK;
			channel->notifyConnectionClosed(shutdownException);
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}
	_channelMap.clear();
}

void AMQChannelManager::closeChannel(const uint16 channelNumber, SmartPtrCCafException& reason) {
	CAF_CM_FUNCNAME("closeChannel");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ChannelMap::iterator channel = _channelMap.find(channelNumber);
	if (channel != _channelMap.end()) {
		{
			CAF_CM_UNLOCK_LOCK;
			channel->second->close(reason);
		}
		_channelMap.erase(channel);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"Channel #%d is not in the channel manager",
				channelNumber);
	}
}

void AMQChannelManager::removeChannel(const uint16 channelNumber) {
	CAF_CM_FUNCNAME("removeChannel");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ChannelMap::iterator channel = _channelMap.find(channelNumber);
	if (channel != _channelMap.end()) {
		_channelMap.erase(channel);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"Channel #%d is not in the channel manager",
				channelNumber);
	}
}
