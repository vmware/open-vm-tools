/*
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef stdafx_h_
#define stdafx_h_

#ifdef WIN32
	#define BASEPLATFORM_LINKAGE __declspec(dllexport)
#else
	#define BASEPLATFORM_LINKAGE
#endif

#include "../../include/BaseDefines.h"
#include "BasePlatformInc.h"

#endif // #ifndef stdafx_h_
