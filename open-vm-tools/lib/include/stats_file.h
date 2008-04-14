/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * stats_file.h --
 *
 *       Implements statistics counters for lib/file/.
 *
 */

#ifndef STATS_FILE_H
#define STATS_FILE_H

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#define STATS_MODULE file
#include "stats_user_defs.h"

#define SETUP_DECLARE_VARS
#ifdef VMX86_STATS
#define STATS_COUNTERS_INLINE \
 STAT_INST(NumReads, STATS_MODULE, "# Calls to FileIO_Read()")                 \
 STAT_INST(NumReadvs, STATS_MODULE, "# Calls to FileIO_Readv()")               \
 STAT_INST(NumPreadvs, STATS_MODULE, "# Calls to FileIO_Preadv()")             \
 STAT_INST(BytesRead, STATS_MODULE, "# Bytes requested of FileIO_Read()")      \
 STAT_INST(BytesReadv, STATS_MODULE, "# Bytes requested of FileIO_Readv()")    \
 STAT_INST(BytesPreadv, STATS_MODULE, "# Bytes requested of FileIO_Preadv()")  \
                                                                               \
 STAT_INST(NumWrites, STATS_MODULE, "# Calls to FileIO_Write()")               \
 STAT_INST(NumWritevs, STATS_MODULE, "# Calls to FileIO_Writev()")             \
 STAT_INST(NumPwritevs, STATS_MODULE, "# Calls to FileIO_Pwritev()")           \
 STAT_INST(BytesWritten, STATS_MODULE, "# Bytes requested of FileIO_Write()")  \
 STAT_INST(BytesWritev, STATS_MODULE, "# Bytes requested of FileIO_Writev()")  \
 STAT_INST(BytesPwritev, STATS_MODULE, "# Bytes requested of FileIO_Pwritev()")

#else
 /* No statcounter in non-STATS builds */
#define STATS_COUNTERS_NONE
#endif
#include "stats_user_setup.h"

#endif
