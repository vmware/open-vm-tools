/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

/*
 * stats_user_setup.h --
 *
 *      The machinery to define statcounters for user at userlevel.  This is
 *      something of a collapsing of stats_setup.h and genstats_setup.h
 *      combined with simplifying to eliminate the monitor entanglements.
 *      The goals are to be simpler to read than the monitor statcounter
 *      code and also (relatedly) easier to use in libs.
 *
 *      Expects at least one of the following:
 *      - SETUP_DECLARE_VARS: Declares the enum for the statcounters (this
 *        is what you need in order to STAT_INC() / DEC() / etc.
 *      - SETUP_DEFINE_VARS: Declares the actual StatsUserBlock with all the
 *        information-- stat names, storage for the counters, etc.
 *
 *      Orthogonally, one can also supply:
 *      - SETUP_WANT_GETVAL: If the includer wants to also {declare, define}
 *        a function to retrieve named stat counter values.
 */

/*
 * This file can be included more than once and should clean up its own
 * defines before finishing each invocation.
 */

#define STAT_NAME_PREFIX STATS
#define STAT_VAR_PREFIX stats

#ifndef STATS_MODULE
#error "stats_user_setup.h must be included with STATS_MODULE defined"
#endif

#if !defined(STATS_COUNTERS_FILE) && !defined(STATS_COUNTERS_INLINE) && \
    !defined(STATS_COUNTERS_NONE)
#error "stats_user_setup.h must be included with statcounters defined"
#endif

#include "vm_basic_defs.h"

#ifdef SETUP_DECLARE_VARS
   #define STAT(_name, _ignore, _explanation) STATS_USER_NAME(_name),
   #define STAT_INST(_name, _ignore, _explanation)
   enum {
      #ifdef STATS_COUNTERS_FILE
      #include STATS_COUNTERS_FILE
      #endif
      #ifdef STATS_COUNTERS_INLINE
      STATS_COUNTERS_INLINE
      #endif
      STAT(Last, unused, "So we always know the number of counters")
   };
   #undef STAT
   #undef STAT_INST

   #define STAT(_name, _ignore, _explanation)
   #define STAT_INST(_name, _ignore, _explanation) STATS_USER_INST_NAME(_name),
   enum {
      #ifdef STATS_COUNTERS_FILE
      #include STATS_COUNTERS_FILE
      #endif
      #ifdef STATS_COUNTERS_INLINE
      STATS_COUNTERS_INLINE
      #endif
      STAT_INST(Last, unused, "So we always know the number of counters")
   };

   EXTERN StatsUserBlock STATS_USER_BLKVAR;
   EXTERN void STATS_USER_LOG_FN(STATS_MODULE)(unsigned int epoch,
                                         void (*LogFunc)(const char *fmt, ...));

   #ifdef SETUP_WANT_GETVAL
   EXTERN Bool STATS_USER_GETVAL_FN(STATS_MODULE)(const char *name,
                                                  uint32 *val);
   #endif /* SETUP_WANT_GETVAL */

   #undef STAT
   #undef STAT_INST
#endif


#ifdef SETUP_DEFINE_VARS
   /*
    * Build a table of counter names so we can log them.
    */
   #define STAT(_name, _ignore, _explanation) XSTR(XXCONC(STATS_MODULE, XCONC(_,_name))),
   #define STAT_INST(_name, _ignore, _explanation)
   #define STATS_USER_STR_TABLE       XCONC(STAT_USER_VAR_PREFIX, StrTable)
   static const char *STATS_USER_STR_TABLE[] = {
      #ifdef STATS_COUNTERS_FILE
      #include STATS_COUNTERS_FILE
      #endif
      #ifdef STATS_COUNTERS_INLINE
      STATS_COUNTERS_INLINE
      #endif
      XSTR(XCONC(STATS_MODULE, _Last))
   };
   #undef STAT
   #undef STAT_INST

   #define STAT(_name, _ignore, _explanation)
   #define STAT_INST(_name, _ignore, _explanation) XSTR(_name),
   #define STATS_USER_INST_STR_TABLE       XCONC(STAT_USER_VAR_PREFIX, InstStrTable)
   static const char *STATS_USER_INST_STR_TABLE[] = {
      #ifdef STATS_COUNTERS_FILE
      #include STATS_COUNTERS_FILE
      #endif
      #ifdef STATS_COUNTERS_INLINE
      STATS_COUNTERS_INLINE
      #endif
      XSTR(XCONC(STATS_MODULE, _Last))
   };
   #undef STAT
   #undef STAT_INST

   /*
    * Define the StatsUserBlock itself for this module.
    */
   StatsUserBlock STATS_USER_BLKVAR;


   /*
    *----------------------------------------------------------------------
    *
    * STATS_USER_LOG_FN --
    *
    *   For logging purposes, we auto-generate a (non-INLINE) function to
    *   iterate over all the counters and dump them.  This seems nicer than
    *   making all clients cookie-cutter this code or else link against an
    *   external binary to get this functionality.
    *
    * Results:
    *   void
    *
    * Side Effects:
    *   Spewification
    *----------------------------------------------------------------------
    */

   void
   STATS_USER_LOG_FN(STATS_MODULE)(unsigned int epoch,
                                   void (*LogFunc)(const char *fmt, ...))
   {
      StatsUserBlock *cur;
      unsigned int i;

      if (!STATS_IS_INITIALIZED()) {
         return;
      }

      for (i = 0; i < STATS_USER_BLKVAR.size; i++) {
         if (STATS_USER_BLKVAR.counters[i].count > 0) {
            LogFunc("STAT %u %-26s %10d\n", epoch,
                STATS_USER_STR_TABLE[i], STATS_USER_BLKVAR.counters[i].count);
         }
      }
      for (cur = STATS_USER_BLKVAR.next; cur != NULL; cur = cur->next) {
         for (i = 0; i < cur->size; i++) {
            if (cur->counters[i].count > 0) {
               LogFunc("STATINST %u %s:%-20s %-15s %10d\n",
                   epoch, XSTR(STATS_MODULE), cur->name,
                   STATS_USER_INST_STR_TABLE[i], cur->counters[i].count);
            }
         }
      }
   }

   #ifdef SETUP_WANT_GETVAL
   /*
    *----------------------------------------------------------------------
    *
    * STATS_USER_GETVAL_FN --
    *
    *      Retrieves the value of a named user stat counter.  Returns
    *      TRUE iff NAME is a recognized user stat counter, and sets
    *      *VAL to the current value of that counter.
    *
    *      This is an optional function.  If a library needs it, use
    *      SETUP_WANT_GETVAL (see top of the header).
    *
    * Results:
    *      See above.
    *
    * Side effects:
    *      None.
    *
    *----------------------------------------------------------------------
    */

   Bool
   STATS_USER_GETVAL_FN(STATS_MODULE)(const char *name,  // IN: counter name
                                      uint32 *val)       // OUT: counter val
   {
      unsigned int i;

      if (!STATS_IS_INITIALIZED()) {
         return FALSE;
      }

      for (i = 0; i < STATS_USER_BLKVAR.size; i++) {
         if (strcmp(STATS_USER_STR_TABLE[i], name) == 0) {
            *val = STATS_USER_BLKVAR.counters[i].count;
            return TRUE;
         }
      }
      return FALSE;
   }
   #endif /* SETUP_WANT_GETVAL */

   #undef STATS_USER_STR_TABLE
   #undef STATS_USER_INST_STR_TABLE
#endif

#if defined(SETUP_DECLARE_VARS) || defined(SETUP_DEFINE_VARS)
   #include "util.h"

   /*
    * Initializes Stats at user-level. The user-level variables statsCount,
    * and statsInfoTable are defined by including "stats_user_setup.h" with
    * the appropriate parameters.
    */
   #define STATS_USER_INIT_MODULE()                                        \
   do {                                                                    \
      STATS_USER_BLKVAR.counters =                                         \
         Util_SafeCalloc(STATS_USER_NAME(Last), sizeof(StatsUserEntry));   \
      STATS_USER_BLKVAR.size = STATS_USER_NAME(Last);                      \
      STATS_USER_BLKVAR.name = XSTR(STATS_MODULE);                         \
   } while (0)

   #define STATS_USER_INIT_MODULE_ONCE()                                   \
   do {                                                                    \
      if (!STATS_IS_INITIALIZED()) {                                       \
         STATS_USER_INIT_MODULE();                                         \
      }                                                                    \
   } while (0)

   #define STATS_USER_EXIT_MODULE()                                        \
   do {                                                                    \
      free(STATS_USER_BLKVAR.counters);                                    \
      STATS_USER_BLKVAR.counters = NULL;                                   \
   } while (0)


   #ifndef STATS_USER_INIT_INST
   /*
    *----------------------------------------------------------------------
    *
    * STATS_USER_INIT_INST --
    *
    *      Stats Instancing:  Some stats are by their nature per-adapter /
    *      per-handle, etc. so we allow the code to dynamically create extra
    *      statcounters and we return a pointer that can be used when setting
    *      them later.  We keep all the instances in a list hanging off
    *      STATS_USER_BLKVAR so we can enumerate all of them at logging time.
    *
    * Results:
    *      The new instance of stats.
    *
    * Side Effects:
    *      Some memory allocation
    *----------------------------------------------------------------------
    */

   static INLINE StatsUserBlock *
   STATS_USER_INIT_INST_FN(STATS_MODULE)(const char *instanceName)
   {
      StatsUserBlock *e;
      if (STATS_USER_BLKVAR.next == NULL) {
         e = STATS_USER_BLKVAR.next = Util_SafeCalloc(1, sizeof *e);
      } else {
         StatsUserBlock *cur = STATS_USER_BLKVAR.next;
         for (; cur->next != NULL; cur = cur->next) {
            if (strcmp(instanceName, cur->name) == 0) { return cur; }
         }
         if (strcmp(instanceName, cur->name) == 0) { return cur; }
         cur->next = e = Util_SafeCalloc(1, sizeof *e);
      }
      e->size = STATS_USER_INST_NAME(Last);
      e->counters = Util_SafeCalloc(e->size, sizeof(StatsUserEntry));
      if (e->name == NULL) { e->name = strdup(instanceName); }

      return e;
   }

   #define STATS_USER_INIT_INST(name) \
        STATS_USER_INIT_INST_FN(STATS_MODULE)(name)
   #endif
#endif

#undef SETUP_DECLARE_VARS
#undef SETUP_DEFINE_VARS
#undef STAT_NAME_PREFIX
#undef STAT_VAR_PREFIX
