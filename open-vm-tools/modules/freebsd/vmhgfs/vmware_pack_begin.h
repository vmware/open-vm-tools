/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vmware_pack_begin.h --
 *
 *    Begin of structure packing. See vmware_pack_init.h for details.
 *
 *    Note that we do not use the following construct in this include file,
 *    because we want to emit the code every time the file is included --hpreg
 *
 *    #ifndef foo
 *    #   define foo
 *    ...
 *    #endif
 *
 */


#include "vmware_pack_init.h"


#ifdef _MSC_VER
#   pragma pack(push, 1)
#elif __GNUC__
#else
#   error Compiler packing...
#endif
