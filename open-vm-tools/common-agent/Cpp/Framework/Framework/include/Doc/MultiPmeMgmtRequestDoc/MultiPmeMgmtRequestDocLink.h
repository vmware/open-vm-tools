/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef MultiPmeMgmtRequestDoc_Link_h_
#define MultiPmeMgmtRequestDoc_Link_h_

#ifndef MULTIPMEMGMTREQUESTDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define MULTIPMEMGMTREQUESTDOC_LINKAGE __declspec(dllexport)
        #else
            #define MULTIPMEMGMTREQUESTDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define MULTIPMEMGMTREQUESTDOC_LINKAGE
    #endif
#endif


#endif /* MultiPmeMgmtRequestDoc_Link_h_ */
