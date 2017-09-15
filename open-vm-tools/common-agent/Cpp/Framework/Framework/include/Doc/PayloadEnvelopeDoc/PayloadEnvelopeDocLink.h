/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef PayloadEnvelopeDoc_Link_h_
#define PayloadEnvelopeDoc_Link_h_

#ifndef PAYLOADENVELOPEDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define PAYLOADENVELOPEDOC_LINKAGE __declspec(dllexport)
        #else
            #define PAYLOADENVELOPEDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define PAYLOADENVELOPEDOC_LINKAGE
    #endif
#endif


#endif /* PayloadEnvelopeDoc_Link_h_ */
