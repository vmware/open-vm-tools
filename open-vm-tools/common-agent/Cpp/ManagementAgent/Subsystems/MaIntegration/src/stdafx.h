/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef stdafx_h_
#define stdafx_h_

//{{CAF_SUBSYSTEM}}

#include <CommonDefines.h>
#include <Integration.h>
#include <DocUtils.h>

#include "MaContracts.h"
#include "MaIntegration.h"
#include "IntegrationObjects.h"

#include "CAttachmentRequestTransformer.h"

#include "CDiagToMgmtRequestTransformer.h"

#include "CInstallToMgmtRequestTransformerInstance.h"
#include "CInstallToMgmtRequestTransformer.h"

#include "CPersistenceNamespaceDb.h"
#include "CPersistenceMessageHandler.h"
#include "CPersistenceInboundChannelAdapterInstance.h"
#include "CPersistenceOutboundChannelAdapterInstance.h"

#include "CPersistenceMerge.h"
#include "CConfigEnvMerge.h"
#include "CConfigEnv.h"
#include "CConfigEnvMessageHandler.h"
#include "CConfigEnvInboundChannelAdapterInstance.h"
#include "CConfigEnvOutboundChannelAdapterInstance.h"


#include "CCollectSchemaExecutor.h"
#include "CProviderCollectSchemaExecutor.h"
#include "CProviderExecutor.h"

#include "CSinglePmeRequestSplitter.h"

#include "CMonitorInboundChannelAdapterInstance.h"

#include "CVersionTransformer.h"
#include "CVersionTransformerInstance.h"

#endif // #ifndef stdafx_h_
