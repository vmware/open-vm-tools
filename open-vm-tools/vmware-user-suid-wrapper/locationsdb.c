/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * locationsdb.c --
 *
 *    Provides the QueryLocationsDB routine for finding keys in the locations
 *    database.  Because our application is a setuid binary and we want to
 *    minimize risk, we retain the duplicated functionality here rather than
 *    link against lib/unixinstall.
 */

#if !defined(sun) && !defined(__FreeBSD__) && !defined(linux)
# error This program is not supported on your platform.
#endif

#include <sys/param.h>

#include <fcntl.h>
#include <string.h>
#include <strings.h>

#include "vm_basic_types.h"
#include "wrapper.h"



/*
 * Local data
 */

/*
 * Mappings between queries and search strings
 */

typedef struct Mapping {
   const char *answer;          /* string to match for "answer FOO" */
   const char *remove;          /* string to match for "remove_answer FOO" */
   size_t answerSize;           /* size of answer buffer */
   size_t removeSize;           /* size of remove buffer */
} Mapping;

/*
 * queryMappings between Selector => search strings.  Used by QueryLocationsDB.
 */

#define ANSWER_LIBDIR   "answer LIBDIR"
#define REMOVE_LIBDIR   "remove_answer LIBDIR"
#define ANSWER_BINDIR   "answer BINDIR"
#define REMOVE_BINDIR   "remove_answer BINDIR"

static Mapping queryMappings[] = {
   { ANSWER_LIBDIR, REMOVE_LIBDIR, sizeof ANSWER_LIBDIR, sizeof REMOVE_LIBDIR },
   { ANSWER_BINDIR, REMOVE_BINDIR, sizeof ANSWER_BINDIR, sizeof REMOVE_BINDIR },
   { 0, 0, 0, 0 }
};


/*
 * Global functions (definitions)
 */


/*
 *----------------------------------------------------------------------------
 *
 * QueryLocationsDB --
 *
 *    Based on the caller's Selector, determines the directory selected as
 *    "LIBDIR", "BINDIR", etc. when the Tools were last configured.
 *
 * Results:
 *    TRUE on success, FALSE on failure.  queryDir is filled in on success.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
QueryLocationsDB(const char *locations, // IN : path of locations database
                 Selector selector,     // IN : DB query to search for
                 char *queryDir,        // OUT: address to write dirname to
                 size_t queryDirSize)   // IN : size of queryDir buffer
{
   FILE *file = NULL;
   char buf[MAXPATHLEN];
   Bool found = FALSE;
   Mapping *map;

   if (selector < 0 || selector >= QUERY_MAX) {
      Error("Internal logic error.  This is a bug.");
      return FALSE;
   }

   file = fopen(locations, "r");
   if (!file) {
      return FALSE;
   }

   map = &queryMappings[selector];

   /*
    * We need to inspect the entire locations database since there are both
    * "answer"s and "remove_answer"s.  We want to provide the last answer that
    * has not been removed.
    */
   while (fgets(buf, sizeof buf, file)) {
      if (strncmp(buf, map->answer, map->answerSize - 1) == 0) {
         char *newline;

         strncpy(queryDir, buf + map->answerSize, queryDirSize);
         if (queryDir[queryDirSize - 1] != '\0') {
            found = FALSE;
            continue;
         }

         /* Truncate the string at the newline character, if it's present. */
         newline = strchr(queryDir, '\n');
         if (newline && newline - queryDir < queryDirSize) {
            *newline = '\0';
         }

         found = TRUE;
      } else if (strncmp(buf, map->remove, map->removeSize - 1) == 0) {
         found = FALSE;
      }
   }

   fclose(file);
   return found;
}
