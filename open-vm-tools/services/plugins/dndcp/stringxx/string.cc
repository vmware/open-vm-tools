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
 * string.cc --
 *
 *     A string wrapper for bora/lib/unicode. This class is intended to provide
 *     more c++ features such as operator overloading, automatic string conversion
 *     between different types of string classes.
 */


#include <sstream>
#include <iostream>

#include "autoCPtr.hh"
#include "string.hh"
#include "unicode.h"
#include "util.h"


namespace utf {

/*
 * Initialize static scope variables,
 *
 * Note that with the way this is done, it's important not to delay load glib
 * libraries. See bug 397373 for more details. If you're getting crazy values
 * for utf::string::npos, check your linker flags.
 */
const string::size_type string::npos = Glib::ustring::npos;


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::string --
 *
 *      Constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::string()
   : mUstr(),
     mUtf16Cache(NULL),
     mUtf16Length(npos)
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::string --
 *
 *      Constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::string(const char *s) // IN
   : mUstr(),
     mUtf16Cache(NULL),
     mUtf16Length(npos)
{
   // If the input is NULL, then there's nothing to do.
   if (UNLIKELY(s == NULL)) {
      return;
   }

   mUstr = s;
   ASSERT(Validate(mUstr));
}


#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::string --
 *
 *      Constructor from a ubstr_t object.  Copies the UTF-16 representation of
 *      the ubstr_t.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Makes a copy of the ubstr_t data and frees that data when the
 *      utf::string is destroyed.
 *
 * Note:
 *      WIN32 only call
 *
 *-----------------------------------------------------------------------------
 */

string::string(const ubstr_t &s) // IN
   : mUstr(),
     mUtf16Cache(NULL),
     mUtf16Length(npos)
{
   // If the input is empty, then there's nothing to do.
   if (s.length() == 0) {
      return;
   }

   mUstr = static_cast<const char *>(s);
   ASSERT(Validate(mUstr));
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::string --
 *
 *      Constructor from a _bstr_t object.  Copies the UTF-16 representation of
 *      the _bstr_t.  Needed for dealing with _com_error::Description().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Makes a copy of the _bstr_t data and frees that data when
 *      the utf::string is destroyed.
 *
 * Note:
 *      WIN32 only call
 *
 *-----------------------------------------------------------------------------
 */

string::string(const _bstr_t &s) // IN
   : mUstr(),
     mUtf16Cache(NULL),
     mUtf16Length(npos)
{
   // If the input is empty, then there's nothing to do.
   if (s.length() == 0) {
      return;
   }

   mUstr = AutoCPtr<char>(
      Unicode_AllocWithUTF16(static_cast<const utf16_t *>(s)),
      free).get();
   ASSERT(Validate(mUstr));
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::string --
 *
 *      Constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::string(const utf16string &s) // IN
   : mUstr(),
     mUtf16Cache(NULL),
     mUtf16Length(npos)
{
   // If the input is empty, then there's nothing to do.
   if (s.empty()) {
      return;
   }

   string copy(s.c_str());
   swap(copy);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::string --
 *
 *      Constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::string(const utf16_t *s) // IN
   : mUstr(),
     mUtf16Cache(NULL),
     mUtf16Length(npos)
{
   // If the input is NULL, then there's nothing to do.
   if (UNLIKELY(s == NULL)) {
      return;
   }

   /*
    * Since we already have a UTF-16 representation of the string, copy it
    * now.
    */
   mUtf16Cache = Unicode_UTF16Strdup(s);

   mUstr = AutoCPtr<char>(Unicode_AllocWithUTF16(s),
                          free).get();
   ASSERT(Validate(mUstr));
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::string --
 *
 *      Constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::string(const char *s,           // IN
               StringEncoding encoding) // IN
   : mUstr(),
     mUtf16Cache(NULL),
     mUtf16Length(npos)
{
   // If the input is NULL, then there's nothing to do.
   if (UNLIKELY(s == NULL)) {
      return;
   }

   mUstr = AutoCPtr<char>(Unicode_Alloc(s, encoding),
                          free).get();
   ASSERT(Validate(mUstr));
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::string --
 *
 *      Constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::string(const Glib::ustring& s) // IN
   : mUstr(s),
     mUtf16Cache(NULL),
     mUtf16Length(npos)
{
   ASSERT(Validate(s));
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::string --
 *
 *      Copy constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::string(const string& s) // IN
   : mUstr(s.mUstr),
     mUtf16Cache(NULL),
     mUtf16Length(npos)
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::~string --
 *
 *      Destructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

string::~string()
{
   InvalidateCache();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator Glib::ustring --
 *
 *      Implicit conversion to Glib::ustring operator
 *
 * Results:
 *      The internal Glib::ustring object.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

string::operator const Glib::ustring& ()
   const
{
   return ustr();
}


#ifdef _WIN32

/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator ubstr_t --
 *
 *      Implicit conversion to ubstr_t
 *
 * Results:
 *      The current ubstr_t string. NUL-terminated.
 *
 * Side effects:
 *      None
 *
 * Note:
 *      This function is only defined in _WIN32
 *
 *-----------------------------------------------------------------------------
 */

string::operator const ubstr_t()
   const
{
   return ubstr_t(GetUtf16Cache());
}

#endif


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator= --
 *
 *      Assignment operator.
 *
 * Results:
 *      A reference to this string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string&
string::operator=(string copy) // IN
{
   swap(copy);
   return *this;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator+= --
 *
 *      Append operator of the utf::string class.
 *
 * Results:
 *      A reference to this string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string&
string::operator+=(const string &s) // IN
{
   return append(s);
}


string&
string::operator+=(value_type uc) // IN
{
   push_back(uc);
   return *this;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::swap --
 *
 *      Swaps the contents with a given utf::string.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
string::swap(string& s) // IN/OUT
{
   using std::swap;
   mUstr.swap(s.mUstr);
   swap(mUtf16Cache, s.mUtf16Cache);
   swap(mUtf16Length, s.mUtf16Length);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::resize --
 *
 *      Change the size of this utf::string.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
string::resize(size_type n,  // IN
               value_type c) // IN/OPT
{
   InvalidateCache();
   mUstr.resize(n, c);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::reserve --
 *
 *      Change the amount of memory allocated for the utf::string.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
string::reserve(size_type n) // IN/OPT
{
   mUstr.reserve(n);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::c_str --
 *
 *      Get the UTF-8 representation of this string.
 *
 * Results:
 *      The current string with UTF-8 encoding. NUL-terminated.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

const char *
string::c_str()
   const
{
   return mUstr.c_str();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::w_str --
 *
 *      Get the UTF-16 representation of this string.
 *
 * Results:
 *      The current string with UTF-16 (host-endian) encoding. NUL-terminated.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

const utf16_t *
string::w_str()
   const
{
   return GetUtf16Cache();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::ustr --
 *
 *      Get the Glib::ustring backing of this string.
 *
 * Results:
 *      The internal Glib::ustring object.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

const Glib::ustring&
string::ustr()
   const
{
   return mUstr;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::empty --
 *
 *      Test if this is an empty string.
 *
 * Results:
 *      true if it's an empty string, otherwise false.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
string::empty()
   const
{
   return mUstr.empty();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::size --
 *
 * Results:
 *      Returns the length of this string, in characters (code points),
 *      excluding NUL.
 *      If length in bytes is wanted, please refer to bytes() method.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::size()
   const
{
   return mUstr.size();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::w_size --
 *
 * Results:
 *      Returns the length of this string, in UTF-16 code units,
 *      excluding NUL.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::w_size()
   const
{
   if (mUtf16Length == npos) {
      mUtf16Length = Unicode_UTF16Strlen(GetUtf16Cache());
   }

   return mUtf16Length;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::length --
 *
 * Results:
 *      Returns the length of this string, in characters (code points),
 *      excluding NUL. (Same as size().)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::length()
   const
{
   return size();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::bytes --
 *
 * Results:
 *      Returns the number of bytes used by the UTF-8 representation of this
 *      string, excluding NUL.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::bytes()
   const
{
   return mUstr.bytes();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::foldCase --
 *
 *      Returns the case-folded string of this string.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::foldCase()
   const
{
   return string(mUstr.casefold());
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::trim --
 *
 *      Returns the whitespace-trimmed version of this string.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::trim()
   const
{
   return CopyAndFree(Unicode_Trim(c_str()), free);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::trimLeft --
 *
 *      Get the left-trimmed version of this string.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::trimLeft()
   const
{
   return CopyAndFree(Unicode_TrimLeft(c_str()), free);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::trimRight --
 *
 *      Get the right-trimmed version of this string.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::trimRight()
   const
{
   return CopyAndFree(Unicode_TrimRight(c_str()), free);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::normalize --
 *
 *      Creates a new string by normalizing the input string.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::normalize(NormalizeMode mode) // IN
   const
{
   return mUstr.normalize((Glib::NormalizeMode)mode);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::toLower --
 *
 *      Creates a new string by lower-casing the input string using
 *      the rules of the specified locale.  If no locale is specified,
 *      uses the process's default locale.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::toLower(const char *locale) // IN/OPT
   const
{
#ifdef USE_ICU
   return CopyAndFree(Unicode_ToLower(c_str(), locale), free);
#else
   return mUstr.lowercase();
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::toUpper --
 *
 *      Creates a new string by upper-casing the input string using
 *      the rules of the specified locale.  If no locale is specified,
 *      uses the process's default locale.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::toUpper(const char *locale) // IN/OPT
   const
{
#ifdef USE_ICU
   return CopyAndFree(Unicode_ToUpper(c_str(), locale), free);
#else
   return mUstr.uppercase();
#endif
}


#ifdef USE_ICU
/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::toTitle --
 *
 *      Creates a new string by title-casing the input string using
 *      the rules of the specified locale.  If no locale is specified,
 *      uses the process's default locale.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::toTitle(const char *locale) // IN/OPT
   const
{
   return CopyAndFree(Unicode_ToTitle(c_str(), locale), free);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::append --
 *
 *      Appends the argument string to this utf::string.
 *
 * Results:
 *      A reference to this object.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string&
string::append(const string &s) // IN
{
   InvalidateCache();
   mUstr.append(s.mUstr);

   return *this;
}


string&
string::append(const string &s, // IN
               size_type i,     // IN
               size_type n)     // IN
{
   InvalidateCache();
   mUstr.append(s.mUstr, i, n);

   return *this;
}


string&
string::append(const char *s,   // IN
               size_type n)     // IN
{
   InvalidateCache();
   mUstr.append(s, n);

   return *this;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::push_back --
 *
 *      Appends the character at the end of this string.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
string::push_back(value_type uc) // IN
{
   InvalidateCache();
   mUstr.push_back(uc);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::assign --
 *
 *      Assigns the passed in string to this string.
 *
 *      Callers should prefer using operator= instead of assign().
 *
 * Results:
 *      A reference to this object
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string&
string::assign(const string &s) // IN
{
   return operator=(s);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::insert --
 *
 *      Inserts the argument string to this string at index i, return this
 *      string.
 *
 *      These are passthrough calls to the Glib::insert calls.
 *
 * Results:
 *      A reference to this object
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string&
string::insert(size_type i,      // IN
               const string& s)  // IN
{
   InvalidateCache();
   mUstr.insert(i, s.mUstr);
   return *this;
}


string&
string::insert(size_type i,      // IN
               size_type n,      // IN
               value_type uc)
{
   InvalidateCache();
   mUstr.insert(i, n, uc);
   return *this;
}


string&
string::insert(iterator p,    // IN
               value_type uc) // IN
{
   InvalidateCache();
   mUstr.insert(p, uc);
   return *this;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::clear --
 *
 *      Clears this string.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
string::clear()
{
   InvalidateCache();
   mUstr.clear();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::erase --
 *
 *      Erase the contents of this string in the specified index range.
 *
 * Results:
 *      A reference to this object
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string&
string::erase(size_type i,    // IN
              size_type n)    // IN
{
   InvalidateCache();
   mUstr.erase(i, n);
   return *this;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::erase --
 *
 *      Erase the contents of this string with given iterator.
 *
 * Results:
 *      The current iterator.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::iterator
string::erase(iterator p)    // IN
{
   InvalidateCache();
   return mUstr.erase(p);
}


string::iterator
string::erase(iterator pbegin,    // IN
              iterator pend)      // IN
{
   InvalidateCache();
   return mUstr.erase(pbegin, pend);
}

/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::replace --
 *
 *      Replace the string contents specified by the range, with the passed in
 *      string.
 *
 * Results:
 *      A reference to this object.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string&
string::replace(size_type i,     // IN
                size_type n,     // IN
                const string& s) // IN
{
   InvalidateCache();
   mUstr.replace(i, n, s.mUstr);
   return *this;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::replace --
 *
 *      Mutates this string by replacing all occurrences of one string with
 *      another.
 *
 *      Does nothing if the `from` string is empty.
 *
 * Results:
 *      A reference to this object.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string&
string::replace(const string& from, // IN
                const string& to)   // IN
{
   if (from.empty()) {
      return *this;
   }

   size_type end;
   size_type start = 0;
   size_type fromSize = from.length();
   string result;
   result.reserve(bytes() * to.bytes() / from.bytes());

   while ((end = find(from, start)) != string::npos) {
      result += substr(start, end - start);
      result += to;

      start = end + fromSize;
   }

   if (start < length()) {
      result += substr(start);
   }

   result.reserve(0);

   swap(result);
   return *this;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::replace_copy --
 *
 * Results:
 *      Returns a new string with all occurrences of one string replaced by
 *      another.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::replace_copy(const string& from, // IN
                     const string& to)   // IN
   const
{
   return string(*this).replace(from, to);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::compare --
 *
 *      A 3-way (output -1, 0, or 1) string comparison. Compares each Unicode
 *      code point of this string to the argument string.
 *
 * Results:
 *      -1 if *this < s, 0 if *this == s, 1 if *this > s.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
string::compare(const string &s, // IN
                bool ignoreCase) // IN/OPT: false by default
   const
{
   return ignoreCase
          ? Unicode_CompareIgnoreCase(c_str(), s.c_str())
          : Unicode_Compare(c_str(), s.c_str());
}


int
string::compare(size_type i,     // IN
                size_type n,     // IN
                const string &s) // IN
   const
{
   return mUstr.compare(i, n, s.mUstr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::compareLength --
 *
 *      A 3-way (output -1, 0, or 1) string comparison with given length.
 *      Compares only the first len characters (in code units) of the strings.
 *
 * Results:
 *      -1 if *this < s, 0 if *this == s, 1 if *this > s.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
string::compareLength(const string &s, // IN
                      size_type len,   // IN: length in code-point
                      bool ignoreCase) // IN/OPT: false by default
   const
{
   return substr(0, len).compare(s.substr(0, len), ignoreCase);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::compareRange --
 *
 *      A 3-way (output -1, 0, or 1) string comparison with given length.
 *      Compares the substrings from this string [thisStart ~ thisStart + thisLength-1]
 *      with the input string str [strStart ~ strStart + strLength - 1].
 *
 * Results:
 *      -1 if *this < s, 0 if *this == s, 1 if *this > s.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
string::compareRange(size_type thisStart,  // IN: index in code-point
                     size_type thisLength, // IN: length in code-point
                     const string &str,    // IN
                     size_type strStart,   // IN: index in code-point
                     size_type strLength,  // IN: length in code-point
                     bool ignoreCase)      // IN/OPT: false by default
   const
{
   return substr(thisStart, thisLength).compare(str.substr(strStart, strLength), ignoreCase);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::find --
 *
 *      Searches for the first occurrence of the input string inside this string.
 *
 * Results:
 *      If s is found, then, it returns the first starting index of the input string.
 *      Otherwise, returns npos.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::find(const string &s,   // IN
             size_type pos)     // IN/OPT
   const
{
   return mUstr.find(s.mUstr, pos);
}


string::size_type
string::find(value_type uc, // IN
             size_type pos) // IN/OPT
   const
{
   return mUstr.find(uc, pos);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::rfind --
 *
 *      Searches for the last occurrence of the input string inside this string.
 *
 * Results:
 *      If s is found, then, it returns the last starting index of the input string.
 *      Otherwise, returns npos.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::rfind(const string &s,   // IN
              size_type pos)     // IN/OPT
   const
{
   return mUstr.rfind(s.mUstr, pos);
}


string::size_type
string::rfind(value_type uc,     // IN
              size_type pos)     // IN/OPT
   const
{
   return mUstr.rfind(uc, pos);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::find_first_of --
 *
 *      Find the first occurrence of 's' in this string.  'i' determines where in
 *      the current string we start searching for 's'
 *
 * Results:
 *      If s is found, then, it returns the index where s occurs in this
 *      string.
 *      Otherwise, returns npos.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::find_first_of(const string &s, // IN
                      size_type i)     // IN/OPT
   const
{
   return mUstr.find_first_of(s.mUstr, i);
}


string::size_type
string::find_first_of(value_type uc,   // IN
                      size_type i)     // IN/OPT
   const
{
   return mUstr.find_first_of(uc, i);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::find_first_not_of --
 *
 *      Find the first occurrence of a string NOT in 's' in this string.  'i'
 *      determines where in this string we start searching to NOT 's'.
 *
 * Results:
 *      Returns the index of the first sequence in this string that is not 's'
 *      Otherwise, returns npos.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::find_first_not_of(const string &s, // IN
                          size_type i)     // IN/OPT
   const
{
   return mUstr.find_first_not_of(s.mUstr, i);
}


string::size_type
string::find_first_not_of(value_type uc,   // IN
                          size_type i)     // IN/OPT
   const
{
   return mUstr.find_first_not_of(uc, i);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::find_last_of --
 *
 *      Does a reverse search in this string for 's'.  'i' determines where we
 *      start the search for in this string.
 *
 * Results:
 *      If s is found, then, it returns the index where s occurs in this
 *      string.
 *      Otherwise, returns npos.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::find_last_of(const string &s, // IN
                     size_type i)     // IN/OPT
   const
{
   return mUstr.find_last_of(s.mUstr, i);
}


string::size_type
string::find_last_of(value_type uc,   // IN
                     size_type i)     // IN/OPT
   const
{
   return mUstr.find_last_of(uc, i);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::find_last_not_of --
 *
 *      Searches for the last character within the current string that does
 *      not match any characters in 's'.  'i' determines where we start the
 *      search for in this string.  (moving backwards).
 *
 * Results:
 *      If NOT 's' is found, then, it returns the index where s does not occurs
 *      in this string.
 *      Otherwise, returns npos.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::size_type
string::find_last_not_of(const string &s, // IN
                         size_type i)     // IN/OPT
   const
{
   return mUstr.find_last_not_of(s.mUstr, i);
}


string::size_type
string::find_last_not_of(value_type uc,   // IN
                         size_type i)     // IN/OPT
   const
{
   return mUstr.find_last_not_of(uc, i);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::substr --
 *
 *      Create a substring of this string with given range.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::substr(size_type start, // IN
               size_type len)   // IN: The substring length in code points.
   const
{
   return string(mUstr.substr(start, len));
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator[] --
 *
 *      Get the UTF-32 character at given index in this string.
 *
 * Results:
 *      UTF-32 character (gunichar).
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::value_type
string::operator[](size_type i)  // IN
   const
{
   return mUstr[i];
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::startsWith --
 *
 *      Tests if the current string starts with 's'
 *
 * Results:
 *      true if current string starts with 's', false otherwise
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
string::startsWith(const string &s, // IN
                   bool ignoreCase) // IN/OPT: false by default
   const
{
   return UnicodeStartsWith(c_str(), s.c_str(), ignoreCase);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::endsWith --
 *
 *      Tests if the current string ends with 's'
 *
 * Results:
 *      true if current string ends with 's', false otherwise
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
string::endsWith(const string &s, // IN
                 bool ignoreCase) // IN/OPT: false by default
   const
{
   return UnicodeEndsWith(c_str(), s.c_str(), ignoreCase);
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::split --
 *
 *      Return a vector of utf::strings.  The vector contains the elements of
 *      the string split by the passed in separator. Empty tokens are not
 *      skipped. If maxStrings is zero, any number of strings will be returned,
 *      otherwise parsing stops after maxStrings - 1 matches of the separator.
 *      In that case, the last string returned includes the rest of the
 *      original string.
 *
 *      "1,2,3".split(",") -> ["1", "2", "3"]
 *      "1,,".split(",") -> ["1", "", ""]
 *      "1".split(",") -> ["1"]
 *      "1,2,3".split(",", 2) -> ["1", "2,3"]
 *
 *      XXX If this is to be used for things like command line parsing, support
 *      for quoted strings needs to be added.
 *
 * Results:
 *      A vector of utf::strings
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

std::vector<string>
string::split(const string &sep, // IN
              size_t maxStrings) // IN/OPT
   const
{
   std::vector<string> splitStrings;
   size_type sIndex = 0;
   size_type sepLen = sep.length();
   size_t count = 0;

   ASSERT(sepLen > 0);

   while (true) {
      size_type index = find(sep, sIndex);
      count++;
      if (count == maxStrings || index == npos) {
         splitStrings.push_back(substr(sIndex));
         break;
      }

      splitStrings.push_back(substr(sIndex, index - sIndex));
      sIndex = index + sepLen;
   }

   return splitStrings;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::GetUtf16Cache --
 *
 *      Return the UTF-16 representation of the current string, this value is
 *      cached, in the object.  If the cache is not valid (NULL), then create
 *      the cache value
 *
 * Results:
 *      A UTF-16 representation of the current string
 *
 * Side effects:
 *      Allocates a UTF16 string
 *
 *-----------------------------------------------------------------------------
 */

const utf16_t *
string::GetUtf16Cache()
   const
{
   if (mUtf16Cache == NULL) {
      mUtf16Cache = Unicode_GetAllocUTF16(c_str());
   }

   return mUtf16Cache;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::InvalidateCache --
 *
 *      Frees the cache in this string.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
string::InvalidateCache()
{
   free(mUtf16Cache);
   mUtf16Cache = NULL;
   mUtf16Length = npos;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator+ --
 *
 *      Create a new string by appending the input string to this string.
 *
 *      NOTE: This is not the same as append.  append() will modify the
 *      current object, while this will return a new object.
 *
 * Results:
 *      The newly created string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
string::operator+(const string &rhs)     // IN
   const
{
   return mUstr + rhs.mUstr;
}


string
string::operator+(value_type uc)        // IN
   const
{
   return mUstr + uc;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator== --
 *
 *      Equality operator for string objects
 *
 * Results:
 *      true or false (true if equal)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
string::operator==(const string &rhs)     // IN
   const
{
   return compare(rhs) == 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator!= --
 *
 *      Inequality operator for string objects
 *
 * Results:
 *      true or false (true if not equal)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
string::operator!=(const string &rhs)     // IN
   const
{
   return compare(rhs) != 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator< --
 *
 *      Less than operator for string objects
 *
 * Results:
 *      true or false (true if lhs is < rhs)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
string::operator<(const string &rhs)     // IN
   const
{
   return compare(rhs) < 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator> --
 *
 *      Greater than operator for string objects
 *
 * Results:
 *      true or false (true if lhs is > rhs)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
string::operator>(const string &rhs)     // IN
   const
{
   return compare(rhs) > 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator<= --
 *
 *      Less than or equal than operator for string objects
 *
 * Results:
 *      true or false (true if lhs is <= rhs)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
string::operator<=(const string &rhs)     // IN
   const
{
   return compare(rhs) <= 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::operator>= --
 *
 *      Greater than or equal than operator for string objects
 *
 * Results:
 *      true or false (true if lhs is >= rhs)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
string::operator>=(const string &rhs)     // IN
   const
{
   return compare(rhs) >= 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::begin --
 *
 *      Returns an iterator to the start of the string.
 *
 * Results:
 *      iterator
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::iterator
string::begin()
{
   return mUstr.begin();
}


string::const_iterator
string::begin()
   const
{
   return mUstr.begin();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::string::end --
 *
 *      Returns an iterator to the end of the string.
 *
 * Results:
 *      iterator
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string::iterator
string::end()
{
   return mUstr.end();
}


string::const_iterator
string::end()
   const
{
   return mUstr.end();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::Validate --
 *
 *      Validates the string.
 *
 * Results:
 *      true if the string contains is valid UTF-8, false otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
Validate(const Glib::ustring& s) // IN
{
   bool isValid = Unicode_IsBufferValid(s.c_str(), s.bytes(),
                                        STRING_ENCODING_UTF8);
   if (!isValid) {
      char *escaped = Unicode_EscapeBuffer(s.c_str(), s.bytes(),
                                           STRING_ENCODING_UTF8);
      Warning("Invalid UTF-8 string: \"%s\"\n", escaped);
      free(escaped);
   }
   return isValid;
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::CreateWithLength --
 *
 *      A wrapper function for Unicode_AllocWithLength() that returns a utf::string.
 *
 * Results:
 *      A utf::string created with given parameters.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
CreateWithLength(const void *buffer,      // IN
                 ssize_t lengthInBytes,   // IN: NUL not included
                 StringEncoding encoding) // IN
{
   if (!Unicode_IsBufferValid(buffer, lengthInBytes, encoding)) {
      throw ConversionError();
   }

   return CopyAndFree(Unicode_AllocWithLength(buffer, lengthInBytes, encoding),
                      free);
}


/*
 *----------------------------------------------------------------------
 *
 * utf::CreateWithBOMBuffer --
 *
 *       Convert a text buffer with BOM (byte-order mark) to utf::string.
 *       If BOM not present, assume it's UTF-8.
 *
 * Results:
 *       A utf::string containing the text buffer.
 *
 * Side Effect:
 *       None.
 *
 *----------------------------------------------------------------------
 */

string
CreateWithBOMBuffer(const void *buffer,      // IN
                    ssize_t lengthInBytes)   // IN: NUL not included
{
   struct BOMMap {
      uint8 bom[4];              // BOM with max size.
      ssize_t len;               // Length of BOM.
      StringEncoding encoding;   // Encoding if a BOM is present.
   };

   static const BOMMap mapBOM[] = {
      {{0}, 0,                      STRING_ENCODING_UTF8     }, // Default encoding.
      {{0xEF, 0xBB, 0xBF}, 3,       STRING_ENCODING_UTF8     },
      {{0xFE, 0xFF}, 2,             STRING_ENCODING_UTF16_BE },
      {{0xFF, 0xFE}, 2,             STRING_ENCODING_UTF16_LE },
      {{0x00, 0x00, 0xFE, 0xFF}, 4, STRING_ENCODING_UTF32_BE },
      {{0xFF, 0xFE, 0x00, 0x00}, 4, STRING_ENCODING_UTF32_LE }
   };

   ASSERT(lengthInBytes >= 0);
   unsigned int index = 0; // Default encoding, no need to check.
   for (unsigned int i = 1; i < ARRAYSIZE(mapBOM); i++) {
      if (   lengthInBytes >= mapBOM[i].len
          && memcmp(mapBOM[i].bom, buffer, mapBOM[i].len) == 0) {
         index = i;
         break;
      }
   }

   return CreateWithLength(reinterpret_cast<const char*>(buffer) + mapBOM[index].len,
                           lengthInBytes - mapBOM[index].len,
                           mapBOM[index].encoding);
}


/*
 *----------------------------------------------------------------------
 *
 * utf::CopyAndFree --
 *
 *       Creates a utf::string from an allocated UTF-8 C string,
 *       automatically freeing it afterward.
 *
 * Results:
 *       A copy of the string.
 *
 * Side Effect:
 *       None.
 *
 *----------------------------------------------------------------------
 */

string
CopyAndFree(char* utf8,              // IN
            void (*freeFunc)(void*)) // IN/OPT
{
   ASSERT(utf8 != NULL);
   return AutoCPtr<char>(utf8, freeFunc).get();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::IntToStr --
 *
 *      Converts an integer to a utf::string.
 *
 * Results:
 *      A utf::string created with the given integer.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

string
IntToStr(int64 val) // IN
{
   std::ostringstream ostream;
   ostream << val;
   return ostream.str().c_str();
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::CopyArray --
 *
 *      Copies an array to a vector.
 *      Guaranteed to not shrink the vector.
 *
 * Results:
 *      A vector containing a shallow copy of the array.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

template<typename T>
static void
CopyArray(const T *p,          // IN:
          size_t n,            // IN: The number of array elements to copy.
          std::vector<T>& buf) // OUT:
{
   if (n > buf.size()) {
      buf.resize(n);
   }

   if (!buf.empty()) {
      ASSERT(p != NULL);
      memcpy(&buf[0], p, n * sizeof buf[0]);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * utf::CreateWritableBuffer --
 *
 *      Copies a utf::string to a writable buffer.
 *      Guaranteed to never shrink the size of the destination buffer.
 *
 * Results:
 *      A std::vector containing the NUL-terminated string data.
 *      The size of the resulting vector may exceed the length of the
 *      NUL-terminated string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
CreateWritableBuffer(const string& s,        // IN:
                     std::vector<char>& buf) // OUT: A copy of the string, as UTF-8.
{
   CopyArray(s.c_str(), s.bytes() + 1, buf);
}


void
CreateWritableBuffer(const string& s,           // IN:
                     std::vector<utf16_t>& buf) // OUT: A copy of the string, as UTF-16.
{
   CopyArray(s.w_str(), s.w_size() + 1, buf);
}


} // namespace utf
