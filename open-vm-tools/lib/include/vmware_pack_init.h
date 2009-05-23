/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*********************************************************
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
