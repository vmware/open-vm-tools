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
 * windowPathFactory.cc --
 *
 * Factory class which, given an X window ID, PID, or pathname tries to
 * find executable path and its corresponding "desktop entry" file.  (See
 * http://standards.freedesktop.org/desktop-entry-spec/latest/.)
 *
 * TODO Isolated from UnityPlatform, this class looks up X atoms directly.
 * Will need to factor atom handling out from UnityPlatform and reconcile it
 * with this class.
 */

#include <giomm/desktopappinfo.h>

#include <fstream>
#include <glibmm.h>

#include "vmware/tools/unity/windowPathFactory.hh"

extern "C" {
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>

#include <gio/gdesktopappinfo.h>

#undef Bool
#include "vmware.h"
#include "vmware/tools/utils.h"
#include "posix.h"
#include "str.h"
#include "xdg.h"
};

namespace vmware {
namespace tools {
namespace unity {


/*
 *-----------------------------------------------------------------------------
 *
 * WindowPathFactory::WindowPathFactory --
 *
 *      Constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

WindowPathFactory::WindowPathFactory(Display* dpy)
   : mDpy(dpy)
{
   /*
    * PR631378 - see
    * http://standards.freedesktop.org/menu-spec/latest/ar01s04.html#menu-file-elements
    *
    * With OpenSUSE 11.2, apps under /usr/share/applications/kde4 are referred
    * to as kde4-$app, not just $app.
    */
   mEnvPrefixes.push_back("");
   mEnvPrefixes.push_back("gnome-");
   mEnvPrefixes.push_back("kde4-");

   /*
    * There isn't always a direct correspondance between an application's executable's
    * path and its .desktop file.  For example on Ubuntu 10.10 Mozilla Firefox has a
    * firefox.desktop which launches "firefox".  However "firefox" is just a symlink to
    * a wrapper around the actual Firefox executable, firefox-bin.  It's the latter
    * which Unity/X11 will encounter and use as a starting point to find the app's
    * .desktop file.
    *
    * Below are pairs of regular expressions and candidate application names.  If an
    * executable name matches pair.first(), we'll check for a pair.second()+".desktop".
    */
   // XXX Keep this in an external file.
#define WPFADDMATCH(pattern, target)       \
   mExecPatterns.push_back(std::make_pair(Glib::Regex::create(pattern), target))
   WPFADDMATCH("acroread$", "AdobeReader");
   WPFADDMATCH("firefox(-bin|$)", "firefox");
   WPFADDMATCH("firefox(-bin|$)", "mozilla-firefox");
   WPFADDMATCH("thunderbird(-bin|$)", "thunderbird");
   WPFADDMATCH("thunderbird(-bin|$)", "mozilla-thunderbird");
   WPFADDMATCH("soffice", "openoffice.org-base");
#undef WPFADDMATCH

   // XXX Keep this in an external file.
   mSkipPatterns = Glib::Regex::create("^(sh|bash)-?|(perl|python)(-|\\d|$)");
}


/*
 *-----------------------------------------------------------------------------
 *
 * WindowPathFactory::FindByXid --
 *
 *      Search for executable and desktop entry based on an X11 window ID.
 *
 * Results:
 *      Returns true and populates pathPair on success.
 *      Returns false on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
WindowPathFactory::FindByXid(XID window,               // IN
                             WindowPathPair& pathPair) // OUT
{
   bool success = false;
   int argc;
   char** argv;
   bool triedLeader = false;
   Window checkWindow = window;

tryLeader:
   /*
    * We examine WM_COMMAND before checking argv because kdeinit has a tendency
    * to rewrite /proc/$pid/cmdline as "kdeinit4: foo [kdeinit] bar baz".  Even
    * though it's deprecated, WM_COMMAND is widely available and specifies a
    * command vector suitable for launching an application from scratch.
    */
   if (XGetCommand(mDpy, checkWindow, &argv, &argc)) {
      std::vector<Glib::ustring> vec;
      for (int i = 0; i < argc; i++) {
         vec.push_back(argv[i]);
      }
      success = FindByArgv("" /* without a PID, cwd is unavailable */, vec,
                           pathPair);
      XFreeStringList(argv);
   }

   pid_t windowPid;
   if (   !success
       && (windowPid = GetPidForXid(checkWindow)) != -1) {
      success = FindByPid(windowPid, pathPair);
   }

   if (!success && !triedLeader) {
      /*
       * Last ditch - look for a client leader window and try all of the above
       * again.
       */
      checkWindow = LookupClientLeader(window);
      if (checkWindow != None) {
         triedLeader = TRUE;
         goto tryLeader;
      }
   }

   return success;
}


/*
 ******************************************************************************
 * BEGIN Private member functions.
 */


/*
 *-----------------------------------------------------------------------------
 *
 * WindowPathFactory::CanonicalizeAppName --
 *
 *      Turns the app name (or path) into a full path for the executable.
 *
 * Results:
 *      Returns string of normalized path.
 *
 * Side effects:
 *      None.
 *
 * TODO:
 *      Deprecate AppUtil_CanonicalizeAppName.
 *
 *-----------------------------------------------------------------------------
 */

Glib::ustring
WindowPathFactory::CanonicalizeAppName(const Glib::ustring& appName, // IN
                                       const Glib::ustring& cwd)     // IN
{
   if (appName.empty()) {
      return Glib::ustring();
   } else if (appName[0] == '/') {
      return appName;
   }

   Glib::ustring tmp = Glib::find_program_in_path(appName);
   if (!tmp.empty()) {
      return tmp;
   }

   if (!cwd.empty()) {
      ASSERT(cwd[0] == '/');

      tmp = cwd + "/" + appName;
      char *ctmp = Posix_RealPath(tmp.c_str());
      if (ctmp) {
         tmp = ctmp;
         vm_free(ctmp);
         return tmp;
      }
   }

   return appName;
}


/*
 *-----------------------------------------------------------------------------
 *
 * WindowPathFactory::FindByArgv --
 *
 *      Given an argument vector, guess which desktop entry fits best.
 *
 * Results:
 *      Returns true and populates at least pathPair.first (executable path) on
 *      success.  Ideally will populate pathPair.second (desktop entry path),
 *      too.
 *      Returns false with pathPair unmodified on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
WindowPathFactory::FindByArgv(const Glib::ustring& cwd,        // IN
                              std::vector<Glib::ustring> argv, // IN
                              WindowPathPair& pathPair)        // OUT
{
   std::vector<Glib::ustring>::iterator arg;

   /* Skip language interpreters. */
   for (arg = argv.begin(); arg != argv.end(); ++arg) {
      if (!IsSkippable(*arg)) {
         break;
      }
   }

   if (arg == argv.end()) {
      g_debug("%s: all args determined skippable.\n", __FUNCTION__);
      return false;
   }

   /* Examine our cache first. */
   ExecMap::const_iterator entry = mExecMap.find(*arg);
   if (entry != mExecMap.end()) {
      pathPair = entry->second;
      return true;
   }

   /* Given a presumable argv[0] and cwd, record the likely executable name. */
   pathPair.first = CanonicalizeAppName(*arg, cwd);
   pathPair.second = "";

   /*
    * Okay, arg may be absolute or relative.
    *
    * We'll search for a matching .desktop entry using the following methods:
    *
    * 1.  Take the basename of arg and search for an application identified by
    *     $arg.desktop.
    * 2.  Consult static list of known applications and guess at possible
    *     launchers.  (firefox-bin => firefox, soffice.bin => ooffice.)
    */

   Glib::ustring testString = Glib::path_get_basename(*arg);

   std::vector<std::string> candidates;
   candidates.push_back(testString);

   for (std::vector<ExecPattern>::iterator pattern = mExecPatterns.begin();
        pattern != mExecPatterns.end();
        ++pattern) {
      if (pattern->first->match(testString)) {
         candidates.push_back(pattern->second);
      }
   }

   for (std::vector<std::string>::iterator candidate = candidates.begin();
        candidate != candidates.end();
        ++candidate)
   {
      for (std::vector<std::string>::iterator i = mEnvPrefixes.begin();
           i != mEnvPrefixes.end();
           ++i) {
         /* Try for a DesktopAppInfo identified by arg. */
         Glib::ustring desktopId =
            Glib::ustring::compose("%1%2.desktop", *i, *candidate);
         Glib::RefPtr<Gio::DesktopAppInfo> desktopApp =
            Gio::DesktopAppInfo::create(desktopId);
         if (desktopApp) {
            GDesktopAppInfo* gobj = desktopApp->gobj();
            pathPair.second = g_desktop_app_info_get_filename(gobj);
            mExecMap[*arg] = pathPair;
            return true;
         }
      }
   }

   /* Cache negative results. */
   mExecMap[*arg] = pathPair;
   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * WindowPathFactory::FindByPid --
 *
 *      Examine /proc/pid to find clues linking a process to a desktop entry.
 *
 * Results:
 *      Returns true and populates desktopEntry on success.
 *      Returns false on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
WindowPathFactory::FindByPid(pid_t pid,                // IN
                             WindowPathPair& pathPair) // OUT
{
   /*
    * - Extract cwd, argv from /proc/pid/{cwd,cmdline}.
    * - Pass down to FindByArgv.
    */

   char cbuf[256];
   Str_Snprintf(cbuf, sizeof cbuf, "/proc/%d/cwd", pid);

   char cwdbuf[PATH_MAX];
   int i = readlink(cbuf, cwdbuf, sizeof cwdbuf);
   if (i <= 0) {
      /* Lookup of cwd failed.  We'll try our best without it. */
      i = 0;
   }
   cwdbuf[i] = '\0';

   Str_Snprintf(cbuf, sizeof cbuf, "/proc/%d/cmdline", pid);

   try {
      std::vector<Glib::ustring> argv;
      std::ifstream ifstr(cbuf);
      std::string arg;

      while (ifstr.good()) {
         getline(ifstr, arg, '\0');
         if (!arg.empty()) {
            argv.push_back(arg);
         }
      }

      if (!argv.empty()) {
         return FindByArgv(Glib::ustring(cwdbuf), argv, pathPair);
      }
   } catch (std::exception& e) {
      g_warning("%s: Exception while parsing /proc/%d/cmdline: %s\n",
                __FUNCTION__, pid, e.what());
   }

   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * WindowPathFactory::GetPidForXid --
 *
 *      Given a window ID, query its _NET_WM_PID property to obtain its owning
 *      PID.
 *
 * Results:
 *      Returns a valid PID on success and -1 on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

pid_t
WindowPathFactory::GetPidForXid(XID window)
{
   pid_t windowPid = -1;

   Atom pidAtom = XInternAtom(mDpy, "_NET_WM_PID", True);
   Atom propertyType;

   int ret;
   int propertyFormat;
   unsigned long itemsReturned = 0;
   unsigned long bytesRemaining;
   unsigned char *valueReturned = NULL;
   ret = XGetWindowProperty(mDpy, window, pidAtom, 0 /* offset */, 1024, False,
                            AnyPropertyType, &propertyType, &propertyFormat,
                            &itemsReturned, &bytesRemaining, &valueReturned);
   if (ret != Success) {
      return false;
   }

   if (propertyType == XA_CARDINAL && itemsReturned >= 1) {
      switch (propertyFormat) {
      case 16:
         windowPid = *(CARD16*)valueReturned;
         break;
      case 32:
         windowPid = *(XID*)valueReturned;
         break;
      default:
         g_warning("%s: Unknown propertyFormat %d while retrieving _NET_WM_PID\n",
                   __FUNCTION__, propertyFormat);
         break;
      }
   }
   XFree(valueReturned);

   return windowPid;
}


/*
 *-----------------------------------------------------------------------------
 *
 * WindowPathFactory::IsSkippable --
 *
 *      Primitive filter which returns true if an argument is a language
 *      interpreter or similar executable not interesting to Unity/GHI.
 *
 *      XXX This function was cribbed from AppUtil_IsAppSkippable.  That function
 *      and this one should eventually go away.
 *
 * Results:
 *      Returns true if component is skippable, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
WindowPathFactory::IsSkippable(const Glib::ustring& component) // IN
{
   Glib::ustring ctmp = Glib::path_get_basename(component);
   return mSkipPatterns->match(ctmp);
}


/*
 *-----------------------------------------------------------------------------
 *
 * WindowPathFactory::LookupClientLeader --
 *
 *      Given a window ID, look up the associated "client leader" window,
 *      identified by the WM_CLIENT_LEADER property, if it exists.
 *
 * Results:
 *      A valid window ID if found, otherwise None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

XID
WindowPathFactory::LookupClientLeader(XID window)  // IN
{
   Atom propertyType;
   int propertyFormat;
   unsigned long itemsReturned;
   unsigned long bytesRemaining;
   unsigned char *valueReturned = NULL;

   Atom findAtom = XInternAtom(mDpy, "WM_CLIENT_LEADER", True);
   Window leaderWindow = None;

   if ((XGetWindowProperty(mDpy, window, findAtom, 0, 4, False, XA_WINDOW,
                           &propertyType, &propertyFormat, &itemsReturned,
                           &bytesRemaining, &valueReturned) == Success) &&
       propertyFormat == 32 && itemsReturned == 1) {
      leaderWindow = *(XID *)valueReturned;
   }

   XFree(valueReturned);

   return leaderWindow;
}

/*
 * END Private members functions.
 ******************************************************************************
 */

} /* namespace unity */ } /*namespace tools */ } /* namespace vmware */
