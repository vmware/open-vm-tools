/*
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef Common_CommonGlobalsLink_h_
#define Common_CommonGlobalsLink_h_

#ifndef COMMONGLOBALS_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define COMMONGLOBALS_LINKAGE __declspec(dllexport)
        #else
            #define COMMONGLOBALS_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define COMMONGLOBALS_LINKAGE
    #endif
#endif

#include "CommonDefines.h"
#include "ClassMacroStrings.h"

#endif
