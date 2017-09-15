/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
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


#endif /* PersistenceXml_Link_h_ */
