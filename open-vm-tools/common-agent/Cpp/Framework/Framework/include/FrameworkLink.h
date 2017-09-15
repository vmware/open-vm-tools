/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef FrameworkLink_h_
#define FrameworkLink_h_

#ifndef FRAMEWORK_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define FRAMEWORK_LINKAGE __declspec(dllexport)
        #else
            #define FRAMEWORK_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define FRAMEWORK_LINKAGE
    #endif
#endif

#endif
