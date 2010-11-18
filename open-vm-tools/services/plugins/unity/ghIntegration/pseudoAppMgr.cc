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
 * pseudoAppMgr.cc --
 *
 *      Manages "pseudo-applications" for special menu items, such as
 *      directories for which we wish to display custom menu item icons or
 *      executables which do not have regular menu items.
 *
 *      Pseudo apps are assigned well-known IDs (AppId, compile-time), and
 *      are associated with URIs at runtime.  (Example:  A user's Desktop
 *      folder has a compile-time AppId of PSEUDO_APP_DESKTOP with a likely
 *      runtime URI of $HOME/Desktop.)
 *
 *      URIs may be influenced by environment variables or simply the existence
 *      of a program in the user's search path.
 */


#include <glibmm.h>
#include <glib/gi18n.h>
#include <stdexcept>

#include "vmware/tools/ghi/pseudoAppMgr.hh"

extern "C" {
#include "vmware.h"
};

namespace vmware {
namespace tools {
namespace ghi {


/*
 * PseudoApps indexed by URI (ex: file:///home/user/Desktop).
 */
std::tr1::unordered_map<Glib::ustring,PseudoApp,
                        std::tr1::hash<std::string> > PseudoAppMgr::sApps;

/*
 * PseudoApp URIs indexed by app ID.
 */
std::vector<Glib::ustring> PseudoAppMgr::sUris;


/*
 *-----------------------------------------------------------------------------
 *
 * PseudoAppMgr::PseudoAppMgr --
 *
 *      Populates sApps.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

PseudoAppMgr::PseudoAppMgr()
{
   static bool initialized = false;

   if (!initialized) {
      InitUriVector();
      InitAppMap();
      initialized = true;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * PseudoAppMgr::GetAppByAppId --
 *
 *      Search for pseudo app by AppId.
 *
 * Results:
 *      Populates app.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
PseudoAppMgr::GetAppByAppId(AppId id,       // IN
                            PseudoApp& app) // OUT
   const
{
   if (id < PSEUDO_APP_HOME || id >= PSEUDO_APP_NAPPS) {
      throw std::logic_error("Invalid PseudoApp identifier");
   }

   app = sApps[sUris[id]];
}


/*
 *-----------------------------------------------------------------------------
 *
 * PseudoAppMgr::LookupPath --
 *
 *      Search for pseudo app by URI.
 *
 * Results:
 *      Returns true and populates app on success.
 *      Returns false on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
PseudoAppMgr::GetAppByUri(const Glib::ustring& uri, // IN
                          PseudoApp& app)           // OUT
   const
{
   if (sApps.find(uri) != sApps.end()) {
      app = sApps[uri];
      return true;
   }

   return false;
}


/*
 ******************************************************************************
 * BEGIN private methods.
 */


/*
 *-----------------------------------------------------------------------------
 *
 * PseudoAppMgr::InitAppMap --
 *
 *      Populate application map based on static data.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Populates sApps.
 *
 *-----------------------------------------------------------------------------
 */

void
PseudoAppMgr::InitAppMap()
{
   static const struct {
      const char* symbolicName;
      const char* iconName;
   } initTable[] = {
      { "Home Folder", "user-home" },           // PSEUDO_APP_HOME
      { "Bookmarks", "user-bookmarks" },        // ..._BOOKMARKS
      { "Desktop", "user-desktop" },            // ..._DESKTOP
      { "Documents", "folder" },                // ..._DOCUMENTS
      { "Download", "folder" },                 // ..._DOWNLOAD
      { "Music", "folder" },                    // ..._MUSIC
      { "Pictures", "folder" },                 // ..._PICTURES
      { "Connect to Server...", "applications-internet" }, // ..._GNOME_CONNECT
   };
   size_t idx;

   ASSERT_ON_COMPILE(ARRAYSIZE(initTable) == PSEUDO_APP_NAPPS);

   for (idx = 0; idx < ARRAYSIZE(initTable); idx++) {
      Glib::ustring uri = sUris[idx];
      if (!uri.empty()) {
         sApps[uri].uri = uri;
         /*
          * gettext lookup against xdg-user-dirs is purely opportunistic.  Standalone
          * apps (likely) won't exist there, but to keep the loop logic simple, they
          * aren't excluded from said lookup.
          */
         sApps[uri].symbolicName = g_dgettext("xdg-user-dirs",
                                              initTable[idx].symbolicName);
         sApps[uri].iconName = initTable[idx].iconName;
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * PseudoAppMgr::InitUriVector --
 *
 *      Populate sUris based on runtime environment.  See xdg-user-dirs for
 *      more details.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Populates sUris.
 *
 *-----------------------------------------------------------------------------
 */

void
PseudoAppMgr::InitUriVector()
{
   sUris.resize(PSEUDO_APP_NAPPS);

   try {
      sUris[PSEUDO_APP_HOME] = Glib::filename_to_uri(Glib::get_home_dir());

      std::vector<Glib::ustring> components;
      components.push_back(Glib::get_home_dir());
      components.push_back(".gtk-bookmarks");
      sUris[PSEUDO_APP_BOOKMARKS] =
         Glib::filename_to_uri(Glib::build_filename(components));

#define MAP_GUSER_PSEUDO(gUserDir, pAppId)  {                           \
   Glib::ustring dir = Glib::get_user_special_dir((gUserDir));          \
   if (!dir.empty()) {                                                  \
      sUris[(pAppId)] = Glib::filename_to_uri(dir);                     \
   }                                                                    \
}

      MAP_GUSER_PSEUDO(G_USER_DIRECTORY_DESKTOP, PSEUDO_APP_DESKTOP);
      MAP_GUSER_PSEUDO(G_USER_DIRECTORY_DOCUMENTS, PSEUDO_APP_DOCUMENTS);
      MAP_GUSER_PSEUDO(G_USER_DIRECTORY_DOWNLOAD, PSEUDO_APP_DOWNLOAD);
      MAP_GUSER_PSEUDO(G_USER_DIRECTORY_MUSIC, PSEUDO_APP_MUSIC);
      MAP_GUSER_PSEUDO(G_USER_DIRECTORY_PICTURES, PSEUDO_APP_PICTURES);

      Glib::ustring connectPath = Glib::find_program_in_path("nautilus-connect-server");
      if (!connectPath.empty()) {
         sUris[PSEUDO_APP_GNOME_CONNECT] = Glib::filename_to_uri(connectPath);
      }
   } catch (std::exception& e) {
      g_warning("%s: Caught exception while learning XDG directories: %s\n",
                __func__, e.what());
   }
}


/*
 * END private methods.
 ******************************************************************************
 */

} /* namespace ghi */ } /* namespace tools */ } /* namespace vmware */
