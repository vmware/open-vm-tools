/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CXPathHeaderEnricherItem_h_
#define CXPathHeaderEnricherItem_h_


#include "Integration/IDocument.h"

namespace Caf {

class CXPathHeaderEnricherItem {
private:
	typedef std::map<std::string, bool> CFileCollection;
	CAF_DECLARE_SMART_POINTER(CFileCollection);

public:
	CXPathHeaderEnricherItem();
	virtual ~CXPathHeaderEnricherItem();

public:
	void initialize(
		const SmartPtrIDocument& configSection,
		const bool& defaultOverwrite);

public:
	std::string getName() const;
	std::string getEvaluationType() const;
	bool getOverwrite() const;
	std::string getXpathExpression() const;
	std::string getXpathExpressionRef() const;

private:
	bool _isInitialized;

	std::string _name;
	std::string _evaluationType;
	bool _overwrite;
	std::string _xpathExpression;
	std::string _xpathExpressionRef;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CXPathHeaderEnricherItem);
};

CAF_DECLARE_SMART_POINTER(CXPathHeaderEnricherItem);

}

#endif // #ifndef CXPathHeaderEnricherItem_h_
