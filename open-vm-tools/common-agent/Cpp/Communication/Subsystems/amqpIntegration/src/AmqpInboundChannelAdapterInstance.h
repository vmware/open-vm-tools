/*
 *  Created on: Aug 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AmqpInboundChannelAdapterInstance_h
#define AmqpInboundChannelAdapterInstance_h


#include "Integration/IIntegrationAppContextAware.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationAppContext.h"
#include "Integration/ITaskExecutor.h"
#include "amqpCore/SimpleMessageListenerContainer.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IPhased.h"
#include "Integration/ISmartLifecycle.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObj
 * @brief Adapter that receives messages from an AMQP queue, converts them into
 * integration messages, and sends the results to a Message Channel.
 * <p>
 * <pre>
 * Example context file declaration:
 *
 * <rabbit-inbound-channel-adapter
 * 	id="inboundAmqp"
 * 	channel="inboundChannel"
 * 	queue-name="inputQueue"
 * 	acknowledge-mode="AUTO"
 * 	connection-factory="connectionFactory"
 * 	error-channel="errorChannel"
 * 	mapped-request-headers="^myApp[.].*"
 * 	auto-startup="true"
 * 	phase="1234"
 * 	prefetch-count="100"
 * 	receive-timeout="5000"
 * 	recovery-interval="15000"
 * 	tx-size="25" />
 *
 * <rabbit-inbound-channel-adapter
 * 	id="inboundAmqp"
 * 	channel="inboundChannel"
 * 	queue-name="#{inputQueue}"
 * 	error-channel="errorChannel" />
 * <rabbit-queue
 *    id="inputQueue"
 *    name="myapp.inputq" />
 *
 * <rabbit-inbound-channel-adapter
 * 	id="inboundAmqp"
 * 	channel="inboundChannel"
 * 	queue-name="${var:appInputQ}"
 * 	error-channel="errorChannel" />
 *
 *	</pre>
 * <p>
 * XML attribute definitions:
 * <table border="1">
 * <tr><th>Attribute</th><th>Description</th></tr>
 * <tr><td>id</td><td><b>optional</b> Unique id for this adapter.</td></tr>
 * <tr><td>channel</td><td><b>required</b> The channel to which messages should be sent.</td></tr>
 * <tr><td>queue-name</td><td><b>required</b> The AMQP queue from which messaged should be consumed.</td></tr>
 * <tr><td>acknowledge-mode</td><td><b>optional</b> Acknowledgment mode (NONE, AUTO, MANUAL). Defaults to AUTO.</td></tr>
 * <tr><td>connection-factory</td><td><b>optional</b> Bean reference to the RabbitMQ ConnectionFactory. Defaults to 'connectionFactory'</td></tr>
 * <tr><td>error-channel</td><td><b>required</b> Message channel to which error messages should be sent.</td></tr>
 * <tr><td>mapped-request-headers</td><td><b>optional</b> A regular expression indicating which AMQP headers will be mapped into message headers.</td></tr>
 * <tr><td>auto-startup</td><td><b>optional</b> Specifies if the adapter is to start automatically.  If 'false', the adapter must be started programatically.  Defaults to 'true'.</td></tr>
 * <tr><td>phase</td><td><b>optional</b> Specifies the phase in which the adapter should be started. By default this value is G_MAXINT32 meaning that this adapter will start as late as possible.</td></tr>
 * <tr><td>prefetch-count</td><td><b>optional</b> Tells the AMQP broker how many messages to send to the the consumer in a single request. Defaults to 1.</td></tr>
 * <tr><td>receive-timeout</td><td><b>optional</b> Receive timeout in milliseconds.  Defaults to 1000.</td></tr>
 * <tr><td>recovery-interval</td><td><b>optional</b> Specifies the interval between broker connection recovery attempts in milliseconds.  Defaults to 5000.</td></tr>
 * <tr><td>tx-size</td><td><b>optional</b> Tells the adapter how many messages to process in a single batch.  This should be less than or equal to to prefetch-count. Defaults to 1.</td></tr>
 * </table>
 */
class AmqpInboundChannelAdapterInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public IIntegrationAppContextAware,
	public ISmartLifecycle,
	public IPhased {
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(IIntegrationAppContextAware)
		CAF_QI_ENTRY(ILifecycle)
		CAF_QI_ENTRY(ISmartLifecycle)
		CAF_QI_ENTRY(IPhased)
	CAF_END_QI()
public:
	AmqpInboundChannelAdapterInstance();
	virtual ~AmqpInboundChannelAdapterInstance();

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

public: // ISmartLifecycle
	bool isAutoStartup() const;

public: // ILifecycle
	void start(const uint32 timeoutMs);
	void stop(const uint32 timeoutMs);
	bool isRunning() const;

public: // IPhased
	int32 getPhase() const;

private:
	bool _isInitialized;
	bool _isRunning;
	SmartPtrIIntegrationAppContext _intAppContext;
	SmartPtrSimpleMessageListenerContainer _listenerContainer;
	SmartPtrITaskExecutor _taskExecutor;

	std::string _idProp;
	std::string _channelProp;
	std::string _queueProp;
	AcknowledgeMode _ackModeProp;
	std::string _connectionFactoryProp;
	std::string _errorChannelProp;
	std::string _mappedRequestHeadersProp;
	bool _autoStartupProp;
	int32 _phaseProp;
	uint32 _prefetchCountProp;
	uint32 _receiveTimeoutProp;
	uint32 _recoveryIntervalProp;
	uint32 _txSizeProp;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(AmqpInboundChannelAdapterInstance);
};
CAF_DECLARE_SMART_QI_POINTER(AmqpInboundChannelAdapterInstance);

}}

#endif /* AmqpInboundChannelAdapterInstance_h */
