/*********************************************************
 * Copyright (c) 2025 Broadcom. All Rights Reserved.
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

#ifndef XUTILS_XUTILS_HH
#define XUTILS_XUTILS_HH

#include <gdkmm.h>
#include <gtkmm.h>

#include "stringxx/string.hh"


namespace xutils {
/* General property helpers */
bool GetCardinal(Glib::RefPtr<const Gdk::Surface> surface,
                 const utf::string& atomName,
                 unsigned long& retValue);
bool GetCardinalList(Glib::RefPtr<const Gdk::Surface> surface,
                     const utf::string& atomName,
                     std::vector<unsigned long>& retValues);
} // namespace xutils

#endif // XUTILS_XUTILS_HH
