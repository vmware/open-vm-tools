/*********************************************************
 * Copyright (C) 2011-2016,2023 VMware, Inc. All rights reserved.
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
 * @file samlInt.hpp
 *
 * Functions that only need to be used within the SAML module or
 * for testing thereof.
 */

#ifndef _SAMLINT_H_
#define _SAMLINT_H_

#include <string>
#include <vector>

#include <xercesc/framework/XMLGrammarPool.hpp>

#include <glib.h>

#include "VGAuthError.h"


using namespace std;

#ifdef XERCES_CPP_NAMESPACE_USE
XERCES_CPP_NAMESPACE_USE
#endif


/**
 * Inherit from this class to disallow copy-construction and
 * assignment. (Similar to boost::noncopyable)
 */

class Noncopyable {
protected:
   Noncopyable()
   {
   }

private:
   // Disallow copy-construction, assignment.
   Noncopyable(const Noncopyable &);
   const Noncopyable& operator=(const Noncopyable &);
};

/**
 * A simple wrapper to convert Xerces's XMLCh strings to UTF-8, and manage
 * the memory for the converted string. Instances of this class are immutable.
 */

class SAMLStringWrapper : public Noncopyable {
public:
   SAMLStringWrapper(const XMLCh *str) :
      m_str(XMLString::transcode(str))
   {
   }

   ~SAMLStringWrapper()
   {
      XMLString::release(const_cast<char **>(&m_str));
   }

   const char *
   c_str() const
   {
      return m_str;
   }

private:
   const char *m_str;
};

/**
 * Wrapper class for strings allocated by GLib. The object takes ownership of
 * the string's memory.
 */

class SAMLGlibString {
public:
   SAMLGlibString(gchar *str) :
      m_str(str)
   {
   }

   SAMLGlibString(const SAMLGlibString &s) :
      m_str(g_strdup(s.m_str))
   {
   }

   ~SAMLGlibString()
   {
      g_free(const_cast<char *>(m_str));
   }

const char *
   c_str() const
   {
      return m_str;
   }

private:
   SAMLGlibString &operator=(const SAMLGlibString &);  // immuatable

   const gchar *m_str;
};

/*
 * Holds data extracted from a SAML token.
 */
struct SAMLTokenData {
   string subjectName;
   vector<string> issuerCerts;
   bool oneTimeUse;
   bool isSSOToken;           // set if token came from VMware SSO server
   string ns;
};


auto_ptr<XMLGrammarPool> SAMLCreateAndPopulateGrammarPool();

VGAuthError SAMLVerifyAssertion(const char *xmlText,
                                gboolean hostVerified,
                                SAMLTokenData &token,
                                vector<string> &certs);
#endif // ifndef _SAMLINT_H_
