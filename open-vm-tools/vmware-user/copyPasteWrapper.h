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
 * @file copyPasteWrapper.h
 * 
 * This singleton class implements a wrapper around various versions of
 * copy and paste.
 *
 */

#ifndef COPYPASTEWRAPPER_H
#define COPYPASTEWRAPPER_H

#if defined(HAVE_GTKMM)
#include "copyPasteUI.h"
#endif

class CopyPasteWrapper
{
public:
   ~CopyPasteWrapper();
   void SetUserData(const void *userData);
   bool Register();
   void Unregister();
   static CopyPasteWrapper *GetInstance();
   int GetVersion();
   void SetIsRegistered(bool isRegistered);
   bool IsRegistered();
   void OnReset();
private:
   /*
    * We're a singleton, so it is a compile time error to call these.
    */
   CopyPasteWrapper();
   CopyPasteWrapper(const CopyPasteWrapper &wrapper);
   CopyPasteWrapper& operator=(const CopyPasteWrapper &wrapper);
private:
#if defined(HAVE_GTKMM)
   CopyPasteUI *m_copyPasteUI;
#endif
   bool m_isRegistered;
   const void *m_userData;
   int m_version;
   static CopyPasteWrapper *m_instance;
};

#endif // COPYPASTEWRAPPER_H
