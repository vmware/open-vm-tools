/*
 *	Author: brets
 *	Created: October 28, 2015
 *
 *	Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef PROVIDERFXLINK_H_
#define PROVIDERFXLINK_H_

#ifndef PROVIDERFX_LINKAGE
    #ifdef WIN32
        #ifdef PROVIDERFX_BUILD
            #define PROVIDERFX_LINKAGE __declspec(dllexport)
        #else
            #define PROVIDERFX_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define PROVIDERFX_LINKAGE
    #endif
#endif

#include "CProviderDriver.h"
#include "CProviderDocHelper.h"

#endif // #ifndef PROVIDERFXLINK_H_
