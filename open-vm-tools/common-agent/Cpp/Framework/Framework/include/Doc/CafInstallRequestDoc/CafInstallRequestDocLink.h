/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CafInstallRequestDoc_Link_h_
#define CafInstallRequestDoc_Link_h_

#ifndef CAFINSTALLREQUESTDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define CAFINSTALLREQUESTDOC_LINKAGE __declspec(dllexport)
        #else
            #define CAFINSTALLREQUESTDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define CAFINSTALLREQUESTDOC_LINKAGE
    #endif
#endif

#include "Doc/CafInstallRequestDoc/CafInstallRequestDocInc.h"

#endif /* CafInstallRequestDoc_Link_h_ */
