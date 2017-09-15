/*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_AMQPMETHODS_H_
#define AMQPCLIENTAPI_AMQPMETHODS_H_

#include "amqpClient/api/Method.h"

namespace Caf { namespace AmqpClient { namespace AmqpMethods {

#if (1) // basic
/**
 * @brief AMQP Basic methods
 * @ingroup AmqpApi
 */
namespace Basic {

/**
 * @ingroup AmqpApi
 * @brief Interface representing the basic.get-ok method parameters
 */
struct __declspec(novtable) GetOk : public Method {
	CAF_DECL_UUID("A3CD4488-B600-4AC6-9513-03021AC06345")

	/** @return the delivery tag used for basic.ack calls */
	virtual uint64 getDeliveryTag() = 0;

	/**
	 * @retval true if the message was redelivered
	 * @retval false if the message has not been redlivered
	 */
	virtual bool getRedelivered() = 0;

	/** @return the name of the exchange supplying the message */
	virtual std::string getExchange() = 0;

	/** @return the message's routing key */
	virtual std::string getRoutingKey() = 0;

	/** @return the number of messages remaining in the queue */
	virtual uint32 getMessageCount() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(GetOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the basic.get-empty method parameters
 */
struct __declspec(novtable) GetEmpty : public Method {
	CAF_DECL_UUID("6bcd3e9e-e2b1-4824-b455-acad073737c5")
};
CAF_DECLARE_SMART_INTERFACE_POINTER(GetEmpty);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the basic.consume-ok method parameters
 */
struct __declspec(novtable) ConsumeOk : public Method {
	CAF_DECL_UUID("29E385DA-37FB-48E4-9F6D-463555C9DDDC")

	virtual std::string getConsumerTag() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ConsumeOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the basic.deliver method parameters
 */
struct __declspec(novtable) Deliver : public Method {
	CAF_DECL_UUID("2AD74C5E-CC9D-4A6C-9738-DA836BC25FCA")

	/** @return the <i>consumer tag</i> associated with the consumer */
	virtual std::string getConsumerTag() = 0;

	/** @return the delivery tag used for basic.ack calls */
	virtual uint64 getDeliveryTag() = 0;

	/**
	 * @retval true if the message was redelivered
	 * @retval false if the message has not been redlivered
	 */
	virtual bool getRedelivered() = 0;

	/** @return the name of the exchange supplying the message */
	virtual std::string getExchange() = 0;

	/** @return the message's routing key */
	virtual std::string getRoutingKey() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Deliver);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the basic.consume-ok method parameters
 */
struct __declspec(novtable) CancelOk : public Method {
	CAF_DECL_UUID("759CEE2C-FDA5-4A2A-BFE0-617A879D05BF")

	/** @return the <i>consumer tag</i> associated with the consumer */
	virtual std::string getConsumerTag() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(CancelOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the basic.return method parameters
 */
struct __declspec(novtable) Return : public Method {
	CAF_DECL_UUID("000A440E-AEE4-418E-B9A0-9857F5C20283")

	/** @return the reply code */
	virtual uint16 getReplyCode() = 0;

	/** @return the reply text */
	virtual std::string getReplyText() = 0;

	/** @return the exchnage name */
	virtual std::string getExchange() = 0;

	/** @return the routing key */
	virtual std::string getRoutingKey() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Return);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the basic.recover-ok method parameters
 */
struct __declspec(novtable) RecoverOk : public Method {
	CAF_DECL_UUID("DF71DC22-B65C-44FC-A0F4-EFAC181E2F69")
};
CAF_DECLARE_SMART_INTERFACE_POINTER(RecoverOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the basic.qos-ok method
 */
struct __declspec(novtable) QosOk : public Method {
	CAF_DECL_UUID("D5710B1C-DE05-42F5-9695-95364C1D9468")
};
CAF_DECLARE_SMART_INTERFACE_POINTER(QosOk);

} // namespace Basic
#endif

#if (1) // channel
/**
 * @ingroup AmqpApi
 * @brief AMQP Channel methods
 */
namespace Channel {

/**
 * @ingroup AmqpApi
 * @brief Interface representing the channel.open-ok method parameters
 */
struct __declspec(novtable) OpenOk : public Method {
	CAF_DECL_UUID("4c027f40-db11-4a72-ac2e-cc8da89035cb")
};
CAF_DECLARE_SMART_INTERFACE_POINTER(OpenOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the channel.close method parameters
 */
struct __declspec(novtable) Close : public Method {
	CAF_DECL_UUID("45d0c75f-ff48-4552-9a83-498efa5f6ad2")

	/** @return the reply code */
	virtual uint16 getReplyCode() = 0;

	/** @return the reply text */
	virtual std::string getReplyText() = 0;

	/** @return the class id of the method that caused the close */
	virtual uint16 getClassId() = 0;

	/** @return the method id of the method that caused the close */
	virtual uint16 getMethodId() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Close);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the channel.close-ok method parameters
 */
struct __declspec(novtable) CloseOk : public Method {
	CAF_DECL_UUID("DAF11BD3-06B6-4FA4-AC80-2B0959D2297D")
};
CAF_DECLARE_SMART_INTERFACE_POINTER(CloseOk);

} // namespace Channel
#endif

#if (1) // exchange
/**
 * @ingroup AmqpApi
 * @brief AMQP Exchange methods
 */
namespace Exchange {

/**
 * @ingroup AmqpApi
 * @brief Interface representing the exchange.declare-ok method parameters
 */
struct __declspec(novtable) DeclareOk : public Method {
	CAF_DECL_UUID("e54d9fff-7905-4e18-b1e8-090279a5cffe")
};
CAF_DECLARE_SMART_INTERFACE_POINTER(DeclareOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the exchange.delete-ok method parameters
 */
struct __declspec(novtable) DeleteOk : public Method {
	CAF_DECL_UUID("9f13d0fb-1bdd-473a-873a-58e948bc256c")
};
CAF_DECLARE_SMART_INTERFACE_POINTER(DeleteOk);

} // namespace Exchange
#endif

#if (1) // queue
/**
 * @ingroup AmqpApi
 * @brief AMQP Queue methods
 */
namespace Queue {

/**
 * @ingroup AmqpApi
 * @brief Interface representing the queue.declare-ok method parameters
 */
struct __declspec(novtable) DeclareOk : public Method {
	CAF_DECL_UUID("EB96E48E-DF40-4D5F-A41F-7F4EBEBE2BE1")

	/** @return the name of the queue */
	virtual std::string getQueueName() = 0;

	/** @return the number of messages in the queue */
	virtual uint32 getMessageCount() = 0;

	/** @return the number of active consumers for the queue */
	virtual uint32 getConsumerCount() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(DeclareOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the queue.delete-ok method parameters
 */
struct __declspec(novtable) DeleteOk : public Method {
	CAF_DECL_UUID("34f4b342-7ab0-44d5-b007-4eec141a4435")

	/** @return the number of messages deleted */
	virtual uint32 getMessageCount() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(DeleteOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the queue.delete-ok method parameters
 */
struct __declspec(novtable) PurgeOk : public Method {
	CAF_DECL_UUID("63bcf694-5ac3-4067-8134-659133986099")

	/** @return the number of messages deleted */
	virtual uint32 getMessageCount() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(PurgeOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the queue.bind-ok method parameters
 */
struct __declspec(novtable) BindOk : public Method {
	CAF_DECL_UUID("1a60c168-24d1-4184-a5ec-fcf9fca70994")
};
CAF_DECLARE_SMART_INTERFACE_POINTER(BindOk);

/**
 * @ingroup AmqpApi
 * @brief Interface representing the queue.unbind-ok method parameters
 */
struct __declspec(novtable) UnbindOk : public Method {
	CAF_DECL_UUID("edf78de1-eee4-44c0-9051-e4f6ee80c0a2")
};
CAF_DECLARE_SMART_INTERFACE_POINTER(UnbindOk);

} // namespace Queue
#endif

}}}

#endif
