/*********************************************************
 * Copyright (C) 2013-2017,2021 VMware, Inc. All rights reserved.
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
 * tracer.hh --
 *
 *    A dumb object to trace function enter/exit.  (Devel-only.)
 */


#ifndef TRACER_HH
#define TRACER_HH

#include "vm_basic_defs.h"

#include "glib.h"


#ifdef VMX86_DEVEL
#   define TRACE_CALL()    Tracer _fn_tracer (__FUNCTION__)
class Tracer {
public:
   Tracer(const char* fnName)
      : mFnName(fnName)
   {
      g_debug("> %s: enter\n", mFnName);
   }

   ~Tracer()
   {
      g_debug("< %s: exit\n", mFnName);
   }

private:
   Tracer();                    // = delete
   Tracer(const Tracer&);       // = delete

   const char* mFnName;
};
#else
#   define TRACE_CALL()
#endif

#endif // ifndef TRACER_HH
