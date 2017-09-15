/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CFileToStringTransformerInstance_h_
#define CFileToStringTransformerInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CFileToStringTransformerInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer
{
public:
	CFileToStringTransformerInstance();
	virtual ~CFileToStringTransformerInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ITransformer)
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

public: // ITransformer
	SmartPtrIIntMessage transformMessage(
		const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	std::string _id;
	bool _deleteFiles;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CFileToStringTransformerInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CFileToStringTransformerInstance);

}

#endif // #ifndef CFileToStringTransformerInstance_h_
