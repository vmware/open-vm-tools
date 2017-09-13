/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CHeaderValueRouterInstance_h_
#define CHeaderValueRouterInstance_h_

namespace Caf {

class CHeaderValueRouterInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public CAbstractMessageRouter
{
public:
	CHeaderValueRouterInstance();
	virtual ~CHeaderValueRouterInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(IMessageRouter)
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

private:
	ChannelCollection getTargetChannels(
			const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	std::string _id;
	std::string _defaultOutputChannelId;
	bool _resolutionRequired;
	std::string _headerName;
	Cmapstrstr _valueToChannelMapping;
	SmartPtrIChannelResolver _channelResolver;

private:
	std::string calcOutputChannel(
		const SmartPtrIIntMessage& message) const;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CHeaderValueRouterInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CHeaderValueRouterInstance);
}

#endif // #ifndef CHeaderValueRouterInstance_h_
