/*
 *	 Author: mdonahue
 *  Created: Jan 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
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
#include <CommonGlobals.h>
#include "../../Common/CAutoMutexLockUnlock.h"
#include "../../Exception/ExceptionLink.h"

#endif /* STDAFX_H_ */
