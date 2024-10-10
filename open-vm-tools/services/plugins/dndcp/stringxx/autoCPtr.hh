/*********************************************************
 * Copyright (c) 2014-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

#ifndef AUTOCPTR_HH
#define AUTOCPTR_HH

#include <memory>


/*
 *-----------------------------------------------------------------------------
 *
 * auto_unique --
 *
 *      A helper function to create and return std::unique_ptr objects with
 *      deduced types.
 *
 * Returns:
 *      Returns the constructed std::unique_ptr.
 *
 * Usage:
 *      auto foo = auto_unique(AllocateFoo(), DeleteFoo);
 *
 *-----------------------------------------------------------------------------
 */

template<typename T, typename Deleter = std::default_delete<T>>
std::unique_ptr<T, Deleter>
auto_unique(T* p,                        // IN
            Deleter deleter = Deleter()) // IN/OPT
{
   return {p, deleter};
}


#endif // AUTOCPTR_HH
