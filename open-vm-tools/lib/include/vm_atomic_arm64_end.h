/*********************************************************
 * Copyright (C) 2017 VMware, Inc. All rights reserved.
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

/*
 * vm_atomic_arm64_end.h --
 *
 *      Undefine private macros used to implement ARM64 atomic operations.
 */

#if !defined _VMATOM_X
#   error "vm_atomic_arm64_begin.h must be included before this file!"
#endif

/* Undefine all the private macros we previously defined. */
#undef _VMATOM_SIZE_8
#undef _VMATOM_SIZE_16
#undef _VMATOM_SIZE_32
#undef _VMATOM_SIZE_64
#undef _VMATOM_X2
#undef _VMATOM_X
#undef _VMATOM_SNIPPET_R_NF
#undef _VMATOM_SNIPPET_R
#undef _VMATOM_SNIPPET_R_SC
#undef _VMATOM_SNIPPET_W_NF
#undef _VMATOM_SNIPPET_W
#undef _VMATOM_SNIPPET_W_SC
#undef _VMATOM_FENCE
#undef _VMATOM_SNIPPET_OP
#undef _VMATOM_SNIPPET_ROP
#undef _VMATOM_SNIPPET_RW
#undef _VMATOM_SNIPPET_RIFEQW
