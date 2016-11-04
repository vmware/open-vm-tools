/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#ifdef WIN32
	#define LOGGING_LINKAGE __declspec(dllexport)
	#define COMMONAGGREGATOR_LINKAGE __declspec(dllexport)
#else
	#define LOGGING_LINKAGE
	#define COMMONAGGREGATOR_LINKAGE
#endif

#include <BaseDefines.h>
#include <stdio.h>
#include <stdlib.h>
#define LOG4CPP_FIX_ERROR_COLLISION 1
#include <log4cpp/Category.hh>
//#include <log4cpp/PropertyConfigurator.hh>
//#include <log4cpp/Configurator.hh>
//#include <log4cpp/FileAppender.hh>
//#include <log4cpp/PatternLayout.hh>

#include "../Exception/ExceptionLink.h"

#include "CLogger.h"
#include "LoggingMacros.h"

#endif /* STDAFX_H_ */
