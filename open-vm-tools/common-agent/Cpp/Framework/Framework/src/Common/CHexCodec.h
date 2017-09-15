/*
 *	 Author: mdonahue
 *  Created: May 24, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CHEXCODEC_H_
#define CHEXCODEC_H_

namespace Caf {
class COMMONAGGREGATOR_LINKAGE CHexCodec {
public:
	/// Encodes a buffer as a 2 digit per byte string.
	///
	/// @param buffer Input buffer
	/// @param bufferSize Input buffer length
	/// @param pairSpacing Number of spaces to insert between digit pairs
	/// @param pairsPerLine Number of pairs to encode per line. 0 = single line output.
	/// @param lineBreakChar Character to insert between lines
	/// @return A string containing the hex encoding
	static std::string Encode(
		const byte* buffer,
		const uint32 bufferSize,
		const uint32 pairSpacing = 0,
		const uint32 pairsPerLine = 0,
		const char lineBreakChar = '\n');
};
}

#endif /* CHEXCODEC_H_ */
