/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CReplyToResolverInstance_h
#define CReplyToResolverInstance_h

#include "IBean.h"
#include "IVariant.h"
#include "ReplyToResolver.h"
#include "Integration/IIntMessage.h"
#include "Integration/IExpressionInvoker.h"

namespace Caf {

class CReplyToResolverInstance :
	public TCafSubSystemObjectRoot<CReplyToResolverInstance>,
	public IBean,
	public ReplyToResolver,
	public IExpressionInvoker {
public:
	CReplyToResolverInstance();
	virtual ~CReplyToResolverInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdCommIntegrationReplyToResolver)

	CAF_BEGIN_INTERFACE_MAP(CReplyToResolverInstance)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(ReplyToResolver)
		CAF_INTERFACE_ENTRY(IExpressionInvoker)
	CAF_END_INTERFACE_MAP()

public: // IBean
	void initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties);

	void terminateBean();

public: // ReplyToResolver
	std::string cacheReplyTo(const SmartPtrIIntMessage& message);

	std::string lookupReplyTo(const SmartPtrIIntMessage& message);

	static std::string getResolverCacheFilePath();

private: // ReplyToResolver
	void loadCache();
	void persistCache();

public: // IExpressionInvoker
	SmartPtrIVariant invokeExpression(
			const std::string& methodName,
			const Cdeqstr& methodParams,
			const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	typedef std::map<UUID, std::string, SGuidLessThan> AddressMap;
	AddressMap _replyToAddresses;
	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CReplyToResolverInstance);
};
}

#endif /* CReplyToResolverInstance_h */
