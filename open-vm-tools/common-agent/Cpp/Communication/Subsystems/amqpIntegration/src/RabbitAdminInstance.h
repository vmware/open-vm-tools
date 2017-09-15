/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef RABBITADMININSTANCE_H_
#define RABBITADMININSTANCE_H_


#include "Integration/IIntegrationAppContextAware.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationAppContext.h"
#include "amqpCore/Binding.h"
#include "amqpCore/Exchange.h"
#include "amqpCore/Queue.h"
#include "amqpCore/RabbitAdmin.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ILifecycle.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObj
 * @brief An Integration Object implementing Caf::AmqpIntegration::AmqpAdmin
 * <p>
 * <pre>
 * Example context file declaration:
 *
 * <rabbit-admin
 *    id="amqpAdmin"
 *    connection-factory="connectionFactory" />
 * </pre>
 * <p>
 * XML attribute definitions:
 * <table border="1">
 * <tr><th>Attribute</th><th>Description</th></tr>
 * <tr><td>id</td><td><b>optional</b> The id of the integration object</td></tr>
 * <tr><td>connection-factory</td>
 * <td><b>required</b> The id of the ConnectionFactory bean</td></tr>
 * </table>
 */
class RabbitAdminInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ILifecycle,
	public IIntegrationAppContextAware,
	public AmqpAdmin
{
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ILifecycle)
		CAF_QI_ENTRY(IIntegrationAppContextAware)
		CAF_QI_ENTRY(AmqpAdmin)
	CAF_END_QI()

public:
	RabbitAdminInstance();
	virtual ~RabbitAdminInstance();

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

public: // IIntegrationAppContextAware
	void setIntegrationAppContext(
				SmartPtrIIntegrationAppContext context);

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

private:
	bool _isRunning;
	std::string _id;
	std::string _connectionFactoryId;
	SmartPtrRabbitAdmin _admin;
	SmartPtrIAppContext _appContext;
	SmartPtrIIntegrationAppContext _integrationAppContext;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(RabbitAdminInstance);
};
CAF_DECLARE_SMART_QI_POINTER(RabbitAdminInstance);

}}

#endif /* RABBITADMININSTANCE_H_ */
