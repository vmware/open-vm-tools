/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
 * ubstr_t.hh --
 *
 *     A string wrapper for _bstr_t.  _bstr_t assumes all char* strings use
 *     the local MBCS encoding, but we want to require that char* strings be
 *     interpreted as UTF-8.
 */

#ifndef _WIN32
#error This file should be built on Windows only.
#endif

#pragma once

#include <algorithm>
#include <comutil.h>
#include <glibmm/refptr.h>

#include "autoCPtr.hh"

#include <unicode.h>
#include <util.h>


#ifdef I_LOVE_BSTR_T_CLASSIC

typedef _bstr_t ubstr_t;
typedef _variant_t uvariant_t;

#else

class ubstr_t
{
public:
   ubstr_t();
   ubstr_t(const char *s);
   ubstr_t(const wchar_t *s);
   ubstr_t(const _variant_t& var);
   explicit ubstr_t(const _bstr_t& bstr);
   ubstr_t(BSTR bstr, bool copy);
   ubstr_t(const ubstr_t& s);

   ubstr_t& operator=(ubstr_t copy);
   ubstr_t& operator=(const char *s);
   ubstr_t& operator=(const wchar_t *s);
   ubstr_t& operator=(const _variant_t& var);

   // Wrappers around standard _bstr_t methods.
   void Assign(BSTR s);
   BSTR copy(bool copy = true) const;

   void Attach(BSTR s);
   BSTR Detach();

   BSTR *GetAddress();
   BSTR& GetBSTR();

   unsigned int length() const;

   ubstr_t& operator+=(const ubstr_t& s);
   ubstr_t operator+(const ubstr_t& s) const;
   ubstr_t operator+(const char *s) const;
   ubstr_t operator+(const wchar_t *s) const;

   bool operator!() const;
   bool operator==(const ubstr_t& s) const;
   bool operator!=(const ubstr_t& s) const;
   bool operator<(const ubstr_t& s) const;
   bool operator>(const ubstr_t& s) const;
   bool operator<=(const ubstr_t& s) const;
   bool operator>=(const ubstr_t& s) const;

   operator const wchar_t*() const;
   operator wchar_t*() const; // XXX: Get rid of this.
   operator const char*() const;
#ifndef I_HATE_NON_CONST_BSTR_T_CASTS
   operator char*() const; // XXX: Get rid of this.
#endif

   /*
    * _variant_t is implicitly constructible from a _bstr_t.  Since we can't
    * add a constructor to _variant_t, instead provide an implicit conversion
    * from ubstr_t to _variant_t.
    */
   operator _variant_t() const;

   void swap(ubstr_t& s);

private:
   class UTF8Data
   {
   public:
      // Takes ownership of the input string.
      UTF8Data(char *utf8String = NULL) // IN/OUT: May be NULL
         : mUTF8String(utf8String),
           mRefCount(1)
      {
      }

      // For Glib::RefPtr.
      void reference()
      {
         ++mRefCount;
      }

      // For Glib::RefPtr.
      void unreference()
      {
         --mRefCount;
         if (mRefCount == 0) {
            delete this;
         }
      }

      // Takes ownership of the input string.
      void Set(char *utf8String) // IN/OUT: May be NULL.
      {
         if (mUTF8String == utf8String) {
            return;
         }
         free(mUTF8String);
         mUTF8String = utf8String;
      }

      char *Get()
      {
         return mUTF8String;
      }

   private:
      // Only destructible via unreference().
      ~UTF8Data()
      {
         free(mUTF8String);
      }

      char *mUTF8String;
      unsigned int mRefCount;

   private:
      // Intentionally unimplemented.
      UTF8Data(const UTF8Data&);
      UTF8Data& operator=(const UTF8Data&);
   };

   char *GetUTF8Cache() const;
   void InvalidateCache();

   /*
    * Anything that mutates mBstr (all non-const methods) must call
    * InvalidateCache().
    */
   _bstr_t mBstr;

   // mUTF8 is allocated and initialized lazily.
   mutable Glib::RefPtr<UTF8Data> mUTF8;
};


/*
 * _variant_t does string conversions too, so we also need to wrap that.
 * Since _variant_t doesn't keep a cached version of the locally-encoded
 * MBCS string and since we don't use _variant_t nearly as much as _bstr_t,
 * substitutability isn't as much of a concern, so we can use inheritance.
 */
class uvariant_t
   : public _variant_t
{
public:
   /*
    * Wrappers around standard _variant_t constructors.  Unfortunately we
    * have to wrap all the constructors since they aren't inherited.
    */
   uvariant_t() : _variant_t() { }
   uvariant_t(const VARIANT& var) : _variant_t(var) { }
   uvariant_t(const VARIANT *var) : _variant_t(var) { }
   uvariant_t(const _variant_t& var) : _variant_t(var) { }
   uvariant_t(VARIANT& var, bool copy) : _variant_t(var, copy) { }
   uvariant_t(short i, VARTYPE vt = VT_I2) : _variant_t(i, vt) { }
   uvariant_t(long i, VARTYPE vt = VT_I4) : _variant_t(i, vt) { }
   uvariant_t(float f) : _variant_t(f) { }
   uvariant_t(double d, VARTYPE vt = VT_R8) : _variant_t(d, vt) { }
   uvariant_t(const CY& cy) : _variant_t(cy) { }
   uvariant_t(const _bstr_t& s) : _variant_t(s) { }
   uvariant_t(const wchar_t *s) : _variant_t(s) { }
   uvariant_t(IDispatch *dispatch, bool addRef = true) : _variant_t(dispatch, addRef) { }
   uvariant_t(bool b) : _variant_t(b) { }
   uvariant_t(IUnknown *unknown, bool addRef = true) : _variant_t(unknown, addRef) { }
   uvariant_t(const DECIMAL& dec) : _variant_t(dec) { }
   uvariant_t(BYTE i) : _variant_t(i) { }
   uvariant_t(char c) : _variant_t(c) { }
   uvariant_t(unsigned short i) : _variant_t(i) { }
   uvariant_t(unsigned long i) : _variant_t(i) { }
   uvariant_t(int i) : _variant_t(i) { }
   uvariant_t(unsigned int i) : _variant_t(i) { }
#if _WIN32_WINNT >= 0x0501
   uvariant_t(__int64 i) : _variant_t(i) { }
   uvariant_t(unsigned __int64 i) : _variant_t(i) { }
#endif

   // Override the _variant_t constructor that assumes locally-encoded strings.
   uvariant_t(const char *s) : _variant_t(ubstr_t(s)) { }

   // Provide conversion from our ubstr_t wrapper.
   uvariant_t(const ubstr_t& s) : _variant_t(s) { }
};


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::ubstr_t --
 *
 *      ubstr_t constructors.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline
ubstr_t::ubstr_t()
   : mBstr(),
     mUTF8()
{
}


inline
ubstr_t::ubstr_t(const char *s) // IN: A UTF-8-encoded string.
   : mBstr(),
     mUTF8()
{
   if (s != NULL) {
      // Since we already have the UTF-8 version of the string, cache it now.
      mUTF8 = Glib::RefPtr<UTF8Data>(new UTF8Data(Util_SafeStrdup(s)));
      mBstr = AutoCPtr<utf16_t>(Unicode_GetAllocUTF16(s), free).get();
   }
}


inline
ubstr_t::ubstr_t(const wchar_t *s) // IN
   : mBstr(s),
     mUTF8()
{
}


inline
ubstr_t::ubstr_t(const _variant_t& var) // IN
   : mBstr(var),
     mUTF8()
{
}


inline
ubstr_t::ubstr_t(const _bstr_t& bstr) // IN
   : mBstr(bstr),
     mUTF8()
{
}


inline
ubstr_t::ubstr_t(BSTR bstr, // IN
                 bool copy) // IN
   : mBstr(bstr, copy),
     mUTF8()
{
}


inline
ubstr_t::ubstr_t(const ubstr_t& s) // IN
   : mBstr(s.mBstr),
     mUTF8(s.mUTF8)
{
   if (static_cast<wchar_t *>(mBstr) != NULL && !mUTF8) {
      mUTF8 = s.mUTF8 = Glib::RefPtr<UTF8Data>(new UTF8Data());
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::operator= --
 *
 *      ubstr_t assignment operators.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline ubstr_t&
ubstr_t::operator=(ubstr_t copy) // IN
{
   swap(copy);
   return *this;
}


inline ubstr_t&
ubstr_t::operator=(const char *s) // IN: A UTF-8-encoded string.
{
   return operator=(ubstr_t(s));
}


inline ubstr_t&
ubstr_t::operator=(const wchar_t *s) // IN
{
   return operator=(ubstr_t(s));
}


inline ubstr_t&
ubstr_t::operator=(const _variant_t& var) // IN
{
   return operator=(ubstr_t(var));
}



/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::Assign --
 *
 *      Wrapper around _bstr_t::Assign.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline void
ubstr_t::Assign(BSTR s) // IN
{
   InvalidateCache();
   mBstr.Assign(s);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::copy --
 *
 *      Wrapper around _bstr_t::copy.
 *
 * Results:
 *      A copy of the underlying BSTR.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline BSTR
ubstr_t::copy(bool copy) // IN/OPT
   const
{
   return mBstr.copy(copy);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::Attach --
 *
 *      Wrapper around _bstr_t::Attach.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Invalidates the UTF-8 cache.
 *
 *-----------------------------------------------------------------------------
 */

inline void
ubstr_t::Attach(BSTR s) // IN
{
   InvalidateCache();
   mBstr.Attach(s);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::Detach --
 *
 *      Wrapper around _bstr_t::Detach.
 *
 * Results:
 *      The underlying BSTR, which is no longer managed.
 *
 * Side effects:
 *      Invalidates the UTF-8 cache.
 *
 *-----------------------------------------------------------------------------
 */

inline BSTR
ubstr_t::Detach()
{
   InvalidateCache();
   return mBstr.Detach();
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::GetAddress --
 *
 *      Wrapper around _bstr_t::GetAddress.
 *
 * Results:
 *      A pointer to the underlying BSTR.
 *
 * Side effects:
 *      Invalidates the UTF-8 cache.
 *
 *-----------------------------------------------------------------------------
 */

inline BSTR *
ubstr_t::GetAddress()
{
   /*
    * We don't know if the underlying BSTR will be modified via the returned
    * pointer.  We can only assume it will.
    */
   InvalidateCache();
   return mBstr.GetAddress();
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::GetBSTR --
 *
 *      Wrapper around _bstr_t::GetBSTR.
 *
 * Results:
 *      A reference to the underlying BSTR.
 *
 * Side effects:
 *      Invalidates the UTF-8 cache.
 *
 *-----------------------------------------------------------------------------
 */

inline BSTR&
ubstr_t::GetBSTR()
{
   /*
    * We don't know if the underlying BSTR will be modified via the returned
    * reference.  We can only assume it will.
    */
   InvalidateCache();
   return mBstr.GetBSTR();
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::length --
 *
 *      Wrapper around _bstr_t::length.
 *
 * Results:
 *      The length of the string, in TCHARs.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline unsigned int
ubstr_t::length()
   const
{
   return mBstr.length();
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::operator+= --
 *
 *      Mutating concatenation operator.
 *
 * Results:
 *      A reference to this ubstr_t object.
 *
 * Side effects:
 *      Invalidates the UTF-8 cache.
 *
 *-----------------------------------------------------------------------------
 */

inline ubstr_t&
ubstr_t::operator+=(const ubstr_t& s) // IN
{
   InvalidateCache();
   mBstr += s.mBstr;
   return *this;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::operator+ --
 *
 *      Concatenation operators.
 *
 * Results:
 *      A copy of the concatenated string.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline ubstr_t
ubstr_t::operator+(const ubstr_t& s) // IN
   const
{
   ubstr_t copy(*this);
   copy += s;
   return copy;
}


inline ubstr_t
ubstr_t::operator+(const char *s) // IN
   const
{
   return operator+(ubstr_t(s));
}


inline ubstr_t
ubstr_t::operator+(const wchar_t *s) // IN
   const
{
   return operator+(ubstr_t(s));
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::operator! --
 *
 *      Wrapper around _bstr_t::operator!.
 *
 * Results:
 *      true if the underlying _bstr_t is NULL, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline bool
ubstr_t::operator!()
   const
{
   return !mBstr;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::operator== --
 * ubstr_t::operator!= --
 * ubstr_t::operator< --
 * ubstr_t::operator> --
 * ubstr_t::operator<= --
 * ubstr_t::operator>= --
 *
 *      Wrappers around _bstr_t's comparison operators.
 *
 * Results:
 *      true if the comparisons are hold, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline bool
ubstr_t::operator==(const ubstr_t& s) // IN
   const
{
   return mBstr == s.mBstr;
}


inline bool
ubstr_t::operator!=(const ubstr_t& s) // IN
   const
{
   return mBstr != s.mBstr;
}


inline bool
ubstr_t::operator<(const ubstr_t& s) // IN
   const
{
   return mBstr < s.mBstr;
}


inline bool
ubstr_t::operator>(const ubstr_t& s) // IN
   const
{
   return mBstr > s.mBstr;
}


inline bool
ubstr_t::operator<=(const ubstr_t& s) // IN
   const
{
   return mBstr <= s.mBstr;
}


inline bool
ubstr_t::operator>=(const ubstr_t& s) // IN
   const
{
   return mBstr >= s.mBstr;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::operator const wchar_t* --
 * ubstr_t::operator wchar_t* --
 *
 *      Wrappers around _bstr_t's cast operators.
 *
 * Results:
 *      A pointer to the underlying UTF-16 string.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline
ubstr_t::operator const wchar_t*()
   const
{
   return static_cast<const wchar_t *>(mBstr);
}


inline
ubstr_t::operator wchar_t*()
   const
{
   return static_cast<wchar_t *>(mBstr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::operator const char_t* --
 * ubstr_t::operator char_t* --
 *
 *      Cast operators to UTF-8 strings.
 *
 * Results:
 *      A pointer to a UTF-8 string.
 *
 * Side effects:
 *      Might initializes the UTF-8 cache.
 *
 *-----------------------------------------------------------------------------
 */

inline
ubstr_t::operator const char*()
   const
{
   return GetUTF8Cache();
}


#ifndef I_HATE_NON_CONST_BSTR_T_CASTS
inline
ubstr_t::operator char*()
   const
{
   return GetUTF8Cache();
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::operator _variant_t --
 *
 *      Cast operator to _variant_t.
 *
 * Results:
 *      A _variant_t equivalent to this ubstr_t object.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline
ubstr_t::operator _variant_t()
   const
{
   return static_cast<const wchar_t *>(mBstr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::swap --
 *
 *      Swaps this ubstr_t object with another.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline void
ubstr_t::swap(ubstr_t& s) // IN/OUT
{
   std::swap(mBstr, s.mBstr);
   mUTF8.swap(s.mUTF8);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::GetUTF8Cache --
 *
 *      Retrieves the UTF-8 cache, initializing it if necessary.
 *
 * Results:
 *      The cached UTF-8 string.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline char *
ubstr_t::GetUTF8Cache()
   const
{
   if (static_cast<wchar_t *>(mBstr) == NULL) {
      // Nothing to do.
      return NULL;
   }

   if (!mUTF8) {
      mUTF8 = Glib::RefPtr<UTF8Data>(new UTF8Data());
   }

   if (mUTF8->Get() == NULL) {
      AutoCPtr<char> utf8Str(
         Unicode_AllocWithUTF16(static_cast<wchar_t *>(mBstr)),
         free);
      mUTF8->Set(utf8Str.get());
      utf8Str.release();
   }

   return mUTF8->Get();
}


/*
 *-----------------------------------------------------------------------------
 *
 * ubstr_t::InvalidateCache --
 *
 *      Clears the UTF-8 cache.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline void
ubstr_t::InvalidateCache()
{
   mUTF8.clear();
}


/*
 *-----------------------------------------------------------------------------
 *
 * operator+ --
 *
 *      Non-member concatenation operators.
 *
 * Results:
 *      A copy of the concatenated string.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

inline ubstr_t
operator+(const char *s1,    // IN: A UTF-8-encoded sting.
          const ubstr_t& s2) // IN
{
   return ubstr_t(s1) + s2;
}


inline ubstr_t
operator+(const wchar_t *s1, // IN
          const ubstr_t& s2) // IN
{
   return ubstr_t(s1) + s2;
}


/*
 * These are not part of the _bstr_t interface but are provided to catch
 * misuse.  They are intentionally unimplemented to avoid a maintenance
 * burden, and they intentionally return nothing so that accidental usage
 * normally results in compile-time errors instead of link-time ones.
 */

void operator==(const char *s1, const ubstr_t& s2);
void operator!=(const char *s1, const ubstr_t& s2);
void operator<(const char *s1, const ubstr_t& s2);
void operator>(const char *s1, const ubstr_t& s2);
void operator<=(const char *s1, const ubstr_t& s2);
void operator>=(const char *s1, const ubstr_t& s2);

void operator==(const WCHAR *s1, const ubstr_t& s2);
void operator!=(const WCHAR *s1, const ubstr_t& s2);
void operator<(const WCHAR *s1, const ubstr_t& s2);
void operator>(const WCHAR *s1, const ubstr_t& s2);
void operator<=(const WCHAR *s1, const ubstr_t& s2);
void operator>=(const WCHAR *s1, const ubstr_t& s2);


#endif // I_LOVE_BSTR_T_CLASSIC
