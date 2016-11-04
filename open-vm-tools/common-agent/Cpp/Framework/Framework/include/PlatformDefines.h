/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef PLATFORM_DEFINES_H
#define PLATFORM_DEFINES_H

#ifdef WIN32
	#ifdef _WIN64
	typedef uint64 SUBSYS_INTPTR;
	const SUBSYS_INTPTR SUBSYS_INTPTR_INVALID = 0xffffffffffffffff;
	#else
	typedef uint32 SUBSYS_INTPTR;
	const SUBSYS_INTPTR SUBSYS_INTPTR_INVALID = 0xffffffff;
	#endif
#else

#ifdef __x86_64__
typedef uint64_t SUBSYS_INTPTR;
const SUBSYS_INTPTR SUBSYS_INTPTR_INVALID = 0xffffffffffffffff;
#else
typedef uint32_t SUBSYS_INTPTR;
const SUBSYS_INTPTR SUBSYS_INTPTR_INVALID = 0xffffffff;
#endif
const uint32 DLL_PROCESS_ATTACH	= 1;
const uint32 DLL_PROCESS_DETACH	= 2;

#ifndef TRUE
#define TRUE true
#endif

#ifndef FALSE
#define FALSE false
#endif

#endif

#endif // #ifndef PLATFORM_DEFINES_H
