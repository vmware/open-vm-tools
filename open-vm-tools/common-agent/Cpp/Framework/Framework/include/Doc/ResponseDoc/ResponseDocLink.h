/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef ResponseDoc_Link_h_
#define ResponseDoc_Link_h_

#ifndef RESPONSEDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define RESPONSEDOC_LINKAGE __declspec(dllexport)
        #else
            #define RESPONSEDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define RESPONSEDOC_LINKAGE
    #endif
#endif


#endif /* ResponseDoc_Link_h_ */
