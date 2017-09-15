/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef COMMONAGGREGATORLINK_H_
#define COMMONAGGREGATORLINK_H_

#ifndef COMMONAGGREGATOR_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define COMMONAGGREGATOR_LINKAGE __declspec(dllexport)
        #else
            #define COMMONAGGREGATOR_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define COMMONAGGREGATOR_LINKAGE
    #endif
#endif

#include <algorithm>
#include <functional>
#include <locale>
#include <errno.h>
#include <glib/gstdio.h>

#include <IBean.h>
#include <IVariant.h>
#include <CommonGlobals.h>

#include "../Logging/LoggingLink.h"
#include "../Exception/ExceptionLink.h"
#include "../Collections/Iterators/IteratorsInc.h"
#include "../Collections/Graphs/GraphsInc.h"

#include "CTimeUnit.h"
#include "CFileLock.h"
#include "CAutoFileUnlock.h"
#include "CCafRegex.h"
#include "CStringUtils.h"
#include "CFileSystemUtils.h"
#include "CProcessUtils.h"
#include "CDateTimeUtils.h"
#include "CThreadUtils.h"
#include "IConfigParams.h"
#include "CConfigParams.h"
#include "CConfigParamsChain.h"
#include "IAppConfig.h"
#include "IAppConfigWrite.h"
#include "IAppContext.h"
#include "AppConfigUtils.h"
#include "CAutoMutex.h"
#include "CAutoRecMutex.h"
#include "CAutoCondition.h"
#include "CAutoMutexLockUnlock.h"
#include "CAutoMutexLockUnlockRaw.h"
#include "CAutoMutexUnlockLock.h"
#include "CThreadSignal.h"
#include "CCmdLineOptions.h"
#include "CafInitialize.h"
#include "CHexCodec.h"
#include "UriUtils.h"
#include "CLoggingUtils.h"
#include "CLoggingSetter.h"
#include "CThreadPool.h"
#include "CManagedThreadPool.h"
#include "TBlockingCell.h"
#include "CIniFile.h"
#include "CVariant.h"
#include "CEnvironmentUtils.h"
#include "CPersistenceUtils.h"
#include "IWork.h"

#if defined(__linux__) || defined(__APPLE__)
#include "CDaemonUtils.h"
#elif WIN32
#include "CWinScm.h"
#include "CWinServiceState.h"
#include "CWinServiceInstance.h"
#endif

#include "../SubSystemBase/SubSystemBaseLink.h"
#include "../Xml/MarkupParser/MarkupParserLink.h"
#include "../Xml/XmlUtils/XmlUtilsLink.h"
#include "../Memory/DynamicArray/DynamicArrayInc.h"

#endif /* COMMONAGGREGATORLINK_H_ */
