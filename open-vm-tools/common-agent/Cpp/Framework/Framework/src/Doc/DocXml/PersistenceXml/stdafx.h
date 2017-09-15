/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#ifdef WIN32
	#define PERSISTENCEXML_LINKAGE __declspec(dllexport)
#else
	#define PERSISTENCEXML_LINKAGE
#endif

#include <CommonDefines.h>

#include <DocContracts.h>
#include <DocUtils.h>


#endif /* STDAFX_H_ */
