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
 * string.hh --
 *
 *     A string wrapper for bora/lib/unicode. This class is intended to provide
 *     more c++ features such as operator overloading, automatic string conversion
 *     between different types of string classes.
 *
 *     This class uses glib::ustring as the underlying storage for its data,
 *     we chose this object because of its internal support for unicode.
 *
 */

#ifndef UTF_STRING_HH
#define UTF_STRING_HH

#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#pragma pack(push, 8)
#endif // _WIN32

/*
 * Include glib.h here as a work-around - hopefully temporary - for a
 * compilation error that would otherwise occur as a result of the
 * inclusion of <glibmm/ustring.h> on the next line.  The compilation
 * error that would otherwise occur is the result of:
 *  (1) nested includes in <glibmm/ustring.h> ultimately include a
 *      glib-related header file other than glib.h; but
 *  (2) including a glib header file other than glib.h outside
 *      of glib code is not allowed with the latest glib.
 *
 * Although including glib.h here does not actually fix the underlying
 * problem, it does turn off the complaint.  It's believed (hoped?) that
 * an upgrade of glibmm will fix the issue properly and eliminate the
 * need to include <glib.h> here.
 */
#include <glib.h>
#include <glibmm/ustring.h>

#ifdef _WIN32
#pragma pack(pop)
#endif // _WIN32

#ifdef _WIN32
#include "ubstr_t.hh"
#endif

#include <libExport.hh>

#include <unicodeTypes.h>

#ifdef _WIN32
/*
 * Disabling Windows warning 4251
 *    This is a warning msg about requiring Glib::ustring to be DLL
 *    exportable.
 */
#pragma warning(push)
#pragma warning(disable:4251)
#endif

namespace utf {


/* utf8string should be replaced with an opaque type. It is temporarily used
 * to replace the std::string in our codebase.
 */
typedef std::string utf8string;
typedef std::basic_string<utf16_t> utf16string;

class VMSTRING_EXPORT string
{
public:
   // type definitions
   typedef Glib::ustring::size_type size_type;
   typedef Glib::ustring::value_type value_type;
   typedef Glib::ustring::iterator iterator;
   typedef Glib::ustring::const_iterator const_iterator;

   // constant definitions
   static const size_type npos;

   // Normalize mode map to Glib::NormalizeMode
   typedef enum {
      NORMALIZE_DEFAULT          = Glib::NORMALIZE_DEFAULT,
      NORMALIZE_NFD              = Glib::NORMALIZE_NFD,
      NORMALIZE_DEFAULT_COMPOSE  = Glib::NORMALIZE_DEFAULT_COMPOSE,
      NORMALIZE_NFC              = Glib::NORMALIZE_NFC,
      NORMALIZE_ALL              = Glib::NORMALIZE_ALL,
      NORMALIZE_NFKD             = Glib::NORMALIZE_NFKD,
      NORMALIZE_ALL_COMPOSE      = Glib::NORMALIZE_ALL_COMPOSE,
      NORMALIZE_NFKC             = Glib::NORMALIZE_NFKC
   } NormalizeMode;

   string();
   string(const char *s);

#ifdef _WIN32
   string(const ubstr_t &s);
   explicit string(const _bstr_t &s);
#endif

   string(const utf16string &s);
   string(const utf16_t *s);
   string(const char *s, StringEncoding encoding);
   string(const Glib::ustring &s);
   string(const string &s);

   ~string();

   // Implicit conversions
   operator const Glib::ustring& () const;
#ifdef _WIN32
   operator const ubstr_t() const;
#endif

   // Conversions to other i18n types (utf8, utf16, unicode)
   const char *c_str() const;
   const utf16_t *w_str() const;
   const Glib::ustring& ustr() const;

   // Mapping functions to Glib::ustring
   void swap(string &s);
   void resize(size_type n, value_type c = '\0');
   void reserve(size_type n = 0);
   bool empty() const;
   size_type size() const;
   size_type w_size() const;
   size_type length() const;
   size_type bytes() const;
   string foldCase() const;
   string trim() const;
   string trimLeft() const;
   string trimRight() const;
   string normalize(NormalizeMode mode = NORMALIZE_DEFAULT_COMPOSE) const;

   string toLower(const char *locale = NULL) const;
   string toUpper(const char *locale = NULL) const;
#ifdef USE_ICU
   string toTitle(const char *locale = NULL) const;
#endif

   // String-level member methods.
   string& append(const string &s);
   string& append(const string &s, size_type i, size_type n);
   string& append(const char *s, size_type n);
   string& assign(const string &s);
   void push_back(value_type uc);
   void clear();
   string& insert(size_type i, const string& s);
   string& insert(size_type i, size_type n, value_type uc);
   string& insert(iterator p, value_type uc);
   string& erase(size_type i, size_type n = npos);
   iterator erase(iterator p);
   iterator erase(iterator pbegin, iterator pend);
   string& replace(size_type i, size_type n, const string& s);
   string& replace(const string& from, const string& to);
   string replace_copy(const string& from, const string& to) const;

   int compare(const string &s, bool ignoreCase = false) const;
   int compare(size_type i, size_type n, const string &s) const;
   int compareLength(const string &s, size_type len, bool ignoreCase = false) const;
   int compareRange(size_type thisStart, size_type thisLength, const string &str,
                    size_type strStart, size_type strLength,
                    bool ignoreCase = false) const;
   size_type find(const string &s, size_type pos = 0) const;
   size_type rfind(const string &s, size_type pos = npos) const;
   size_type find_first_of(const string &s, size_type i = 0) const;
   size_type find_first_not_of(const string &s, size_type i = 0) const;
   size_type find_last_of(const string &s, size_type i = npos) const;
   size_type find_last_not_of(const string &s, size_type i = npos) const;
   string substr(size_type start = 0, size_type len = npos) const;

   // Character-level member methods.
   value_type operator[](size_type i) const;
   size_type find(value_type uc, size_type pos = 0) const;
   size_type rfind(value_type uc, size_type pos = npos) const;
   size_type find_first_of(value_type uc, size_type i = 0) const;
   size_type find_first_not_of(value_type uc, size_type i = 0) const;
   size_type find_last_of(value_type uc, size_type i = npos) const;
   size_type find_last_not_of(value_type uc, size_type i = npos) const;

   // Sequence accessor.
   iterator begin();
   iterator end();
   const_iterator begin() const;
   const_iterator end() const;

   // Operator overloads
   string& operator=(string copy);
   string& operator+=(const string &s);
   string& operator+=(value_type uc);

   // Some helper functions that are nice to have
   bool startsWith(const string &s, bool ignoreCase = false) const;
   bool endsWith(const string &s, bool ignoreCase = false) const;
   std::vector<string> split(const string &sep, size_t maxStrings = 0) const;

   // Overloaded operators
   string operator+(const string &rhs) const;
   string operator+(value_type uc) const;
   bool operator==(const string &rhs) const;
   bool operator!=(const string &rhs) const;
   bool operator<(const string &rhs) const;
   bool operator>(const string &rhs) const;
   bool operator<=(const string &rhs) const;
   bool operator>=(const string &rhs) const;

private:
   // Cache operations
   void InvalidateCache();

   // Cache accessors
   const utf16_t *GetUtf16Cache() const;

   // utf::string is internally backed by Glib::ustring.
   Glib::ustring mUstr;

   // Cached representations.
   mutable utf16_t *mUtf16Cache;
   mutable size_type mUtf16Length;

   /*
    * All added members need to be initialized in all constructors and need
    * to be handled in swap().
    */
};

// Helper operators

string inline
operator+(const char *lhs, const string &rhs) {
   return string(lhs) + rhs;
}

string inline
operator+(const string& lhs, const char *rhs) {
   return lhs + string(rhs);
}

bool inline
operator==(const char *lhs, const string &rhs) {
   return string(lhs) == rhs;
}

bool inline
operator!=(const char *lhs, const string &rhs) {
   return string(lhs) != rhs;
}

bool inline
operator<(const char *lhs, const string &rhs) {
   return string(lhs) < rhs;
}

bool inline
operator>(const char *lhs, const string &rhs) {
   return string(lhs) > rhs;
}

bool inline
operator<=(const char *lhs, const string &rhs) {
   return string(lhs) <= rhs;
}

bool inline
operator>=(const char *lhs, const string &rhs) {
   return string(lhs) >= rhs;
}

// This lets a utf::string appear on the right side of a stream insertion operator.
inline std::ostream&
operator<<(std::ostream& strm, const string& s)
{
   strm << s.c_str();
   return strm;
}

inline std::wostream&
operator<<(std::wostream& strm, const string& s)
{
   strm << s.w_str();
   return strm;
}

// ConversionError class for exception

class ConversionError {};


// Helper functions

VMSTRING_EXPORT bool Validate(const Glib::ustring& s);

VMSTRING_EXPORT string
CreateWithLength(const void *buffer, ssize_t lengthInBytes, StringEncoding encoding);

VMSTRING_EXPORT string
CreateWithBOMBuffer(const void *buffer, ssize_t lengthInBytes);

VMSTRING_EXPORT string CopyAndFree(char* utf8, void (*freeFunc)(void*) = free);

VMSTRING_EXPORT string IntToStr(int64 val);

VMSTRING_EXPORT void CreateWritableBuffer(const string& s,
                                          std::vector<char>& buf);
VMSTRING_EXPORT void CreateWritableBuffer(const string& s,
                                          std::vector<utf16_t>& buf);


} // namespace utf



// Template specializations for utf::string.
namespace std {

template<>
inline void
swap<utf::string>(utf::string& s1, // IN/OUT
                  utf::string& s2) // IN/OUT
{
   s1.swap(s2);
}

} // namespace std


#ifdef _WIN32
#pragma warning(pop)
#endif

#endif
