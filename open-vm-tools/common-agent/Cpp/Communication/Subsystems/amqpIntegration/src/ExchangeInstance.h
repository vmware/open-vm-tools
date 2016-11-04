/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef EXCHANGEINSTANCE_H_
#define EXCHANGEINSTANCE_H_


#include "Integration/IIntegrationObject.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationAppContext.h"
#include "amqpCore/Binding.h"
#include "amqpCore/Exchange.h"
#include "amqpCore/ExchangeInternal.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObj
 * @brief An Integration Object implementing Caf::AmqpIntegration::Exchange
 * <p>
 * %Exchange to queue bindings are also declared as part of an exchange declaration.
 * <p>
 * Example context file declarations:
 * <p>
 * Direct exchange:
 * <pre>
 * <rabbit-direct-exchange
 *    name="cafResponses">
 *    <rabbit-bindings
 *    	<rabbit-binding
 *    		queue="inboundQueue"
 *    		key="caf.mgmt.response" />
 *    </rabbit-bindings>
 * </rabbit-direct-exchange>
 * </pre>
 * Topic exchange:
 * <pre>
 * <rabbit-topic-exchange
 *    name="cafEvents"
 *    durable="false">
 *    <rabbit-bindings
 *    	<rabbit-binding
 *    		queue="inboundEventQ"
 *    		key="caf.mgmt.event.*" />
 *    </rabbit-bindings>
 * </rabbit-topic-exchange>
 * </pre>
 * Other exchange types are <i>rabbit-headers-exchange</i> and
 * <i>rabbit-fanout-exchange</i>.
 * <p>
 * %Exchange XML attribute definitions:
 * <table border="1">
 * <tr><th>Attribute</th><th>Description</th></tr>
 * <tr><td>name</td>
 * <td><b>required</b> The AMQP name of the exchange.  This is the name that
 * will be sent in the Exchange.Declare AMQP method.</td></tr>
 * <tr><td>durable</td>
 * <td><i>optional</i> <i>true</i> to declare a durable exchange else <i>false</i>.</td></tr>
 * </table>
 * %Binding XML attribute definitions:
 * <table border="1">
 * <tr><th>Attribute</th><th>Description</th></tr>
 * <tr><td>queue</td>
 * <td><b>required</b> The <i>id</i> of the queue integration object to bind to the exchange.</td></tr>
 * <tr><td>key</td>
 * <td><b>required</b> The routing key for the binding.
 * The format is specific to the type of exchange.</td></tr>
 * </table>
 */
class ExchangeInstance :
	public IIntegrationObject,
	public ExchangeInternal,
	public Exchange
{
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(ExchangeInternal)
		CAF_QI_ENTRY(Exchange)
	CAF_END_QI()

public:
	ExchangeInstance();
	virtual ~ExchangeInstance();

public: // IIntegrationObject
	void initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection);

	std::string getId() const;

public: // Exchange
	std::string getName() const;
	std::string getType() const;
	bool isDurable() const;

public: // ExchangeInternal
	std::deque<SmartPtrBinding> getEmbeddedBindings() const;

private:
	std::string _id;
	SmartPtrIIntegrationAppContext _integrationAppContext;
	SmartPtrExchange _exchange;
	std::deque<SmartPtrBinding> _bindings;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(ExchangeInstance);
};
CAF_DECLARE_SMART_QI_POINTER(ExchangeInstance);

}}

#endif /* EXCHANGEINSTANCE_H_ */
