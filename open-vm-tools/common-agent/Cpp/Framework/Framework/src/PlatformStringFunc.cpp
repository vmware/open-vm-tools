/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "PlatformStringFunc.h"
#include <sstream>

using namespace std;

namespace BasePlatform {
#ifdef fix
std::wstring A2W(const std::string& str)
{
    wstringstream wstm ;
    const ctype<wchar_t>& ctfacet = use_facet< ctype<wchar_t> >(wstm.getloc());
    for (size_t i = 0; i < str.size(); ++i) {
    	wstm << ctfacet.widen(str[i]);
    }
    return wstm.str();
}

std::string W2A(const std::wstring& str)
{
    ostringstream stm;
    const ctype<char>& ctfacet = use_facet< ctype<char> >(stm.getloc());
    for (size_t i = 0; i < str.size(); ++i) {
    	stm << ctfacet.narrow(str[i], 0);
    }
    return stm.str();
}
#endif
}
