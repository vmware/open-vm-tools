/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BasePlatformLink_h_
#define BasePlatformLink_h_

#include "BasePlatformInc.h"

#include <deque>
#include <vector>
#include <set>
#include <list>
#include <map>

namespace Caf {

// Basic string containers
typedef std::deque<std::string> Cdeqstr;
typedef std::set<std::string> Csetstr;
typedef std::vector<std::string> Cvecstr;
typedef std::map<std::string, std::string> Cmapstrstr;
typedef std::multimap<std::string, std::string> Cmmapstrstr;
CAF_DECLARE_SMART_POINTER(Cdeqstr);
CAF_DECLARE_SMART_POINTER(Csetstr);
CAF_DECLARE_SMART_POINTER(Cvecstr);
CAF_DECLARE_SMART_POINTER(Cmapstrstr);
CAF_DECLARE_SMART_POINTER(Cmmapstrstr);

// GUID containers
typedef std::vector<GUID> Cvecguid;
typedef std::deque<GUID> Cdeqguid;
CAF_DECLARE_SMART_POINTER(Cvecguid);
CAF_DECLARE_SMART_POINTER(Cdeqguid);

struct SGuidLessThan
{
	bool operator()(const GUID clhs, const GUID crhs) const
	{
		return ::memcmp(&clhs, &crhs, sizeof(GUID)) < 0;
	}
};

typedef std::set<GUID, SGuidLessThan> Csetguid;
CAF_DECLARE_SMART_POINTER(Csetguid);

// class to extract first part of a pair
template <typename T>
struct select1st
{
	typename T::first_type operator()(const T & p) const { return p.first; }
};

// template function to make use of select1st easy.
// T is a container whose value_type is a pair
template <typename T>
select1st<typename T::value_type> make_select1st(const T &) { return select1st<typename T::value_type>(); };

// class to extract second part of a pair
template <typename T>
struct select2nd
{
	typename T::second_type operator()(const T & p) const { return p.second; }
};

// template function to make use of select2nd easy.
// T is a container whose value_type is a pair
template <typename T>
select2nd<typename T::value_type> make_select2nd(const T &) { return select2nd<typename T::value_type>(); };

}

#ifdef WIN32
#include "PlatformApi.h"
#endif

#endif

