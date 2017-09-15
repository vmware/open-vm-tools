/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#ifdef WIN32
	#define INTEGRATIONCAF_LINKAGE __declspec(dllexport)
#else
	#define INTEGRATIONCAF_LINKAGE
#endif

#include <CommonDefines.h>
#include <DocUtils.h>
#include <Integration.h>

#endif /* STDAFX_H_ */
