/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BasePlatformInc_h_
#define BasePlatformInc_h_

#ifndef BASEPLATFORM_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define BASEPLATFORM_LINKAGE __declspec(dllexport)
        #else
            #define BASEPLATFORM_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define BASEPLATFORM_LINKAGE
    #endif
#endif

#include "PlatformTypes.h"
#include "PlatformDefines.h"
#include "PlatformErrors.h"
#include "PlatformIID.h"
#include "PlatformStringFunc.h"
#include "ICafObject.h"
#include "TCafObject.h"
#include "TCafQIObject.h"
#include "TCafSmartPtr.h"
#include "TCafStackObject.h"

CAF_DECLARE_SMART_INTERFACE_POINTER(ICafObject);

#endif
