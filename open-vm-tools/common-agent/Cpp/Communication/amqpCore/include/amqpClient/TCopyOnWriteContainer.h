/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef TCOPYONWRITECONTAINER_H_
#define TCOPYONWRITECONTAINER_H_

namespace Caf { namespace AmqpClient {

/**
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief An STL container wrapper that implements copy-on-write semantics
 */
template <typename Container, typename ValueType = typename Container::value_type>
class TCopyOnWriteContainer {
public:
	typedef TCafSmartPtr<Container, TCafObject<Container> > SmartPtrContainer;

	TCopyOnWriteContainer() {
		_container.CreateInstance();
	}

	SmartPtrContainer getAll() {
		return _container;
	}

	void add(const ValueType& value) {
		SmartPtrContainer newContainer;
		newContainer.CreateInstance();
		newContainer->insert(
				newContainer->begin(),
				_container->begin(),
				_container->end());
		newContainer->push_back(value);
		_container = newContainer;
	}

	bool remove(const ValueType& value) {
		bool found = false;
		SmartPtrContainer newContainer;
		newContainer.CreateInstance();
		SmartPtrContainer currContainer = _container;
		for (typename Container::const_iterator iter = currContainer->begin();
				iter != currContainer->end();
				++iter) {
			if (*iter == value) {
				found = true;
			} else {
				newContainer->push_back(*iter);
			}
		}

		if (found) {
			_container = newContainer;
		}
		return found;
	}

	void clear() {
		_container.CreateInstance();
	}

private:
	SmartPtrContainer _container;
};

}}

#endif /* TCOPYONWRITECONTAINER_H_ */
