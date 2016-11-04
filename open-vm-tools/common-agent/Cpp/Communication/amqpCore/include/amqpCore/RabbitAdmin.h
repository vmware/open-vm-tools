/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_RABBITADMIN_H_
#define AMQPINTEGRATIONCORE_RABBITADMIN_H_

#include "amqpClient/api/Channel.h"
#include "amqpCore/Binding.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpCore/Exchange.h"
#include "amqpCore/Queue.h"
#include "amqpCore/RabbitTemplate.h"
#include "amqpCore/AmqpAdmin.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Implementation of the RabbitAdmin Integration Object
 */
class AMQPINTEGRATIONCORE_LINKAGE RabbitAdmin : public AmqpAdmin {
public:
	RabbitAdmin();
	virtual ~RabbitAdmin();

	void init(SmartPtrConnectionFactory connectionFactory);

	void term();

public: // AmqpAdmin
	void declareExchange(SmartPtrExchange exchange);

	bool deleteExchange(const std::string& exchange);

	SmartPtrQueue declareQueue();

	void declareQueue(SmartPtrQueue queue);

	bool deleteQueue(const std::string& queue);

	void deleteQueue(
			const std::string& queue,
			const bool unused,
			const bool empty);

	void purgeQueue(const std::string& queue);

	void declareBinding(SmartPtrBinding binding);

	void removeBinding(SmartPtrBinding binding);

private: // Executors
	class DeclareExchangeExecutor : public AmqpTemplate::Executor {
	public:
		gpointer execute(AmqpClient::SmartPtrChannel channel, gpointer data);
	};
	CAF_DECLARE_SMART_POINTER(DeclareExchangeExecutor);

	class DeleteExchangeExecutor : public AmqpTemplate::Executor {
		gpointer execute(AmqpClient::SmartPtrChannel channel, gpointer data);
	};
	CAF_DECLARE_SMART_POINTER(DeleteExchangeExecutor);

	class DeclareQueueExecutor : public AmqpTemplate::Executor {
		gpointer execute(AmqpClient::SmartPtrChannel channel, gpointer data);
	};
	CAF_DECLARE_SMART_POINTER(DeclareQueueExecutor);

	class DeleteQueueExecutor : public AmqpTemplate::Executor {
		gpointer execute(AmqpClient::SmartPtrChannel channel, gpointer data);
	};
	CAF_DECLARE_SMART_POINTER(DeleteQueueExecutor);

	class DeleteQueueExExecutor : public AmqpTemplate::Executor {
		gpointer execute(AmqpClient::SmartPtrChannel channel, gpointer data);
	};
	CAF_DECLARE_SMART_POINTER(DeleteQueueExExecutor);

	class PurgeQueueExecutor : public AmqpTemplate::Executor {
		gpointer execute(AmqpClient::SmartPtrChannel channel, gpointer data);
	};
	CAF_DECLARE_SMART_POINTER(PurgeQueueExecutor);

	class DeclareBindingExecutor : public AmqpTemplate::Executor {
		gpointer execute(AmqpClient::SmartPtrChannel channel, gpointer data);
	};
	CAF_DECLARE_SMART_POINTER(DeclareBindingExecutor);

	class RemoveBindingExecutor : public AmqpTemplate::Executor {
		gpointer execute(AmqpClient::SmartPtrChannel channel, gpointer data);
	};
	CAF_DECLARE_SMART_POINTER(RemoveBindingExecutor);

private:
	bool _isInitialized;
	SmartPtrRabbitTemplate _rabbitTemplate;
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(RabbitAdmin);
};

CAF_DECLARE_SMART_POINTER(RabbitAdmin);

}}

#endif
