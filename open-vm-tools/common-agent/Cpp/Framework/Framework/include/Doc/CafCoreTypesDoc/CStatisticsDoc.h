/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CStatisticsDoc_h_
#define CStatisticsDoc_h_


#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type Statistics
class CAFCORETYPESDOC_LINKAGE CStatisticsDoc {
public:
	CStatisticsDoc();
	virtual ~CStatisticsDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCPropertyCollectionDoc propertyCollection,
		const int32 providerJobNum,
		const int32 providerJobTotal,
		const int32 providerJobDurationSecs,
		const int32 pmeNum,
		const int32 pmeTotal,
		const int32 pmeDurationSecs);

public:
	/// Accessor for the PropertyCollection
	SmartPtrCPropertyCollectionDoc getPropertyCollection() const;

	/// Accessor for the ProviderJobNum
	int32 getProviderJobNum() const;

	/// Accessor for the ProviderJobTotal
	int32 getProviderJobTotal() const;

	/// Accessor for the ProviderJobDurationSecs
	int32 getProviderJobDurationSecs() const;

	/// Accessor for the PmeNum
	int32 getPmeNum() const;

	/// Accessor for the PmeTotal
	int32 getPmeTotal() const;

	/// Accessor for the PmeDurationSecs
	int32 getPmeDurationSecs() const;

private:
	SmartPtrCPropertyCollectionDoc _propertyCollection;
	int32 _providerJobNum;
	int32 _providerJobTotal;
	int32 _providerJobDurationSecs;
	int32 _pmeNum;
	int32 _pmeTotal;
	int32 _pmeDurationSecs;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CStatisticsDoc);
};

CAF_DECLARE_SMART_POINTER(CStatisticsDoc);

}

#endif
