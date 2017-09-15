/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CProviderCollectSchemaExecutor_h_
#define CProviderCollectSchemaExecutor_h_


#include "IBean.h"

#include "Common/CLoggingSetter.h"
#include "Doc/ProviderInfraDoc/CProviderRegDoc.h"
#include "Doc/ProviderInfraDoc/CSchemaSummaryDoc.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageProcessor.h"

using namespace Caf;

/// TODO - describe class
class CProviderCollectSchemaExecutor :
	public TCafSubSystemObjectRoot<CProviderCollectSchemaExecutor>,
	public IBean,
	public IMessageProcessor {
public:
	CProviderCollectSchemaExecutor();
	virtual ~CProviderCollectSchemaExecutor();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdProviderCollectSchemaExecutor)

	CAF_BEGIN_INTERFACE_MAP(CProviderCollectSchemaExecutor)
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
	void executeProvider(
		const SmartPtrCProviderRegDoc& providerReg,
		const std::string& invokersDir,
		const std::string& providerSchemaCacheDir,
		const std::string& providerResponsePath,
		SmartPtrCLoggingSetter& loggingSetter) const;

	void setupSchemaCacheDir(
		const std::string& providerSchemaCacheDir,
		const SmartPtrCLoggingSetter& loggingSetter) const;

	void runProvider(
		const std::string& invokerPath,
		const std::string& providerSchemaCacheDir) const;

	SmartPtrCSchemaSummaryDoc createSchemaSummary(
		const std::string& schemaPath,
		const std::string& invokerPath,
		const std::string& providerNamespace,
		const std::string& providerName,
		const std::string& providerVersion) const;

	std::string findSchemaPath(
		const std::string& providerResponsePath) const;

private:
	bool _isInitialized;
	std::string _schemaCacheDirPath;
	std::string _invokersDir;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CProviderCollectSchemaExecutor);
};

#endif // #ifndef CProviderCollectSchemaExecutor_h_
