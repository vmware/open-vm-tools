/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef stdafx_h_
#define stdafx_h_

#ifdef WIN32
	#define BASEPLATFORM_LINKAGE __declspec(dllexport)
#else
	#define BASEPLATFORM_LINKAGE
#endif

#include <BaseDefines.h>
#include "BasePlatformInc.h"

#endif // #ifndef stdafx_h_
