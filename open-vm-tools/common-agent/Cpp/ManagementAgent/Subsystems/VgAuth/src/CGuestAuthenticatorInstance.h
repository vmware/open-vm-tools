/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CGuestAuthenticatorInstance_h_
#define CGuestAuthenticatorInstance_h_


#include "Integration/IErrorProcessor.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CGuestAuthenticatorInstance :
	public TCafSubSystemObjectRoot<CGuestAuthenticatorInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer,
	public IErrorProcessor {
public:
	CGuestAuthenticatorInstance();
	virtual ~CGuestAuthenticatorInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdGuestAuthenticatorInstance)

	CAF_BEGIN_INTERFACE_MAP(CGuestAuthenticatorInstance)
		CAF_INTERFACE_ENTRY(IIntegrationObject)
		CAF_INTERFACE_ENTRY(IIntegrationComponentInstance)
		CAF_INTERFACE_ENTRY(ITransformer)
		CAF_INTERFACE_ENTRY(IErrorProcessor)
	CAF_END_INTERFACE_MAP()

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

public: // ITransformer
	SmartPtrIIntMessage transformMessage(
			const SmartPtrIIntMessage& message);

public: // IErrorProcessor
	SmartPtrIIntMessage processErrorMessage(
			const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	std::string _id;
	bool _beginImpersonation;
	bool _endImpersonation;

private:
	std::string getSignedSamlToken(
			const SmartPtrCDynamicByteArray& payload) const;

	std::string findOptionalProperty(
			const std::string& propertyName,
			const IBean::Cprops& properties) const;

	void logUserInfo(
			const std::string& msg) const;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CGuestAuthenticatorInstance);
};

}

#endif // #ifndef CGuestAuthenticatorInstance_h_
