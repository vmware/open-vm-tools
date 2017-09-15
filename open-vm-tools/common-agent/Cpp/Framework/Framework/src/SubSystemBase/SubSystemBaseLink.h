/*
 *  Created: Nov 21, 2003
 *
 *	Copyright (C) 2003-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef SubSystemBaseLink_h_
#define SubSystemBaseLink_h_

#ifndef SUBSYSTEMBASE_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define SUBSYSTEMBASE_LINKAGE __declspec(dllexport)
        #else
            #define SUBSYSTEMBASE_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define SUBSYSTEMBASE_LINKAGE
    #endif
#endif

#include "SubSystemBaseInc.h"

#include "CEcmDllManager.h"
#include "CEcmSubSystem.h"
#include "CEcmSubSystemModule.h"
#include "CEcmSubSystemRegistry.h"

#endif // #ifndef SubSystemBaseLink_h_
