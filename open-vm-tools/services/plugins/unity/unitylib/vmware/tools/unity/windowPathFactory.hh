/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * windowPathFactory.hh --
 *
 *      Factory class which, given an X window ID, PID, or pathname tries to
 *      find executable path and its corresponding "desktop entry" file.  (See
 *      http://standards.freedesktop.org/desktop-entry-spec/latest/.)
 */

#ifndef _UNITY_WINDOWPATHFACTORY_HH_
#define _UNITY_WINDOWPATHFACTORY_HH_

#include <tr1/unordered_map>
#include <glibmm.h>
#include <utility>
#include <vector>

extern "C" {
#include <X11/Xlib.h>
}

namespace Glib {
   class ustring;
}

namespace vmware {
namespace tools {
namespace unity {

class WindowPathFactory {
public:
   // executable path, desktop entry path
   typedef std::pair<Glib::ustring, Glib::ustring> WindowPathPair;

   WindowPathFactory(Display* dpy);
   bool FindByXid(XID window, WindowPathPair& pathPair);

private:
   typedef std::tr1::unordered_map<Glib::ustring, WindowPathPair,
                                   std::tr1::hash<std::string> > ExecMap;
   typedef std::pair<Glib::RefPtr<Glib::Regex>, std::string> ExecPattern;

   Glib::ustring CanonicalizeAppName(const Glib::ustring& appName,
                                     const Glib::ustring& cwd);
   bool FindByArgv(const Glib::ustring& cwd, std::vector<Glib::ustring> argv,
                   WindowPathPair& pathPair);
   bool FindByPid(pid_t pid, WindowPathPair& pathPair);
   pid_t GetPidForXid(XID window);
   bool IsSkippable(const Glib::ustring& component);
   Window LookupClientLeader(XID window);

   Display* mDpy;
   ExecMap mExecMap;
   std::vector<std::string> mEnvPrefixes;
   std::vector<ExecPattern> mExecPatterns;
   Glib::RefPtr<Glib::Regex> mSkipPatterns;
};

} /* namespace unity */ } /* namespace tools */ } /* namespace vmware */


#endif // ifndef _UNITY_WINDOWPATHFACTORY_HH_
