/*
 *  Created on: May 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CACHINGCONNECTIONFACTORY_H_
#define CACHINGCONNECTIONFACTORY_H_

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Common/CAutoRecMutex.h"
#include "Exception/CCafException.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/AmqpMethods.h"
#include "amqpClient/api/Channel.h"
#include "amqpClient/api/Consumer.h"
#include "amqpClient/api/GetResponse.h"
#include "amqpClient/api/ReturnListener.h"
#include "amqpClient/api/amqpClient.h"
#include "amqpCore/ChannelProxy.h"
#include "amqpClient/api/Connection.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpCore/ConnectionListener.h"
#include "amqpCore/AbstractConnectionFactory.h"
#include "amqpCore/ConnectionProxy.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Implementation of the CachingConnectionFactory Integration Object
 */
class AMQPINTEGRATIONCORE_LINKAGE CachingConnectionFactory :
	public AbstractConnectionFactory {
public:
	CachingConnectionFactory();
	virtual ~CachingConnectionFactory();

	void init();

	void init(const std::string& protocol, const std::string& host, const uint32 port);

	void init(const std::string& protocol, const std::string& host);

	void init(const uint32 port);

	void init(const AmqpClient::SmartPtrConnectionFactory& amqpConnectionFactory);

	void destroy();

	uint32 getChannelCacheSize();

	AmqpClient::SmartPtrChannel getChannel();

	void setConnectionListeners(const std::deque<SmartPtrConnectionListener>& listeners);

	void setChannelCacheSize(uint32 cacheSize);

public: // ConnectionFactory
	SmartPtrConnection createConnection();

	void addConnectionListener(const SmartPtrConnectionListener& listener);

private:
	typedef std::deque<SmartPtrChannelProxy> ProxyDeque;
	CAF_DECLARE_SMART_POINTER(ProxyDeque);

	void reset();

	SmartPtrChannelProxy newCachedChannelProxy();

	AmqpClient::SmartPtrChannel createBareChannel();

private:
	class ChannelCachingConnectionProxy : public ConnectionProxy {
	public:
		ChannelCachingConnectionProxy();
		virtual ~ChannelCachingConnectionProxy();

	public:
		void init(
				SmartPtrConnection connection,
				CachingConnectionFactory *parent);

		void destroy();

	public: // ConnectionProxy
		SmartPtrConnection getTargetConnection();

	public: // Connection
		AmqpClient::SmartPtrChannel createChannel();
		void close();
		bool isOpen();

	public:
		AmqpClient::SmartPtrChannel createBareChannel();

	private:
		SmartPtrConnection _target;
		CachingConnectionFactory *_parent;

		CAF_CM_CREATE;
		CAF_CM_CREATE_LOG;
		CAF_CM_DECLARE_NOCOPY(ChannelCachingConnectionProxy);
	};
	CAF_DECLARE_SMART_POINTER(ChannelCachingConnectionProxy);
	friend class ChannelCachingConnectionProxy;

	class CachedChannelHandler : public ChannelProxy {
	public:
		CachedChannelHandler();
		virtual ~CachedChannelHandler();

		void init(
				CachingConnectionFactory *parent,
				AmqpClient::SmartPtrChannel channel);

	private:
		void logicalClose();

		void physicalClose();

		void checkChannel();

		void postProcessCall(SmartPtrCCafException exception);

	public: // ChannelProxy
		AmqpClient::SmartPtrChannel getTargetChannel();

	public: // Channel
		uint16 getChannelNumber();

		void close();

		bool isOpen();

		void basicAck(
			const uint64 deliveryTag,
			const bool ackMultiple);

		AmqpClient::SmartPtrGetResponse basicGet(
			const std::string& queue,
			const bool noAck);

		void basicPublish(
			const std::string& exchange,
			const std::string& routingKey,
			const AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties& properties,
			const SmartPtrCDynamicByteArray& body);

		void basicPublish(
			const std::string& exchange,
			const std::string& routingKey,
			const bool mandatory,
			const bool immediate,
			const AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties& properties,
			const SmartPtrCDynamicByteArray& body);

		AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk basicConsume(
				const std::string& queue,
				const AmqpClient::SmartPtrConsumer& consumer);

		AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk basicConsume(
				const std::string& queue,
				const bool noAck,
				const AmqpClient::SmartPtrConsumer& consumer);

		AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk basicConsume(
				const std::string& queue,
				const std::string& consumerTag,
				const bool noAck,
				const bool noLocal,
				const bool exclusive,
				const AmqpClient::SmartPtrConsumer& consumer,
				const AmqpClient::SmartPtrTable& arguments = AmqpClient::SmartPtrTable());

		AmqpClient::AmqpMethods::Basic::SmartPtrCancelOk basicCancel(
				const std::string& consumerTag);

		AmqpClient::AmqpMethods::Basic::SmartPtrRecoverOk basicRecover(
				const bool requeue);

		AmqpClient::AmqpMethods::Basic::SmartPtrQosOk basicQos(
				const uint32 prefetchSize,
				const uint32 prefetchCount,
				const bool global);

		void basicReject(
				const uint64 deliveryTag,
				const bool requeue);

		AmqpClient::AmqpMethods::Exchange::SmartPtrDeclareOk exchangeDeclare(
			const std::string& exchange,
			const std::string& type,
			const bool durable = false,
			const AmqpClient::SmartPtrTable& arguments = AmqpClient::SmartPtrTable());

		AmqpClient::AmqpMethods::Exchange::SmartPtrDeleteOk exchangeDelete(
			const std::string& exchange,
			const bool ifUnused);

		AmqpClient::AmqpMethods::Queue::SmartPtrDeclareOk queueDeclare();

		AmqpClient::AmqpMethods::Queue::SmartPtrDeclareOk queueDeclare(
			const std::string& queue,
			const bool durable,
			const bool exclusive,
			const bool autoDelete,
			const AmqpClient::SmartPtrTable& arguments = AmqpClient::SmartPtrTable());

		AmqpClient::AmqpMethods::Queue::SmartPtrDeclareOk queueDeclarePassive(
			const std::string& queue);

		AmqpClient::AmqpMethods::Queue::SmartPtrDeleteOk queueDelete(
			const std::string& queue,
			const bool ifUnused,
			const bool ifEmpty);

		AmqpClient::AmqpMethods::Queue::SmartPtrPurgeOk queuePurge(
			const std::string& queue);

		AmqpClient::AmqpMethods::Queue::SmartPtrBindOk queueBind(
			const std::string& queue,
			const std::string& exchange,
			const std::string& routingKey,
			const AmqpClient::SmartPtrTable& arguments = AmqpClient::SmartPtrTable());

		AmqpClient::AmqpMethods::Queue::SmartPtrUnbindOk queueUnbind(
			const std::string& queue,
			const std::string& exchange,
			const std::string& routingKey,
			const AmqpClient::SmartPtrTable& arguments = AmqpClient::SmartPtrTable());

		void addReturnListener(
				const AmqpClient::SmartPtrReturnListener& listener);

		bool removeReturnListener(
				const AmqpClient::SmartPtrReturnListener& listener);

	private:
		CachingConnectionFactory *_parent;
		AmqpClient::SmartPtrChannel _channel;
		CAF_CM_CREATE;
		CAF_CM_CREATE_LOG;
		CAF_CM_CREATE_THREADSAFE;
		CAF_CM_DECLARE_NOCOPY(CachedChannelHandler);
	};
	CAF_DECLARE_SMART_POINTER(CachedChannelHandler);
	friend class CachedChannelHandler;

private:
	bool _isInitialized;
	bool _isActive;
	SmartPtrCAutoRecMutex _connectionMonitor;
	SmartPtrChannelCachingConnectionProxy _connection;
	uint32 _channelCacheSize;
	SmartPtrProxyDeque _cachedChannels;
	SmartPtrCAutoRecMutex _cachedChannelsMonitor;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CachingConnectionFactory);
};
CAF_DECLARE_SMART_POINTER(CachingConnectionFactory);

}}

#endif /* CACHINGCONNECTIONFACTORY_H_ */
