/*
 *	 Author: mdonahue
 *  Created: Jan 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#ifdef WIN32
	#define MARKUPPARSER_LINKAGE __declspec(dllexport)
	#define COMMONAGGREGATOR_LINKAGE __declspec(dllexport)
#else
	#define MARKUPPARSER_LINKAGE
	#define COMMONAGGREGATOR_LINKAGE
#endif

#include <CommonDefines.h>
#include "../../Common/CAutoMutexLockUnlock.h"
#include "../../Globals/CommonGlobalsLink.h"
#include "../../Exception/ExceptionLink.h"

#endif /* STDAFX_H_ */
