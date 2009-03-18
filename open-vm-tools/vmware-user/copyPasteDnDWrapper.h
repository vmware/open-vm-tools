/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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

/**
 * @file copyPasteDnDWrapper.h
 * 
 * This singleton class implements a wrapper around various versions of
 * copy and paste.
 *
 */

#ifndef COPYPASTEDNDWRAPPER_H
#define COPYPASTEDNDWRAPPER_H

#if defined(HAVE_GTKMM)
#include "copyPasteUI.h"
#endif

class CopyPasteDnDWrapper
{
public:
   ~CopyPasteDnDWrapper();
   static CopyPasteDnDWrapper *GetInstance();
   bool Register();
   void Unregister();
   int GetVersion();
   void SetIsRegistered(bool isRegistered);
   bool IsRegistered();
   void OnReset();
   void SetBlockFd(int blockFd);
   void SetUserData(const void *userData);
private:
   /*
    * We're a singleton, so it is a compile time error to call these.
    */
   CopyPasteDnDWrapper();
   CopyPasteDnDWrapper(const CopyPasteDnDWrapper &wrapper);
   CopyPasteDnDWrapper& operator=(const CopyPasteDnDWrapper &wrapper);
private:
#if defined(HAVE_GTKMM)
   CopyPasteUI *m_copyPasteUI;
#endif
   bool m_isRegistered;
   const void *m_userData;
   int m_version;
   static CopyPasteDnDWrapper *m_instance;
   int mBlockFd;
};

#endif // COPYPASTEDNDWRAPPER_H
