/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef PersistenceXml_Link_h_
#define PersistenceXml_Link_h_

#ifndef PERSISTENCEXML_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define PERSISTENCEXML_LINKAGE __declspec(dllexport)
        #else
            #define PERSISTENCEXML_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define PERSISTENCEXML_LINKAGE
    #endif
#endif

#include "PersistenceXmlInc.h"

#endif /* PersistenceXml_Link_h_ */
