/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef SchemaTypesDoc_Link_h_
#define SchemaTypesDoc_Link_h_

#ifndef SCHEMATYPESDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define SCHEMATYPESDOC_LINKAGE __declspec(dllexport)
        #else
            #define SCHEMATYPESDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define SCHEMATYPESDOC_LINKAGE
    #endif
#endif


#endif /* SchemaTypesDoc_Link_h_ */
