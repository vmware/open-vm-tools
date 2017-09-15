/*
 *	 Author: bwilliams
 *  Created: Jan 26, 2011
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _FxContracts_IBean_h_
#define _FxContracts_IBean_h_


#include "ICafObject.h"

namespace Caf {

CAF_FORWARD_DECLARE_SMART_INTERFACE(IBean);

/// TODO - describe interface
struct __declspec(novtable)
IBean : public ICafObject {
	CAF_DECL_UUID("860C6E41-76E4-404b-913F-C330EE864DCD")

	struct CArg {
		CArg(const SmartPtrIBean& ref) :
			_reference(ref),
			_type(REFERENCE) {}
		CArg(const std::string& val) :
			_value(val),
			_type(VALUE) {}

		typedef enum {
			REFERENCE,
			VALUE
		} ARG_TYPE;

		SmartPtrIBean _reference;
		std::string _value;
		ARG_TYPE _type;

	private:
		CArg();
	};

	typedef std::deque<CArg> Cargs;
	typedef Cmapstrstr Cprops;

	virtual void initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties) = 0;

	virtual void terminateBean() = 0;
};

}

#endif
