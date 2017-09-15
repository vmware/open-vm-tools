/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCollectSchemaExecutor_h_
#define CCollectSchemaExecutor_h_


#include "IBean.h"

#include "Integration/IIntMessage.h"
#include "Integration/IMessageProcessor.h"

using namespace Caf;

/// TODO - describe class
class CCollectSchemaExecutor :
	public TCafSubSystemObjectRoot<CCollectSchemaExecutor>,
	public IBean,
	public IMessageProcessor {
public:
	CCollectSchemaExecutor();
	virtual ~CCollectSchemaExecutor();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdCollectSchemaExecutor)

	CAF_BEGIN_INTERFACE_MAP(CCollectSchemaExecutor)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IMessageProcessor)
	CAF_END_INTERFACE_MAP()

public: // IBean
	virtual void initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties);

	virtual void terminateBean();

public: // IMessageProcessor
	SmartPtrIIntMessage processMessage(
		const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	std::string _schemaCacheDirPath;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CCollectSchemaExecutor);
};

#endif // #ifndef CCollectSchemaExecutor_h_
