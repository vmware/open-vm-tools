/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * toolsLoggerInt.h --
 *
 *    All-purpose logging facility. This header file contains 
 *    declarations that are intended just to be seen internally.
 *
 */


#ifndef __TOOLSLOGGERINT_H__
#   define __TOOLSLOGGERINT_H__


/*
 * Log sinks
 */
typedef enum {
   TOOLSLOG_SINK_FILE,
   TOOLSLOG_SINK_CONSOLE,
   TOOLSLOG_SINK_SYSLOG,
   TOOLSLOG_SINK_HOST,
   TOOLSLOG_SINK_STDERR,

   TOOLSLOG_SINK_LAST   /* Must be the last one */
} ToolsLogSink;


/*
 * Tools log file name 
 */
#define CONFNAME_LOGFILE                          "log.file"
#define CONFVAL_LOGFILE_DEFAULT                   "vmware-tools.log"


#endif /* __TOOLSLOGGERINT_H__ */
