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
 * stats_user_defs.h --
 *
 *      Defines for user statcounters.  This started life as a clone of
 *      stats_defs.h, which is too monitor entangled to easily work outside
 *      of vmx code (e.g. in lib).
 */

#ifndef STATS_USER_DEFS_H
#define STATS_USER_DEFS_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#ifndef STATS_MODULE
#error "stats_user_defs.h must be included with STATS_MODULE defined"
#endif

#define EXTERN_STAT_DECL(_name, _desc)
#define STAT_DECL(_name, _desc)

#define STATS_USER_MODULE_STR  XSTR(XCONC(STATS_, STATS_MODULE))
#define STATS_USER_NAME(name)  XCONC(STAT_, XCONC(STATS_MODULE, _##name))
#define STATS_USER_BLKVAR      XCONC(_stats_, XCONC(STATS_MODULE, _Blk))
#define STATS_USER_LOG_FN(name)XCONC(name, _StatsLog)
#define STATS_USER_INST_NAME(name) \
        XCONC(STATINST_, XCONC(STATS_MODULE, _##name))
#define STATS_USER_INIT_INST_FN(name)   XCONC(name, _InitInstance)
#define STATS_USER_GETVAL_FN(name) XCONC(name, _StatsGetVal)

typedef struct StatsUserEntry {
   uint32 count;
} StatsUserEntry;

typedef struct StatsUserBlock {
   const char *name;
   uint32 size;
   StatsUserEntry *counters;
   struct StatsUserBlock *next;
} StatsUserBlock;

EXTERN StatsUserBlock STATS_USER_BLKVAR;

#ifndef STATS_SKIP_ACCESSORS
#define STATS_IS_INITIALIZED()     (STATS_USER_BLKVAR.counters != NULL)
#define STAT_GET(stat) \
        (&(STATS_USER_BLKVAR.counters[STATS_USER_NAME(stat)]))
#define STAT_INST_GET(inst, stat) \
        (&((inst)->counters[STATS_USER_INST_NAME(stat)]))

#define STAT_SAMPLE(stat)          STAT_GET(stat)->count++
#ifdef VMX86_STATS
#  define STAT_INC(stat)          STAT_GET(stat)->count++
#  define STAT_INC_BY(stat,inc)   STAT_GET(stat)->count += (inc)
#  define STAT_DEC_BY(stat,inc)   STAT_GET(stat)->count -= (inc)
#  define STAT_DEBUG_INC(stat)    DEBUG_ONLY(STAT_INC(stat))
#  define STAT_INST_INC(i,s)          STAT_INST_GET((i), s)->count++
#  define STAT_INST_INC_BY(i,s,inc)   STAT_INST_GET((i), s)->count += (inc)
#  define STAT_INST_DEC_BY(i,s,inc)   STAT_INST_GET((i), s)->count -= (inc)
#  define STAT_INST_DEBUG_INC(i,s)    DEBUG_ONLY(STAT_INST_INC((i), s))
#else
#  define STAT_INC(stat)
#  define STAT_INC_BY(stat,inc)
#  define STAT_DEC_BY(stat,inc)
#  define STAT_DEBUG_INC(stat)
#  define STAT_INST_INC(inst,stat)
#  define STAT_INST_INC_BY(inst,stat,inc)
#  define STAT_INST_DEC_BY(inst,stat,inc)
#  define STAT_INST_DEBUG_INC(stat)
#endif
#endif /* STATS_SKIP_ACCESSORS */
#endif /* STATS_DEFS_H */
