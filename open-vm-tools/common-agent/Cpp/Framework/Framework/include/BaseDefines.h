/*
 *	 Author: mdonahue
 *  Created: Jan 12, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef SYS_INC_BASEDEFINES_H_
#define SYS_INC_BASEDEFINES_H_

#ifdef ECM_SUB_SYSTEM
#error "The ECM_SUB_SYSTEM define has been deprecated.  Use CAF_SUB_SYSTEM instead."
#endif

#ifdef WIN32
// Disable compiler warning 4275: non dll-interface used as base for dll-interface class
#pragma warning(disable: 4275)
#endif

#include <glib.h>

const gboolean GLIB_FALSE = (gboolean)0;
const gboolean GLIB_TRUE = !GLIB_FALSE;

// Windows includes
#if defined( WIN32 )
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif

	#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0502
	#endif

	#include <windows.h>
#endif

#include <stdint.h>
#if defined( WIN32 )
	typedef __int8 int8;
	typedef unsigned __int8  uint8;
	typedef __int16 int16;
	typedef unsigned __int16 uint16;
	typedef __int32 int32;
	typedef unsigned __int32 uint32;
	typedef __int64 int64;
	typedef unsigned __int64 uint64;

	struct timezone
	{
		int32 tz_minuteswest;
		int32 tz_dsttime;
	};

#else
	#include <unistd.h>

	typedef signed char int8;
	typedef unsigned char  uint8;
	typedef int16_t int16;
	typedef uint16_t uint16;
	typedef int32_t int32;
	typedef uint32_t uint32;
	typedef int64_t int64;
	typedef uint64_t uint64;
#endif

#include <typeinfo>
#include <stdexcept>

#include "BasePlatformLink.h"

#endif /* SYS_INC_BASEDEFINES_H_ */
