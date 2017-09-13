/*
 *	 Author: bwilliams
 *  Created: April 6, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef Proxy_CafContracts_h_
#define Proxy_CafContracts_h_

#include "../../src/Globals/CommonGlobalsLink.h"

namespace Caf {
	extern COMMONGLOBALS_LINKAGE const char* _sConfigTmpDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigInputDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigOutputDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigSchemaCacheDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigProviderRegDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigInstallDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigInvokersDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigProvidersDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigCommonPackagesDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigWorkingDir;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigPrivateKeyFile;
	extern COMMONGLOBALS_LINKAGE const char* _sConfigCertFile;

	extern COMMONGLOBALS_LINKAGE const char* _sProviderHostArea;
	extern COMMONGLOBALS_LINKAGE const char* _sManagementAgentArea;

	extern COMMONGLOBALS_LINKAGE const char* _sSchemaSummaryFilename;
	extern COMMONGLOBALS_LINKAGE const char* _sProviderResponseFilename;
	extern COMMONGLOBALS_LINKAGE const char* _sStdoutFilename;
	extern COMMONGLOBALS_LINKAGE const char* _sStderrFilename;

	extern COMMONGLOBALS_LINKAGE const char* _sPayloadRequestFilename;
	extern COMMONGLOBALS_LINKAGE const char* _sResponseFilename;
	extern COMMONGLOBALS_LINKAGE const char* _sErrorResponseFilename;
	extern COMMONGLOBALS_LINKAGE const char* _sProviderRequestFilename;
	extern COMMONGLOBALS_LINKAGE const char* _sInfraErrFilename;

	// FxProviderFx related string constants
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdProviderDriver;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdProviderCdifFormatter;

	// MA related string constants
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdCollectSchemaExecutor;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdProviderCollectSchemaExecutor;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdProviderExecutor;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdSinglePmeRequestSplitterInstance;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdSinglePmeRequestSplitter;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdDiagToMgmtRequestTransformerInstance;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdDiagToMgmtRequestTransformer;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdInstallToMgmtRequestTransformerInstance;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdInstallToMgmtRequestTransformer;

	extern COMMONGLOBALS_LINKAGE const char* _sObjIdAttachmentRequestTransformerInstance;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdAttachmentRequestTransformer;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdVersionTransformerInstance;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdVersionTransformer;

	extern COMMONGLOBALS_LINKAGE const char* _sObjIdPersistenceNamespaceDb;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdConfigEnv;

	// Framework related string constants
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdErrorToResponseTransformerInstance;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdErrorToResponseTransformer;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdPayloadHeaderEnricherInstance;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdPayloadHeaderEnricher;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdEnvelopeToPayloadTransformerInstance;
	extern COMMONGLOBALS_LINKAGE const char* _sObjIdEnvelopeToPayloadTransformer;
}

#endif
