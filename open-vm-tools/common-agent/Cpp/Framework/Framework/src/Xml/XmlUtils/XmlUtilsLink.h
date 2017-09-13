/*
 *	 Author: mdonahue
 *  Created: Sep 28, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef XMLUTILSLINK_H_
#define XMLUTILSLINK_H_

#ifndef XMLUTILS_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define XMLUTILS_LINKAGE __declspec(dllexport)
        #else
            #define XMLUTILS_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define XMLUTILS_LINKAGE
    #endif
#endif

#include "CXmlElement.h"
#include "CXmlUtils.h"

#endif
