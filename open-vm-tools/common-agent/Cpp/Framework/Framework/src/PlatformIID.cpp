/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "PlatformIID.h"

#include <sstream>
#include <iomanip>
#ifndef WIN32
#include <stdlib.h>
#include <sys/time.h>
#endif

GMutex BasePlatform::gs_BaseIIDInitMutex;

using namespace std;

namespace BasePlatform {

BASEPLATFORM_LINKAGE std::string UuidToString(const UUID& uuid) {
	stringstream str;
	str.fill('0');
	str.setf(ios_base::uppercase);
	str.setf(ios_base::hex, ios_base::basefield);
	str << setw(8) << uuid.Data1;
	str << '-' << setw(4) << uuid.Data2;
	str << '-' << setw(4) << uuid.Data3;
	str << '-' << setw(2) << (int32)uuid.Data4[0];
	str << setw(2) << (int32)uuid.Data4[1];
	str << '-' << setw(2) << (int32)uuid.Data4[2];
	str << setw(2) << (int32)uuid.Data4[3];
	str << setw(2) << (int32)uuid.Data4[4];
	str << setw(2) << (int32)uuid.Data4[5];
	str << setw(2) << (int32)uuid.Data4[6];
	str << setw(2) << (int32)uuid.Data4[7];
	return str.str();
}

unsigned char Char2Bin(const unsigned char cucValue)
{
	unsigned char ucBinValue = 0;
	if ('0' <= cucValue && '9' >= cucValue)
	{
		ucBinValue = cucValue - '0';
	}
	else
	{
		ucBinValue = ::tolower(cucValue) - 'a' + 10;
	}
	return ucBinValue;
}

BASEPLATFORM_LINKAGE HRESULT UuidFromString(const char* pszUuid, UUID& uuid) {
	if (pszUuid)
	{
		// Check to see if the guid is surrounded by '{' '}'
		if (*pszUuid == '{') {
			pszUuid = &pszUuid[1];
		}

		if (::strlen(pszUuid) >= 36)
		{
			if (('-' != pszUuid[8]) || ('-' != pszUuid[13]) ||
				 ('-' != pszUuid[18]) || ('-' != pszUuid[23]))
			{
				return E_INVALIDARG;
			}

			for (int32 iLoop = 0; iLoop < 36; ++iLoop)
			{
				if (8 != iLoop && 13 != iLoop && 18 != iLoop && 23 != iLoop)
				{
#if defined (__APPLE__) || defined (__hpux__) || defined (__sun__) || (defined (__linux__) && OS_RELEASE_MAJOR == 2)
					if (!isxdigit(pszUuid[iLoop]))
#else
					if (!::isxdigit(pszUuid[iLoop]))
#endif
					{
						return E_INVALIDARG;
					}
				}
			}

			// so we've validated the string, so now we need to BCD it into
			// the UUID structure - make a temporary and then memcpy it
			uuid.Data1 = Char2Bin(pszUuid[0]) << 28;
			uuid.Data1 |= Char2Bin(pszUuid[1]) << 24;
			uuid.Data1 |= Char2Bin(pszUuid[2]) << 20;
			uuid.Data1 |= Char2Bin(pszUuid[3]) << 16;
			uuid.Data1 |= Char2Bin(pszUuid[4]) << 12;
			uuid.Data1 |= Char2Bin(pszUuid[5]) << 8;
			uuid.Data1 |= Char2Bin(pszUuid[6]) << 4;
			uuid.Data1 |= Char2Bin(pszUuid[7]);

			uuid.Data2 = Char2Bin(pszUuid[9]) << 12;
			uuid.Data2 |= Char2Bin(pszUuid[10]) << 8;
			uuid.Data2 |= Char2Bin(pszUuid[11]) << 4;
			uuid.Data2 |= Char2Bin(pszUuid[12]);

			uuid.Data3 = Char2Bin(pszUuid[14]) << 12;
			uuid.Data3 |= Char2Bin(pszUuid[15]) << 8;
			uuid.Data3 |= Char2Bin(pszUuid[16]) << 4;
			uuid.Data3 |= Char2Bin(pszUuid[17]);

			uuid.Data4[0] = Char2Bin(pszUuid[19]) << 4;
			uuid.Data4[0] |= Char2Bin(pszUuid[20]);
			uuid.Data4[1] = Char2Bin(pszUuid[21]) << 4;
			uuid.Data4[1] |= Char2Bin(pszUuid[22]);
			uuid.Data4[2] = Char2Bin(pszUuid[24]) << 4;
			uuid.Data4[2] |= Char2Bin(pszUuid[25]);
			uuid.Data4[3] = Char2Bin(pszUuid[26]) << 4;
			uuid.Data4[3] |= Char2Bin(pszUuid[27]);
			uuid.Data4[4] = Char2Bin(pszUuid[28]) << 4;
			uuid.Data4[4] |= Char2Bin(pszUuid[29]);
			uuid.Data4[5] = Char2Bin(pszUuid[30]) << 4;
			uuid.Data4[5] |= Char2Bin(pszUuid[31]);
			uuid.Data4[6] = Char2Bin(pszUuid[32]) << 4;
			uuid.Data4[6] |= Char2Bin(pszUuid[33]);
			uuid.Data4[7] = Char2Bin(pszUuid[34]) << 4;
			uuid.Data4[7] |= Char2Bin(pszUuid[35]);

			return S_OK;
		}
		else
		{
			return E_INVALIDARG;
		}
	}

	return E_INVALIDARG;
}
}
#ifndef WIN32

HRESULT UuidCreate(UUID* uuid) {
	if (uuid) {
		// a uuid is 16 bytes - we'll fill them with a
		// random number (1st 4 bytes)
		// the current time in seconds (2nd 4 bytes)
		// the hostid of the machine (3rd 4 bytes)
		// the current micro seconds (last 4 bytes)
		int32 iRand = ::rand();
		struct timeval stTimeVal;
		::gettimeofday(&stTimeVal, NULL);
		int32 iHostId = ::gethostid();

		byte * pbUuid = reinterpret_cast<byte*>(uuid);
		::memcpy(pbUuid, &iRand, 4);
		::memcpy(pbUuid+4, &stTimeVal.tv_sec, 4);
		::memcpy(pbUuid+8, &iHostId, 4);
		::memcpy(pbUuid+12, &stTimeVal.tv_usec, 4);
		return S_OK;
	}
	else {
		return OLE_E_BLANK;
	}
}

#endif
