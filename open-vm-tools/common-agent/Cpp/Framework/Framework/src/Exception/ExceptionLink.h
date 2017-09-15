/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef EXCEPTIONLINK_H_
#define EXCEPTIONLINK_H_

#ifndef EXCEPTION_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define EXCEPTION_LINKAGE __declspec(dllexport)
        #else
            #define EXCEPTION_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define EXCEPTION_LINKAGE
    #endif
#endif

#include "CBacktraceUtils.h"
#include "ClassMacros.h"
#include "ExceptionMacros.h"
#include "CCafExceptionEx.h"
#include "ExceptionExMacros.h"
#include "CValidate.h"
#include "ValidationMacros.h"
#include "ValidationMacrosRaw.h"
#include "StatusMacros.h"

#endif /* EXCEPTIONLINK_H_ */
