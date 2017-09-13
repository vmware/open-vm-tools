/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CPublishSubscribeChannelInstance_h_
#define CPublishSubscribeChannelInstance_h_

namespace Caf {

/// Sends responses/errors back to the client.
class CPublishSubscribeChannelInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ISubscribableChannel,
	public CAbstractMessageChannel
{
public:
	CPublishSubscribeChannelInstance();
	virtual ~CPublishSubscribeChannelInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ISubscribableChannel)
		CAF_QI_ENTRY(IMessageChannel)
		CAF_QI_ENTRY(IChannelInterceptorSupport)
	CAF_END_QI()

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

public: // ISubscribableChannel
	void subscribe(
		const SmartPtrIMessageHandler& messageHandler);

	void unsubscribe(
		const SmartPtrIMessageHandler& messageHandler);

protected: // CAbstractMessageChannel
	bool doSend(
			const SmartPtrIIntMessage& message,
			int32 timeout);

private:
	bool _isInitialized;
	SmartPtrIDocument _configSection;
	std::string _id;
	SmartPtrIMessageDispatcher _messageDispatcher;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CPublishSubscribeChannelInstance);
};

CAF_DECLARE_SMART_QI_POINTER(CPublishSubscribeChannelInstance);

}

#endif // #ifndef CPublishSubscribeChannelInstance_h_
