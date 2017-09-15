/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CXmlUtils_H_
#define CXmlUtils_H_



#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

class XMLUTILS_LINKAGE CXmlUtils {
public:
	static SmartPtrCXmlElement parseFile(
		const std::string& path,
		const std::string& rootName);

	static SmartPtrCXmlElement parseString(
		const std::string& xml,
		const std::string& rootName);

	static SmartPtrCXmlElement createRootElement(
		const std::string& rootName,
		const std::string& rootNamespace);

	static SmartPtrCXmlElement createRootElement(
		const std::string& rootName,
		const std::string& rootNamespace,
		const std::string& schemaLocation);

	static std::string escape(
		const std::string& text);

private:
	CAF_CM_DECLARE_NOCREATE(CXmlUtils);
};

}

#endif /* CXmlUtils_H_ */
