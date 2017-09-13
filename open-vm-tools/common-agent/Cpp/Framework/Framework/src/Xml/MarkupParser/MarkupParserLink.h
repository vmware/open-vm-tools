/*
 *	 Author: mdonahue
 *  Created: Jan 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef MARKUPPARSERLINK_H_
#define MARKUPPARSERLINK_H_

#ifndef MARKUPPARSER_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define MARKUPPARSER_LINKAGE __declspec(dllexport)
        #else
            #define MARKUPPARSER_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define MARKUPPARSER_LINKAGE
    #endif
#endif

#include "CMarkupParser.h"

#endif /* MARKUPPARSERLINK_H_ */
