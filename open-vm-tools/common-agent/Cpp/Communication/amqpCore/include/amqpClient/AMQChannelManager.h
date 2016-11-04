/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQCHANNELMANAGER_H_
#define AMQCHANNELMANAGER_H_



#include "Exception/CCafException.h"
#include "amqpClient/AMQChannel.h"
#include "amqpClient/ConsumerWorkService.h"
#include "amqpClient/api/Channel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Manages a set of channels for a connection.
 * Channels are indexed by channel number (<code><b>1.._channelMax</b></code>).
 */
class AMQChannelManager {
public:
	AMQChannelManager();
	virtual ~AMQChannelManager();

public:
	/**
	 * @brief Initialize the manager to control <code><b>channelMax</b></code> channels.
	 * @param workService the service (thread pool) to run the channels in
	 */
	void init(const SmartPtrConsumerWorkService& workService);

	/**
	 * @brief Create a new channel
	 * @param connection The controlling #Caf::AmqpClient::IConnectionInt connection
	 * @return the new Channel
	 */
	SmartPtrChannel createChannel(const SmartPtrIConnectionInt& connection);

	/**
	 * @brief Return an existing channel on this connection.
	 * @param channelNumber the number of the required channel
	 * @return the #Caf::AmqpClient::Channel interface to the channel if it exists else
	 * throws an exception
	 */
	SmartPtrChannel getChannel(const uint16 channelNumber);

	/**
	 * @brief Return the number of open channels
	 * @return the number of open channels
	 */
	size_t getOpenChannelCount();

	/**
	 * @brief Notify all channels that the connection is closed and the reason for it
	 * @param shutdownException the exception (reason) for the closure
	 */
	void notifyConnectionClose(SmartPtrCCafException& shutdownException);

	/**
	 * @brief Close achannel with the supplied reason
	 * @param channelNumber the channel to close
	 * @param reason the reason for closure
	 */
	void closeChannel(const uint16 channelNumber, SmartPtrCCafException& reason);

	/**
	 * @brief Remove a channel from management
	 * @param channelNumber channel number
	 */
	void removeChannel(const uint16 channelNumber);

private:
	typedef std::map<uint16, SmartPtrAMQChannel> ChannelMap;

	bool _isInitialized;
	ChannelMap _channelMap;
	SmartPtrConsumerWorkService _workService;

	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(AMQChannelManager);
};
CAF_DECLARE_SMART_POINTER(AMQChannelManager);

}}

#endif /* AMQCHANNELMANAGER_H_ */
