/*
 *  Created: Mar 21, 2004
 *
 *	Copyright (c) 2004-2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef stdafx_h_
#define stdafx_h_

#ifdef WIN32
	#define SUBSYSTEMBASE_LINKAGE __declspec(dllexport)
#else
	#define SUBSYSTEMBASE_LINKAGE
#endif

#include <BaseDefines.h>
#include "../Common/CommonAggregatorLink.h"

#endif // #ifndef stdafx_h_
