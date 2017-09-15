/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef stdafx_h_
#define stdafx_h_

//{{CAF_SUBSYSTEM}}

#include <CommonDefines.h>
#include <Integration.h>
#include <DocXml.h>
#include <DocUtils.h>

#include "MaContracts.h"
#include "MaIntegration.h"
#include "IntegrationObjects.h"

#include "CAttachmentRequestTransformerInstance.h"
#include "CAttachmentRequestTransformer.h"

#include "CDiagToMgmtRequestTransformerInstance.h"
#include "CDiagToMgmtRequestTransformer.h"

#include "CInstallToMgmtRequestTransformerInstance.h"
#include "CInstallToMgmtRequestTransformer.h"

#include "CPersistenceNamespaceDb.h"
#include "CPersistenceReadingMessageSource.h"
#include "CPersistenceMessageHandler.h"
#include "CPersistenceInboundChannelAdapterInstance.h"
#include "CPersistenceOutboundChannelAdapterInstance.h"

#include "CPersistenceMerge.h"
#include "CConfigEnvMerge.h"
#include "CConfigEnv.h"
#include "CConfigEnvReadingMessageSource.h"
#include "CConfigEnvMessageHandler.h"
#include "CConfigEnvInboundChannelAdapterInstance.h"
#include "CConfigEnvOutboundChannelAdapterInstance.h"

#include "CSchemaCacheManager.h"
#include "CResponseFactory.h"

#include "CCollectSchemaExecutor.h"
#include "CProviderCollectSchemaExecutor.h"
#include "CProviderExecutorRequest.h"
#include "CProviderExecutorRequestHandler.h"
#include "CProviderExecutor.h"

#include "CSinglePmeRequestSplitter.h"
#include "CSinglePmeRequestSplitterInstance.h"

#include "CMonitorReadingMessageSource.h"
#include "CMonitorInboundChannelAdapterInstance.h"

#include "CVersionTransformer.h"
#include "CVersionTransformerInstance.h"

#endif // #ifndef stdafx_h_
