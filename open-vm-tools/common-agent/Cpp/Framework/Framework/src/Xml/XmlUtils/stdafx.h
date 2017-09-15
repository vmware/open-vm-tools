/*
 *	 Author: mdonahue
 *  Created: Sep 28, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#ifdef WIN32
	#define XMLUTILS_LINKAGE __declspec(dllexport)
	#define COMMONAGGREGATOR_LINKAGE __declspec(dllexport)
#else
	#define XMLUTILS_LINKAGE
	#define COMMONAGGREGATOR_LINKAGE
#endif

#include <BaseDefines.h>
#include <CommonGlobals.h>
#include "../../Exception/ExceptionLink.h"
#include "../../Logging/LoggingLink.h"
#include "../../Collections/Iterators/IteratorsInc.h"
#include "../../Common/CFileSystemUtils.h"
#include "../MarkupParser/MarkupParserLink.h"

#endif /* STDAFX_H_ */
