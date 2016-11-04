/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef TMETHODIMPL_H_
#define TMETHODIMPL_H_


#include "amqpClient/amqpImpl/IMethod.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Template implementing the static creator function for AMQP method implementations
 */
template <class Cl>
class TMethodImpl : public IMethod {
public:
	/**
	 * @brief Create the method object
	 * @return the method object
	 */
	static SmartPtrIMethod Creator();
};

template<class Cl>
SmartPtrIMethod TMethodImpl<Cl>::Creator() {
	TCafSmartPtr<Cl, TCafQIObject<Cl> > method;
	method.CreateInstance();
	return method;
}

}}

#define METHOD_DECL(_amqpImpl_, _num_, _name_, _has_content_) \
	CAF_BEGIN_QI() \
		CAF_QI_ENTRY(IMethod) \
		CAF_QI_ENTRY(_amqpImpl_) \
	CAF_END_QI() \
	public: \
	bool hasContent() { \
		return _has_content_; \
	} \
	uint16 getProtocolClassId() { \
		return (uint16)((_num_ & 0xffff0000) >> 16); \
	} \
	\
	uint16 getProtocolMethodId() { \
		return (uint16)(_num_ & 0x0000ffff); \
	} \
	\
	virtual std::string getProtocolMethodName() { \
		return _name_; \
	}

#endif /* TMETHODIMPL_H_ */
