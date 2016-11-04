/*
 *  Created on: May 15, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef TBLOCKINGCELL_H_
#define TBLOCKINGCELL_H_

#include "Common/CThreadSignal.h"
#include "Exception/CCafException.h"

namespace Caf {

/**
 * @brief Simple one-shot IPC mechanism.
 * Allows a value to be passed from one thread to another in a thread-safe manner.
 * The value may only be set once and once set may get retrieved as many times as
 * desired.
 */
template <class T> class TBlockingCell {

public:
	TBlockingCell() :
		_filled(false) {
		CAF_CM_INIT_THREADSAFE;
		CAF_THREADSIGNAL_INIT;
		//TODO: This has stopped working... why?
		//_value = NULL;
		_valueSignal.initialize("valueSignal");
	}

	virtual ~TBlockingCell() {
	}

	/**
	 * @brief Wait for a value indefinitely
	 * Waits indefinitely for the value to be set or returns the value if it is
	 * already set.
	 * @return value
	 */
	T get() {
		return get(0);
	}

	/**
	 * @brief Wait for a value for a specified amount of time
	 * Waits for a set amout of time (milliseconds) for the value to be set or returns
	 * the value if it is already set.
	 * @retval value if set
	 * @retval TimeoutException thrown if time expires
	 */
	T get(uint32 timeoutMs) {
		CAF_CM_STATIC_FUNC("TBlockingCell", "get");
		CAF_CM_LOCK;
		CAF_THREADSIGNAL_LOCK_UNLOCK;
		if (!_filled) {
			CAF_CM_UNLOCK;
			_valueSignal.waitOrTimeout(CAF_THREADSIGNAL_MUTEX, timeoutMs);
			CAF_CM_LOCK;
			if (!_filled) {
				CAF_CM_UNLOCK;
				CAF_CM_EXCEPTIONEX_VA0(
						TimeoutException,
						0,
						"Timed out waiting for value to be set");
			} else {
				CAF_CM_UNLOCK;
			}
		} else {
			CAF_CM_UNLOCK;
		}
		return _value;
	}

	/**
	 * @brief Sets a new value if the value is not already set
	 * If the value is already set an IllegalStateException will be thrown.
	 */
	void set(T newValue) {
		CAF_CM_STATIC_FUNC("TBlockingCell", "set");
		CAF_CM_LOCK_UNLOCK;
		if (_filled) {
			CAF_CM_EXCEPTIONEX_VA0(
					IllegalStateException,
					0,
					"The value can only be set once");
		}
		_value = newValue;
		_filled = true;
		_valueSignal.signal();
	}

private:
	bool _filled;
	T _value;
	CAF_THREADSIGNAL_CREATE;
	CThreadSignal _valueSignal;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(TBlockingCell);
};

}


#endif /* TBLOCKINGCELL_H_ */
