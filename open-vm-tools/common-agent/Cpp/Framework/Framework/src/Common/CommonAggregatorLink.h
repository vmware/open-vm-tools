/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
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
#include <CommonGlobals.h>

#include "../Logging/LoggingLink.h"
#include "../Exception/ExceptionLink.h"
#include "../Collections/Iterators/IteratorsInc.h"
#include "../Collections/Graphs/GraphsInc.h"

#include "CTimeUnit.h"
#include "CAutoFileUnlock.h"
#include "CStringUtils.h"
#include "CFileSystemUtils.h"
#include "CProcessUtils.h"
#include "CDateTimeUtils.h"
#include "CThreadUtils.h"
#include "AppConfigUtils.h"
#include "CAutoMutexLockUnlock.h"
#include "CAutoMutexLockUnlockRaw.h"
#include "CAutoMutexUnlockLock.h"
#include "CafInitialize.h"
#include "CHexCodec.h"
#include "UriUtils.h"
#include "TBlockingCell.h"
#include "Common/CVariant.h"
#include "CEnvironmentUtils.h"
#include "CPersistenceUtils.h"

#if defined(__linux__) || defined(__APPLE__)
#include "CDaemonUtils.h"
#elif WIN32
#endif

#include "../SubSystemBase/SubSystemBaseLink.h"
#include "../Xml/MarkupParser/MarkupParserLink.h"
#include "../Xml/XmlUtils/XmlUtilsLink.h"

#endif /* COMMONAGGREGATORLINK_H_ */
