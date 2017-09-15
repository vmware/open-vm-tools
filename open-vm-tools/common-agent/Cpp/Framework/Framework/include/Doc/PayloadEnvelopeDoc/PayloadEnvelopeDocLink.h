/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
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

#include "Doc/PayloadEnvelopeDoc/PayloadEnvelopeDocInc.h"

#endif /* PayloadEnvelopeDoc_Link_h_ */
