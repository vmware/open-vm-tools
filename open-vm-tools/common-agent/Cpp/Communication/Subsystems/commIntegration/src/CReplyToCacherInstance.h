/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CReplyToCacherInstance_h
#define CReplyToCacherInstance_h



#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "ReplyToResolver.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CReplyToCacherInstance :
	public TCafSubSystemObjectRoot<CReplyToCacherInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer {
public:
	CReplyToCacherInstance();
	virtual ~CReplyToCacherInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdCommIntegrationReplyToCacherInstance)

	CAF_BEGIN_INTERFACE_MAP(CReplyToCacherInstance)
		CAF_INTERFACE_ENTRY(IIntegrationObject)
		CAF_INTERFACE_ENTRY(IIntegrationComponentInstance)
		CAF_INTERFACE_ENTRY(ITransformer)
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

private:
	bool _isInitialized;
	std::string _id;
	std::string _replyToResolverId;
	SmartPtrReplyToResolver _replyToResolver;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CReplyToCacherInstance);
};

}

#endif /* CReplyToCacherInstance_h */
