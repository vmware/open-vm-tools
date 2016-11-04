/*********************************************************
 * Copyright (C) 2012-2016 VMware, Inc. All rights reserved.
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
#ifndef VM_COMPILATION_OPTION_H
#define VM_COMPILATION_OPTION_H

#ifdef VMX86_DEVEL
#   ifdef VMX86_DEBUG
#      define COMPILATION_OPTION "DEBUG"
#   else
#      define COMPILATION_OPTION "OPT"
#   endif
#else
#   ifdef VMX86_ALPHA
#      define COMPILATION_OPTION "ALPHA"
#   elif defined(VMX86_BETA)
#      ifdef VMX86_EXPERIMENTAL
#         define COMPILATION_OPTION "BETA-EXPERIMENTAL"
#      else
#         define COMPILATION_OPTION "BETA"
#      endif
#   elif defined(VMX86_RELEASE)
#      define COMPILATION_OPTION "Release"
#   elif defined(VMX86_OPT)
#      define COMPILATION_OPTION "OPT"
#   elif defined(VMX86_DEBUG)
#      define COMPILATION_OPTION "DEBUG"
#   elif defined(VMX86_STATS)
#      define COMPILATION_OPTION "STATS"
#   endif
#endif

#endif
