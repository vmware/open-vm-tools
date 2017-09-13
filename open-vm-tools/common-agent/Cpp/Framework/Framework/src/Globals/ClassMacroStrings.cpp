/*
 *	Copyright (c) 2011 VMware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "ClassMacroStrings.h"

const char _sPRECOND_ISCONSTRUCTED[] = "Pre-condition failed because object was not constructed successfully";
const char _sPRECOND_ISINITIALIZED[] = "Pre-condition failed because object was not initialized successfully";
const char _sPRECOND_ISNOTINITIALIZED[] = "Pre-condition failed because object has already been initialized";
const char _sPRECOND_ISNOTREADONLY[] = "Pre-condition failed because the object is read-only.";

const char _sVALIDATE_STRINGEMPTY[] ="Invalid Argument because a string is empty:";
const char _sVALIDATE_STRINGPTRNULL[] ="Invalid Argument because a string pointer is null:";
const char _sVALIDATE_STRINGPTREMPTY[] ="Invalid Argument because a string pointer is empty:";
const char _sVALIDATE_PTRARRAYNULL[] ="Invalid Argument because a pointer array is null:";
const char _sVALIDATE_PTRARRAYEMPTY[] ="Invalid Argument because a pointer array is empty:";
const char _sVALIDATE_STL[] ="Invalid Argument because an STL container is empty:";
const char _sVALIDATE_STL_EMPTY[] ="Invalid Argument because an STL container is not empty:";
const char _sVALIDATE_STL_ITERATOR[] ="Invalid Argument because an STL iterator is at the end of the containter:";
const char _sVALIDATE_INTERFACE[] ="Invalid Argument because an interface is NULL:";
const char _sVALIDATE_ZERO[] ="Invalid Argument because a number is not equal to zero:";
const char _sVALIDATE_ISNOTZERO[] ="Invalid Argument because a number is equal to zero:";
const char _sVALIDATE_POSITIVE[] ="Invalid Argument because a number is less-than or equal-to zero:";
const char _sVALIDATE_NEGATIVE[] ="Invalid Argument because a number is greater-than or equal-to zero:";
const char _sVALIDATE_NONNEGATIVE[] ="Invalid Argument because a number is less-than zero:";
const char _sVALIDATE_NOTNULL[] ="Invalid Argument is NULL:";
const char _sVALIDATE_NULL[] ="Invalid Argument must be NULL:";
const char _sVALIDATE_BOOL[] ="Invalid Argument because a bool is false:";
const char _sVALIDATE_GUID[] ="Invalid Argument because a GUID is NULL:";
const char _sASSERT_FAILED[] ="Assertion failed:";
