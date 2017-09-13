/*
 *	 Author: bwilliams
 *  Created: 10/19/2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include <CafContracts.h>

namespace Caf {
	const char* _sConfigTmpDir = "tmp_dir";
	const char* _sConfigInputDir = "input_dir";
	const char* _sConfigOutputDir = "output_dir";
	const char* _sConfigSchemaCacheDir = "schema_cache_dir";
	const char* _sConfigProviderRegDir = "provider_reg_dir";
	const char* _sConfigInstallDir = "install_dir";
	const char* _sConfigInvokersDir = "invokers_dir";
	const char* _sConfigProvidersDir = "providers_dir";
	const char* _sConfigCommonPackagesDir = "common_packages_dir";
	const char* _sConfigWorkingDir = "working_dir";
	const char* _sConfigPrivateKeyFile = "private_key_file";
	const char* _sConfigCertFile = "cert_file";

	const char* _sProviderHostArea = "providerHost";
	const char* _sManagementAgentArea = "managementAgent";

	const char* _sSchemaSummaryFilename = "schemaSummary.xml";
	const char* _sProviderResponseFilename = "providerResponse.xml";
	const char* _sStdoutFilename = "_provider_stdout_";
	const char* _sStderrFilename = "_provider_stderr_";

	const char* _sPayloadRequestFilename = "payloadRequest.xml";
	const char* _sResponseFilename = "response.xml";
	const char* _sErrorResponseFilename = "errorResponse.xml";
	const char* _sProviderRequestFilename = "providerRequest.xml";
	const char* _sInfraErrFilename = "_infraError_";

	// FxProviderFx related string constants
	const char* _sObjIdProviderDriver = "com.vmware.commonagent.providerfx.providerdriver";
	const char* _sObjIdProviderCdifFormatter = "com.vmware.commonagent.providerfx.providercdifformatter";

	// MA related string constants
	const char* _sObjIdCollectSchemaExecutor = "com.vmware.commonagent.maintegration.collectschemaexecutor";
	const char* _sObjIdProviderCollectSchemaExecutor = "com.vmware.commonagent.maintegration.providercollectschemaexecutor";
	const char* _sObjIdProviderExecutor = "com.vmware.commonagent.maintegration.providerexecutor";
	const char* _sObjIdSinglePmeRequestSplitterInstance = "com.vmware.commonagent.maintegration.singlepmerequestsplitterinstance";
	const char* _sObjIdSinglePmeRequestSplitter = "com.vmware.commonagent.maintegration.singlepmerequestsplitter";
	const char* _sObjIdDiagToMgmtRequestTransformerInstance = "com.vmware.commonagent.maintegration.diagtomgmtrequesttransformerinstance";
	const char* _sObjIdDiagToMgmtRequestTransformer = "com.vmware.commonagent.maintegration.diagtomgmtrequesttransformer";
	const char* _sObjIdInstallToMgmtRequestTransformerInstance = "com.vmware.commonagent.maintegration.installtomgmtrequesttransformerinstance";
	const char* _sObjIdInstallToMgmtRequestTransformer = "com.vmware.commonagent.maintegration.installtomgmtrequesttransformer";
	const char* _sObjIdPersistenceNamespaceDb = "com.vmware.commonagent.maintegration.persistencenamespacedb";
	const char* _sObjIdConfigEnv = "com.vmware.commonagent.maintegration.configenv";

	const char* _sObjIdAttachmentRequestTransformerInstance = "com.vmware.commonagent.maintegration.attachmentrequesttransformerinstance";
	const char* _sObjIdAttachmentRequestTransformer = "com.vmware.commonagent.maintegration.attachmentrequesttransformer";
	const char* _sObjIdVersionTransformerInstance = "com.vmware.commonagent.maintegration.versiontransformerinstance";
	const char* _sObjIdVersionTransformer = "com.vmware.commonagent.maintegration.versiontransformer";

	// Framework related string constants
	const char* _sObjIdErrorToResponseTransformerInstance = "com.vmware.commonagent.cafintegration.errortoresponsetransformerinstance";
	const char* _sObjIdErrorToResponseTransformer = "com.vmware.commonagent.cafintegration.errortoresponsetransformer";
	const char* _sObjIdPayloadHeaderEnricherInstance = "com.vmware.commonagent.cafintegration.payloadheaderenricherinstance";
	const char* _sObjIdPayloadHeaderEnricher = "com.vmware.commonagent.cafintegration.payloadheaderenricher";
	const char* _sObjIdEnvelopeToPayloadTransformerInstance = "com.vmware.commonagent.cafintegration.envelopetopayloadtransformerinstance";
	const char* _sObjIdEnvelopeToPayloadTransformer = "com.vmware.commonagent.cafintegration.envelopetopayloadtransformer";
}
