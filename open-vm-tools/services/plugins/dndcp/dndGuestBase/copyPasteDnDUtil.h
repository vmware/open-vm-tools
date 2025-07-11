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

/**
 * @file copyPasteDnDUtil.h
 *
 *    Utility functions for Drag and Drop and Copy and Paste.
 *
 */

#ifndef COPYPASTE_DND_UTIL_H
#define COPYPASTE_DND_UTIL_H

#include "dndClipboard.h"
#include <vector>
#include "stringxx/string.hh"

bool LocalPrepareFileContents(const CPClipboard *clip,
                              std::vector<utf::string>& fileContentsList);

#endif
