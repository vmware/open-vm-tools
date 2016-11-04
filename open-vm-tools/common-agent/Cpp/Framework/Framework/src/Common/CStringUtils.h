/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CSTRINGUTILS_H_
#define CSTRINGUTILS_H_

#include <sstream>

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CStringUtils {
public:
	static Cdeqstr split(const std::string &str, const char delim);
	static std::string trim(const std::string &s);
	static std::string trimLeft(const std::string &s);
	static std::string trimRight(const std::string &s);
	static std::string expandEnv(const std::string &s);
	static UUID createRandomUuidRaw();
	static std::string createRandomUuid();
	static bool isEqualIgnoreCase(const std::string & src, const std::string & srch);
#ifdef WIN32
	static std::string convertWideToNarrow(const std::wstring& src);
	static std::wstring convertNarrowToWide(const std::string& src);
#endif

	static std::string toLower(
			const std::string& str);

	static std::string toUpper(
			const std::string& str);

private:
	CAF_CM_DECLARE_NOCREATE(CStringUtils);
};

namespace CStringConv {

// Templates to convert numbers to strings
template <class T, class chartype>
inline std::basic_string<chartype> toTString(const T& t) {
	std::basic_ostringstream<chartype> o;
	o << t;
	if (o.fail()) {
		throw std::runtime_error("cannot convert number to string");
	}
	return o.str();
}

template <class T>
inline std::string toString(const T& t) {
	return toTString<T, char>(t);
}

template <class T>
inline std::wstring toWString(const T& t) {
	return toTString<T, wchar_t>(t);
}

// Templates to convert strings to numbers
template <class T, class chartype>
inline T fromTString(const std::basic_string<chartype>& s) {
	T t;
	std::basic_istringstream<chartype> i(s);
	i >> t;
	if (i.fail()) {
		throw std::runtime_error("cannot convert string '" + s + "' to number");
	}
	return t;
}

template <class T>
inline T fromString(const std::string& s) {
	return fromTString<T, char>(s);
}

template <class T>
inline T fromWString(const std::wstring& s) {
	return fromTString<T, wchar_t>(s);
}

}}

#endif /* CSTRINGUTILS_H_ */
