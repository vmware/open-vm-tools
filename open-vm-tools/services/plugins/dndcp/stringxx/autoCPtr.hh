/*********************************************************
 * Copyright (c) 2014-2018 VMware, Inc. All rights reserved.
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
 * autoCPtr.hh --
 *
 *      A simple, std::auto_ptr-like class for managing memory usually
 *      allocated by C functions.
 *
 *      Unlike std::auto_ptr, allows providing a customer deleter and disallows
 *      copying.  This is basically a wanna-be std::unique_ptr for platforms
 *      that don't have C++11 available yet.
 *
 *      XXX: When everything uses C++11, this can be replaced with
 *      std::unique_ptr.
 */

#ifndef AUTOCPTR_HH
#define AUTOCPTR_HH

#include <cstdlib>
#include <utility>


template<typename T, typename FreeFunc = void (*)(void*)>
class AutoCPtr
{
private:
   typedef AutoCPtr<T, FreeFunc> SelfType;

public:
   explicit AutoCPtr(T* p = NULL,            // IN/OPT
                     FreeFunc f = std::free) // IN/OPT
      : mP(p),
        mFree(f)
   {
   }

   ~AutoCPtr() { mFree(mP); }

   void reset(T* p = NULL) // IN/OPT
   {
      if (p == mP) {
         return;
      }

      SelfType copy(mP, mFree);
      mP = p;
   }

   T* release()
   {
      T* p = mP;
      mP = NULL;
      return p;
   }

   T* get() const { return mP; }
   T* operator->() const { return mP; }
   T& operator*() const { return *mP; }

   void swap(SelfType& other) // IN/OUT
   {
      using std::swap;
      swap(mP, other.mP);
      swap(mFree, other.mFree);
   }

private:
   T* mP;
   FreeFunc mFree;

private:
   // Non-copyable.
   AutoCPtr(const SelfType&);
   SelfType& operator=(const SelfType&);
};


#endif // AUTOCPTR_HH
