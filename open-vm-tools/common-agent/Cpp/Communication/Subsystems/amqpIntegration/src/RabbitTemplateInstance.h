/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef RABBITTEMPLATEINSTANCE_H_
#define RABBITTEMPLATEINSTANCE_H_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpCore/AmqpTemplate.h"
#include "amqpCore/RabbitTemplate.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ILifecycle.h"

namespace Caf { namespace AmqpIntegration {

CAF_DECLARE_CLASS_AND_IMPQI_POINTER(RabbitTemplateInstance);

/**
 * @ingroup IntObj
 * @brief An Integration Object implementing Caf::AmqpIntegration::AmqpTemplate
 * <p>
 * <pre>
 * Example context file declaration:
 *
 * <rabbit-template
 * 	id="amqpTemplate"
 * 	connection-factory="connectionFactory"
 * 	reply_timeout="3000" />
 * </pre>
 * <p>
 * XML attribute definitions:
 * <table border="1">
 * <tr><th>Attribute</th><th>Description</th></tr>
 * <tr><td>id</td><td><b>optional</b> The id of the integration object</td></tr>
 * <tr><td>connection-factory</td>
 * <td><b>required</b> The id of the ConnectionFactory bean</td></tr>
 * <tr><td>exchange</td><td><i>optional</i> The name of the exchange to use by default</td></tr>
 * <tr><td>queue</td>
 * <td><i>optional</i> The id of the queue to use by default. The queue name comes from
 * the queue object with the given id.</td></tr>
 * <tr><td>routing-key</td><td><i>optional</i> The routing-key to use by default</td></tr>
 * <tr><td>reply_timeout</td>
 * <td><i>optional</i> The number of milliseconds to wait for a response when using
 * sendAndReceive methods.  This is an unsigned value.  A value of zero indicates
 * wait indefinitely.</td></tr>
 * </table>
 */
class RabbitTemplateInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ILifecycle,
	public AmqpTemplate
{
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ILifecycle)
		CAF_QI_ENTRY(AmqpTemplate)
	CAF_END_QI()

public:
	RabbitTemplateInstance();
	virtual ~RabbitTemplateInstance();

public: // IIntegrationObject
	void initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection);

	std::string getId() const;

public: // IIntegrationComponentInstance
	void wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver);

public: // ILifecycle
	void start(const uint32 timeoutMs);
	void stop(const uint32 timeoutMs);
	bool isRunning() const;

public: // AmqpTemplate
	void send(
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	void send(
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	void send(
			const std::string& exchange,
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage receive(
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage receive(
			const std::string& queueName,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage sendAndReceive(
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage sendAndReceive(
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage sendAndReceive(
			const std::string& exchange,
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper());

	gpointer execute(SmartPtrExecutor executor, gpointer data);

private:
	bool _isWired;
	std::string _id;
	SmartPtrIDocument _configSection;
	SmartPtrRabbitTemplate _template;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(RabbitTemplateInstance);
};

}}

#endif /* RABBITTEMPLATEINSTANCE_H_ */
