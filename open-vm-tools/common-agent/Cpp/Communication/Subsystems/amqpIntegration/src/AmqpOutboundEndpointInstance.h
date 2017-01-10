/*
 *  Created on: Jul 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPOUTBOUNDENDPOINTINSTANCE_H_
#define AMQPOUTBOUNDENDPOINTINSTANCE_H_


#include "Integration/IIntegrationAppContextAware.h"

#include "Common/IAppContext.h"
#include "Integration/Core/CMessagingTemplate.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationAppContext.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ILifecycle.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObj
 * @brief An Integration Object implementing Caf::AmqpIntegration::AmqpOutboundEndpoint
 * <p>
 * <pre>
 * Example context file declaration:
 *
 * <rabbit-outbound-channel-adapter
 *		exchange-name="test.direct"
 *		routing-key="test.direct"
 *		channel="testChannel" />
 *
 * <rabbit-outbound-channel-adapter
 *		exchange-name="${var:exchangeName}"
 *		routing-key="${env:ROUTING_KEY}"
 *		channel="testChannel" />
 *
 * <rabbit-outbound-channel-adapter
 *		exchange-name-expression="@headerExprInvoker.toString('exchangeName')"
 *		routing-key-expression="@headerExprInvoker.toString('routingKey')"
 *		mapped-request-headers="^myApp[.].*"
 *		channel="testChannel" />
 *	</pre>
 * <p>
 * XML attribute definitions:
 * <table border="1">
 * <tr><th>Attribute</th><th>Description</th></tr>
 * <tr><td>channel</td><td><b>required</b> The channel supplying messages to be sent.</td></tr>
 * <tr><td>exchange-name</td><td>The name of the exchange to publish to.
 * Either <i>exchange-name</i> or <i>exchange-name-expression</i> must be specified.</td></tr>
 * <tr><td>exchange-name-expression</td>
 * <td>The name of the exchange to publish to resolved by calling an IExpressionInvoker object.
 * Either <i>exchange-name</i> or <i>exchange-name-expression</i> must be specified.</td></tr>
 * <tr><td>routing-key</td><td>The routing key for the message.
 * Either <i>routing-key</i> or <i>routing-key-expression</i> must be specified.</td></tr>
 * <tr><td>routing-key-expression</td>
 * <td>The routing key for the message resolved by calling an IExpressionInvoker object.
 * Either <i>routing-key</i> or <i>routing-key-expression</i> must be specified.</td></tr>
 * <tr><td>mapped-request-headers</td><td><b>optional</b> A regular expression used to transmit user-defined headers along with the message.
 * </table>
 */
class AmqpOutboundEndpointInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public IIntegrationAppContextAware,
	public ILifecycle {
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(IIntegrationAppContextAware)
		CAF_QI_ENTRY(ILifecycle)
	CAF_END_QI()

public:
	AmqpOutboundEndpointInstance();
	virtual ~AmqpOutboundEndpointInstance();

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

public: // IIntegrationAppContextAware
	void setIntegrationAppContext(
				SmartPtrIIntegrationAppContext context);

public: // ILifecycle
	void start(const uint32 timeoutMs);
	void stop(const uint32 timeoutMs);
	bool isRunning() const;

private:
	bool _isInitialized;
	bool _isRunning;
	std::string _id;
	SmartPtrIDocument _configSection;
	SmartPtrCMessagingTemplate _messagingTemplate;
	SmartPtrIIntegrationAppContext _context;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(AmqpOutboundEndpointInstance);
};
CAF_DECLARE_SMART_QI_POINTER(AmqpOutboundEndpointInstance);

}}
#endif /* AMQPOUTBOUNDENDPOINTINSTANCE_H_ */
