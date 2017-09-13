/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef INTEGRATIONCAFLINK_H_
#define INTEGRATIONCAFLINK_H_

#ifndef INTEGRATIONCAF_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define INTEGRATIONCAF_LINKAGE __declspec(dllexport)
        #else
            #define INTEGRATIONCAF_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define INTEGRATIONCAF_LINKAGE
    #endif
#endif

#include "CCafMessagePayload.h"
#include "CBeanPropertiesHelper.h"
#include "CCafMessageHeadersWriter.h"
#include "CCafMessageCreator.h"
#include "CCafMessageHeaders.h"
#include "CCafMessagePayloadParser.h"

#endif /* INTEGRATIONCAFLINK_H_ */
