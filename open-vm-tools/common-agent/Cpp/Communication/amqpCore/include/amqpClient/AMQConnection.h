/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQCONNECTION_H_
#define AMQCONNECTION_H_


#include "Common/CThreadSignal.h"

#include "amqpClient/CAmqpChannel.h"
#include "Common/CManagedThreadPool.h"
#include "amqpClient/AMQChannelManager.h"
#include "amqpClient/CAmqpAuthMechanism.h"
#include "amqpClient/CAmqpConnection.h"
#include "amqpClient/ConnectionWeakReference.h"
#include "amqpClient/ConsumerWorkService.h"
#include "amqpClient/api/Address.h"
#include "amqpClient/api/CertInfo.h"
#include "amqpClient/api/Channel.h"
#include "amqpClient/IConnectionInt.h"
#include "amqpClient/api/Connection.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Concrete class representing and managing an AMQP connection to a broker.
 * <p>
 * To create a broker connection, use a ConnectionFactory created
 * by calling #createConnectionFactory
 */
class AMQConnection : public Connection, public IConnectionInt {
public:
	AMQConnection();
	virtual ~AMQConnection();

public:
	/**
	 * @brief Initialize the connection.
	 * @param username name used to establish connection
	 * @param password for <code><b>username</b></code>
	 * @param address info needed to establish connection
	 * @param certInfo info needed to establish a secure connection
	 * @param requestedFrameMax max size of frame
	 * @param requestedChannelMax max number of channels
	 * @param requestedHeartbeat hearbeat in seconds
	 * @param connectionTimeout connection timeout in milliseconds
	 * @param consumerThreadCount number of consumer threads
	 */
	void init(
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
			const uint16 secondsToWait);

	/**
	 * @brief Start up the connection.
	 */
	void start();


public: // Connection
	SmartPtrChannel createChannel();

	void closeChannel(const SmartPtrChannel& channel);

	void close();

	bool isOpen();

public: // IConnectionInt
	AMQPStatus amqpConnectionOpenChannel(SmartPtrCAmqpChannel& channel);

	void notifyChannelClosedByServer(const uint16 channelNumber);

	void channelCloseChannel(Channel *channel);

private:
	void initConnection();

	static void closeChannel(
		const bool isRunning,
		const SmartPtrAMQChannelManager channelManager,
		const SmartPtrChannel& channel);

	static SmartPtrChannel createChannel(
		const bool isRunning,
		const SmartPtrAMQChannelManager& channelManager,
		const SmartPtrConnectionWeakReference& weakReferenceSelf);

	static void* threadFunc(void* context);

	void threadWorker();

private:
	bool _isInitialized;
	volatile bool _isRunning;
	volatile bool _shouldShutdown;
	bool _wasCloseCalled;
	GThread* _thread;
	CThreadSignal _connectionSignal;
	SmartPtrAddress _address;
	SmartPtrCertInfo _certInfo;
	uint32 _connectionTimeout;
	SmartPtrCAmqpConnection _connectionHandle;
	uint32 _requestedFrameMax;
	uint32 _requestedChannelMax;
	uint32 _requestedHeartbeat;
	uint16 _retries;
	uint16 _secondsToWait;
	SmartPtrCAmqpAuthMechanism _authMechanism;
	SmartPtrAMQChannelManager _channelManager;
	SmartPtrConnectionWeakReference _weakReferenceSelf;
	SmartPtrCManagedThreadPool _threadPool;
	SmartPtrConsumerWorkService _workService;

	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_CREATE_LOG;
	CAF_THREADSIGNAL_CREATE;
	CAF_CM_DECLARE_NOCOPY(AMQConnection);
};

CAF_DECLARE_SMART_POINTER(AMQConnection);

}}

#endif /* AMQCONNECTION_H_ */
