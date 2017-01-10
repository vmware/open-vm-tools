/*
 *	 Author: mdonahue
 *  Created: May 24, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CHexCodec.h"
#include <sstream>
#include <iomanip>

using namespace Caf;
using namespace std;

std::string CHexCodec::Encode(
	const byte* buffer,
	const uint32 bufferSize,
	const uint32 pairSpacing,
	const uint32 pairsPerLine,
	const char lineBreakChar) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CHexCodec", "Encode");

	stringstream encoding;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_PTR(buffer);
		CAF_CM_VALIDATE_NOTZERO(bufferSize);

		string spacing;
		for (uint32 u = 0; u < pairSpacing; ++u) {
			spacing += ' ';
		}

		uint16 uTest = 1;
		byte bTest[2] = { 1, 0 };
		const bool isLittleEndian = (::memcmp(&uTest, bTest, 2) == 0);
		uint32 groups = bufferSize >> 2;
		uint32 remainder = bufferSize % 4;
		encoding.fill('0');
		encoding.setf(ios_base::uppercase);
		encoding.setf(ios_base::hex, ios_base::basefield);
		uint8 b[4];
		const uint32* uintPtr = reinterpret_cast<const uint32*>(buffer);
		uint32 pairs = 0;
		for (uint32 u = 0; u < groups; ++u) {
			if (isLittleEndian) {
				b[0] = static_cast<uint8>(*uintPtr);
				b[1] = static_cast<uint8>(*uintPtr >> 8);
				b[2] = static_cast<uint8>(*uintPtr >> 16);
				b[3] = static_cast<uint8>(*uintPtr >> 24);
			} else {
				b[0] = static_cast<uint8>(*uintPtr >> 24);
				b[1] = static_cast<uint8>(*uintPtr >> 16);
				b[2] = static_cast<uint8>(*uintPtr >> 8);
				b[3] = static_cast<uint8>(*uintPtr);
			}

			for (uint32 p = 0; p < 4; ++p) {
				if (pairSpacing && pairs) {
					encoding << spacing;
				}
				encoding << setw(2) << static_cast<uint32>(b[p]);
				if (pairsPerLine) {
					if (0 == (++pairs % pairsPerLine)) {
						encoding << lineBreakChar;
						pairs = 0;
					}
				} else {
					++pairs;
				}
			}
			++uintPtr;
		}

		switch (remainder) {
		case 0:
			break;
		case 1:
			if (isLittleEndian) {
				b[0] = static_cast<uint8>(*uintPtr);
			} else {
				b[0] = static_cast<uint8>(*uintPtr >> 24);
			}
			break;
		case 2:
			if (isLittleEndian) {
				b[0] = static_cast<uint8>(*uintPtr);
				b[1] = static_cast<uint8>(*uintPtr >> 8);
			} else {
				b[0] = static_cast<uint8>(*uintPtr >> 24);
				b[1] = static_cast<uint8>(*uintPtr >> 16);
			}
			break;
		case 3:
			if (isLittleEndian) {
				b[0] = static_cast<uint8>(*uintPtr);
				b[1] = static_cast<uint8>(*uintPtr >> 8);
				b[2] = static_cast<uint8>(*uintPtr >> 16);
			} else {
				b[0] = static_cast<uint8>(*uintPtr >> 24);
				b[1] = static_cast<uint8>(*uintPtr >> 16);
				b[2] = static_cast<uint8>(*uintPtr >> 8);
			}
			break;
		}

		for (uint32 p = 0; p < remainder; ++p) {
			if (pairSpacing && pairs) {
				encoding << spacing;
			}
			encoding << setw(2) << static_cast<uint32>(b[p]);
			if (pairsPerLine) {
				if (0 == (++pairs % pairsPerLine)) {
					encoding << lineBreakChar;
					pairs = 0;
				}
			} else {
				++pairs;
			}
		}
	}
	CAF_CM_EXIT;

	return encoding.str();
}
