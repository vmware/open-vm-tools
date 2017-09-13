/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CNullChannelInstance_h_
#define CNullChannelInstance_h_

namespace Caf {

/// Sends responses/errors back to the client.
class CNullChannelInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public CAbstractMessageChannel
{
public:
	CNullChannelInstance();
	virtual ~CNullChannelInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
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

protected: // CAbstractMessageChannel
	bool doSend(
			const SmartPtrIIntMessage& message,
			int32 timeout);

private:
	bool _isInitialized;
	std::string _id;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CNullChannelInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CNullChannelInstance);
}

#endif // #ifndef CNullChannelInstance_h_
