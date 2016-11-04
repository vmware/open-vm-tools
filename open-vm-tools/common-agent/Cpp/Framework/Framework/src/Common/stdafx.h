/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#ifdef WIN32
	#define COMMONAGGREGATOR_LINKAGE __declspec(dllexport)
	#define EXCEPTION_LINKAGE __declspec(dllexport)
	#define LOGGING_LINKAGE __declspec(dllexport)
	#define SUBSYSTEMBASE_LINKAGE __declspec(dllexport)
	#define MARKUPPARSER_LINKAGE __declspec(dllexport)
	#define XMLUTILS_LINKAGE __declspec(dllexport)
#else
	#define COMMONAGGREGATOR_LINKAGE
	#define EXCEPTION_LINKAGE
	#define LOGGING_LINKAGE
	#define SUBSYSTEMBASE_LINKAGE
	#define MARKUPPARSER_LINKAGE
	#define XMLUTILS_LINKAGE
#endif

#include <BaseDefines.h>

#include "CommonAggregatorLink.h"

#endif /* STDAFX_H_ */
