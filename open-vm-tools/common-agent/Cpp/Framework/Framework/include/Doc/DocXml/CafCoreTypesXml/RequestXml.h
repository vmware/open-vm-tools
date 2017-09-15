/*
 *  Author: bwilliams
 *  Created: Nov 21, 2014
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef RequestXml_h_
#define RequestXml_h_


#include "Xml/XmlUtils/CXmlElement.h"

#include "Doc/DocXml/CafCoreTypesXml/CafCoreTypesXmlLink.h"

namespace Caf {

	namespace RequestXml {

		SmartPtrCXmlElement CAFCORETYPESXML_LINKAGE parseString(
			const std::string& xml,
			const std::string& rootName);

		SmartPtrCXmlElement CAFCORETYPESXML_LINKAGE parseFile(
			const std::string& xml,
			const std::string& rootName);
	}
}

#endif
