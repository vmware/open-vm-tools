/*
 *	 Author: bwilliams
 *  Created: 10/22/2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef IntegrationContractsDefines_h_
#define IntegrationContractsDefines_h_

#include <BaseDefines.h>

#include <map>
#include <string>
#include <list>
#include <deque>
#include <vector>

////////////////////////////////////////////////////////////////////////
//
// Class declaration helpers
//
////////////////////////////////////////////////////////////////////////
#define INTEGRATION_CONTRACT_DECLARE_STD_ACCESSOR( _classname_ ) \
	public: \
	_classname_(){}\
	private: \
	_classname_ (const _classname_ &); \
	_classname_ & operator=(const _classname_ &); \
	public:

#define INTEGRATION_CONTRACT_DECLARE_NOCOPY( _classname_ ) \
	private: \
	_classname_ (const _classname_ &); \
	_classname_ & operator=(const _classname_ &)

////////////////////////////////////////////////////////////////////////
//
// Put these accessors in the header file to provide the implementation.
//
////////////////////////////////////////////////////////////////////////
#define INTEGRATION_CONTRACT_ACCESSOR_IMPL_GET_PUT( _Type_, _Name_ ) \
	private: \
		_Type_ _##_Name_; \
	public: \
		_Type_ get##_Name_() const \
		{ \
			return _##_Name_; \
		} \
		void put##_Name_(const _Type_& cr##_Name_) \
		{ \
			_##_Name_ = cr##_Name_; \
		}

////////////////////////////////////////////////////////////////////////
//
// Put these accessors in the interface file to provide the declaration.
//
////////////////////////////////////////////////////////////////////////
#define INTEGRATION_CONTRACT_ACCESSOR_VIRT_GET( _Type_, _Name_ ) \
	virtual _Type_ get##_Name_() const = 0;

#define INTEGRATION_CONTRACT_ACCESSOR_VIRT_PUT( _Type_, _Name_ ) \
	virtual void put##_Name_(const _Type_ & cr##_Name_) = 0;

#define INTEGRATION_CONTRACT_ACCESSOR_VIRT_GET_PUT( _Type_, _Name_ ) \
	virtual _Type_ get##_Name_() const = 0; \
	virtual void put##_Name_(const _Type_ & cr##_Name_) = 0;

#endif
