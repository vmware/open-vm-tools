/*
 *  Created on: Nov 11, 2015
 *      Author: bwilliams
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _MaIntegration_h_
#define _MaIntegration_h_

namespace Caf {

/** @brief CAF AMQP Integration */
namespace MaIntegration {
	extern const char* _sObjIdIntegrationObjects;
}}

namespace Caf {
	extern const char* _sObjIdCollectSchemaExecutor;
	extern const char* _sObjIdProviderCollectSchemaExecutor;
	extern const char* _sObjIdProviderExecutor;
	extern const char* _sObjIdSinglePmeRequestSplitterInstance;
	extern const char* _sObjIdSinglePmeRequestSplitter;
	extern const char* _sObjIdDiagToMgmtRequestTransformerInstance;
	extern const char* _sObjIdDiagToMgmtRequestTransformer;
	extern const char* _sObjIdInstallToMgmtRequestTransformerInstance;
	extern const char* _sObjIdInstallToMgmtRequestTransformer;

	extern const char* _sObjIdAttachmentRequestTransformerInstance;
	extern const char* _sObjIdAttachmentRequestTransformer;
	extern const char* _sObjIdVersionTransformerInstance;
	extern const char* _sObjIdVersionTransformer;

	extern const char* _sObjIdPersistenceNamespaceDb;
	extern const char* _sObjIdConfigEnv;
}

#endif /* _MaIntegration_h_ */
