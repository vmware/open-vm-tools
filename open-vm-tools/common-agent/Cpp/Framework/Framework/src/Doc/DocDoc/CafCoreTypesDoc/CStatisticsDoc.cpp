/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CStatisticsDoc.h"

using namespace Caf;

/// A simple container for objects of type Statistics
CStatisticsDoc::CStatisticsDoc() :
	_providerJobNum(0),
	_providerJobTotal(0),
	_providerJobDurationSecs(0),
	_pmeNum(0),
	_pmeTotal(0),
	_pmeDurationSecs(0),
	_isInitialized(false) {}
CStatisticsDoc::~CStatisticsDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CStatisticsDoc::initialize(
	const SmartPtrCPropertyCollectionDoc propertyCollection,
	const int32 providerJobNum,
	const int32 providerJobTotal,
	const int32 providerJobDurationSecs,
	const int32 pmeNum,
	const int32 pmeTotal,
	const int32 pmeDurationSecs) {
	if (! _isInitialized) {
		_propertyCollection = propertyCollection;
		_providerJobNum = providerJobNum;
		_providerJobTotal = providerJobTotal;
		_providerJobDurationSecs = providerJobDurationSecs;
		_pmeNum = pmeNum;
		_pmeTotal = pmeTotal;
		_pmeDurationSecs = pmeDurationSecs;

		_isInitialized = true;
	}
}

/// Accessor for the PropertyCollection
SmartPtrCPropertyCollectionDoc CStatisticsDoc::getPropertyCollection() const {
	return _propertyCollection;
}

/// Accessor for the ProviderJobNum
int32 CStatisticsDoc::getProviderJobNum() const {
	return _providerJobNum;
}

/// Accessor for the ProviderJobTotal
int32 CStatisticsDoc::getProviderJobTotal() const {
	return _providerJobTotal;
}

/// Accessor for the ProviderJobDurationSecs
int32 CStatisticsDoc::getProviderJobDurationSecs() const {
	return _providerJobDurationSecs;
}

/// Accessor for the PmeNum
int32 CStatisticsDoc::getPmeNum() const {
	return _pmeNum;
}

/// Accessor for the PmeTotal
int32 CStatisticsDoc::getPmeTotal() const {
	return _pmeTotal;
}

/// Accessor for the PmeDurationSecs
int32 CStatisticsDoc::getPmeDurationSecs() const {
	return _pmeDurationSecs;
}





