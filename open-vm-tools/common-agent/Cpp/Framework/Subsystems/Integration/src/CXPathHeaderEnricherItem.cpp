/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "CXPathHeaderEnricherItem.h"

using namespace Caf;

CXPathHeaderEnricherItem::CXPathHeaderEnricherItem() :
	_isInitialized(false),
	_overwrite(true),
	CAF_CM_INIT("CXPathHeaderEnricherItem") {
}

CXPathHeaderEnricherItem::~CXPathHeaderEnricherItem() {
}

void CXPathHeaderEnricherItem::initialize(
	const SmartPtrIDocument& configSection,
	const bool& defaultOverwrite) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_name = configSection->findRequiredAttribute("name");
	_evaluationType = configSection->findOptionalAttribute("evaluation-type");
	_xpathExpression = configSection->findOptionalAttribute("xpath-expression");
	_xpathExpressionRef = configSection->findOptionalAttribute("xpath-expression-ref");

	_overwrite = defaultOverwrite;
	const std::string overwriteStr = configSection->findOptionalAttribute("overwrite");
	if (! overwriteStr.empty()) {
		_overwrite = (overwriteStr.compare("true") == 0);
	}

	if (_evaluationType.empty()) {
		_evaluationType = "STRING_RESULT";
	}

	_isInitialized = true;
}

std::string CXPathHeaderEnricherItem::getName() const {
	return _name;
}

std::string CXPathHeaderEnricherItem::getEvaluationType() const {
	return _evaluationType;
}

bool CXPathHeaderEnricherItem::getOverwrite() const {
	return _overwrite;
}

std::string CXPathHeaderEnricherItem::getXpathExpression() const {
	return _xpathExpression;
}

std::string CXPathHeaderEnricherItem::getXpathExpressionRef() const {
	return _xpathExpressionRef;
}
