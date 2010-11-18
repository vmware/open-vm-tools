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
 * pseudoAppMgr.hh --
 *
 *	Manages "pseudo-applications" for special menu items, such as
 *	directories for which we wish to display custom menu item icons.
 */

#ifndef _GHI_PSEUDOAPPMGR_HH_
#define _GHI_PSEUDOAPPMGR_HH_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include <glibmm.h>
#include <tr1/unordered_map>

namespace vmware {
namespace tools {
namespace ghi {

struct PseudoApp {
   Glib::ustring uri;           // file:///home/foo/Documents
   Glib::ustring symbolicName;  // Documents or Dokumente
   Glib::ustring iconName;      // see icon-naming-spec
};

class PseudoAppMgr {
public:
   // WARNING: Don't change these values w/o visiting the assignment code in
   // pseudoAppMgr.cc.
   enum AppId {
      // Defined entirely by GHI.
      PSEUDO_APP_HOME,
      PSEUDO_APP_BOOKMARKS,
      // Based on GLib GUserDirectory.
      PSEUDO_APP_DESKTOP,
      PSEUDO_APP_DOCUMENTS,
      PSEUDO_APP_DOWNLOAD,
      PSEUDO_APP_MUSIC,
      PSEUDO_APP_PICTURES,
      // executables sans .desktop files
      PSEUDO_APP_GNOME_CONNECT,         // gnome-connect-server
      // Placeholder.
      PSEUDO_APP_NAPPS
   };

   PseudoAppMgr();

   // XXX I don't like the asymmetric return/pass-by-reference semantics.
   void GetAppByAppId(AppId id, PseudoApp& app) const;
   bool GetAppByUri(const Glib::ustring& uri, PseudoApp& app) const;
private:
   void InitAppMap();
   void InitUriVector();

   // Indexed by URI.
   static std::tr1::unordered_map<Glib::ustring,PseudoApp,
                                  std::tr1::hash<std::string> > sApps;
   // AppId => URI.
   static std::vector<Glib::ustring> sUris;
};

} /* namespace ghi */ } /* namespace tools */ } /* namespace vmware */

#endif // ifndef _GHI_PSEUDOAPPMGR_HH_
