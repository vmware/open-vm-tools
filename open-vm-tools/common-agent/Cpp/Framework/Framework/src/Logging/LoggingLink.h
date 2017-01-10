/*
 *	 Author: bwilliams
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef LOGGINGLINK_H_
#define LOGGINGLINK_H_

#ifndef LOGGING_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define LOGGING_LINKAGE __declspec(dllexport)
        #else
            #define LOGGING_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define LOGGING_LINKAGE
    #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#define LOG4CPP_FIX_ERROR_COLLISION 1
#include <log4cpp/Category.hh>
#include <log4cpp/PropertyConfigurator.hh>
#include <log4cpp/Configurator.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/PatternLayout.hh>

#include "../Exception/ExceptionLink.h"

#include "CLogger.h"
#include "LoggingMacros.h"

#endif /* LOGGINGLINK_H_ */
