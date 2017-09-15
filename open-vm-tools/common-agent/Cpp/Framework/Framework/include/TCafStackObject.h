/*
 *  Created on: Jul 23, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef TCAFSTACKOBJECT_H_
#define TCAFSTACKOBJECT_H_

namespace Caf {

template <class Base>
class TCafStackObject : public Base {
public:
	TCafStackObject() {}
	virtual ~TCafStackObject() {}

private:
	virtual void AddRef() {
		throw std::runtime_error("TCafStackObj::AddRef not supported");
	}

	virtual void Release() {
		throw std::runtime_error("TCafStackObj::Release not supported");
	}

	virtual void QueryInterface(const IID&, void** ppv) {
		throw std::runtime_error("TCafStackObj::QueryInterface not supported");
	}
};

}
#endif /* TCAFSTACKOBJECT_H_ */
