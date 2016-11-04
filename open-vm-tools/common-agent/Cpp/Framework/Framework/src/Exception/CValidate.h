/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CVALIDATE_H_
#define CVALIDATE_H_

namespace Caf {

class EXCEPTION_LINKAGE CValidate {
public:
	static void constructed(const bool value, const char* className, const char* funcName);
	static void initialized(const bool value, const char* className, const char* funcName);
	static void notInitialized(const bool value, const char* className, const char* funcName);
	static void notEmptyStr(const std::string& value, const char* name, const char* className, const char* funcName);
	static void notEmptyStr(const std::wstring& value, const char* name, const char* className, const char* funcName);
	static void notEmptyUuid(const UUID value, const char* name, const char* className, const char* funcName);
	static void notNullOrEmptyStr(const char* value, const char* name, const char* className, const char* funcName);
	static void notNullOrEmptyStr(const wchar_t* value, const char* name, const char* className, const char* funcName);
	static void notNullOrEmptyPtrArr(const void** value, const char* name, const char* className, const char* funcName);
	static void notNullInterface(const ICafObject* value, const char* name, const char* className, const char* funcName);
	static void notNullPtr(const void* value, const char* name, const char* className, const char* funcName);
	static void nullPtr(const void* value, const char* name, const char* className, const char* funcName);
	static void zero(const int32 value, const char* name, const char* className, const char* funcName);
	static void notZero(const int32 value, const char* name, const char* className, const char* funcName);
	static void positive(const int32 value, const char* name, const char* className, const char* funcName);
	static void negative(const int32 value, const char* name, const char* className, const char* funcName);
	static void nonNegative(const int32 value, const char* name, const char* className, const char* funcName);
    static void nonNegative64(const int64 value, const char* name, const char* className, const char* funcName);
	static void isTrue(const bool value, const char* name, const char* className, const char* funcName);
	static void notEmptyStl(const size_t value, const char* name, const char* className, const char* funcName);
	static void emptyStl(const size_t value, const char* name, const char* className, const char* funcName);

private:
	CAF_CM_DECLARE_NOCREATE(CValidate);
};

}

#endif /* CVALIDATE_H_ */
