/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/


/*
 * stubs.c --
 *
 *      Common stubs.
 */

#include <stdarg.h>

#include "os.h"

/*
 *----------------------------------------------------------------------------
 *
 * Panic --
 *
 *    Panic implementation.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
Panic(const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   os_panic(fmt, args);
   va_end(args);
}
