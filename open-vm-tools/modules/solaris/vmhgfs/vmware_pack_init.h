/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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

#ifndef __VMWARE_PACK_INIT_H__
#   define __VMWARE_PACK_INIT_H__


/*
 * vmware_pack_init.h --
 *
 *    Platform-independent code to make the compiler pack (i.e. have them
 *    occupy the smallest possible space) structure definitions. The following
 *    constructs are known to work --hpreg
 *
 *    #include "vmware_pack_begin.h"
 *    struct foo {
 *       ...
 *    }
 *    #include "vmware_pack_end.h"
 *    ;
 *
 *    typedef
 *    #include "vmware_pack_begin.h"
 *    struct foo {
 *       ...
 *    }
 *    #include "vmware_pack_end.h"
 *    foo;
 */


#ifdef _MSC_VER
/*
 * MSVC 6.0 emits warning 4103 when the pack push and pop pragma pairing is
 * not balanced within 1 included file. That is annoying because our scheme
 * is based on the pairing being balanced between 2 included files.
 *
 * So we disable this warning, but this is safe because the compiler will also
 * emit warning 4161 when there is more pops than pushes within 1 main
 * file --hpreg
 */

#   pragma warning(disable:4103)
#elif __GNUC__
#else
#   error Compiler packing...
#endif


#endif /* __VMWARE_PACK_INIT_H__ */
