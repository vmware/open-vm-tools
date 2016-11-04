/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef DiagRequestDoc_Link_h_
#define DiagRequestDoc_Link_h_

#ifndef DIAGREQUESTDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define DIAGREQUESTDOC_LINKAGE __declspec(dllexport)
        #else
            #define DIAGREQUESTDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define DIAGREQUESTDOC_LINKAGE
    #endif
#endif


#endif /* DiagRequestDoc_Link_h_ */
