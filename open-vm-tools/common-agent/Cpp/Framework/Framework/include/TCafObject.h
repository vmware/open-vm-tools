/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */
#ifndef _TCafObject_h
#define _TCafObject_h

template <class Base>
class TCafObject : public Base {
public:
	TCafObject() : _refCnt(0) {}

public: // ICafObect Implementations
	void AddRef() {
		g_atomic_int_inc(&_refCnt);
	}

	void Release() {
		if (g_atomic_int_dec_and_test(&_refCnt)) {
			delete this;
		}
	}

	void QueryInterface(const IID&, void**) {
		throw std::runtime_error("QueryInterface not supported");
	}

private:
	gint _refCnt;

private:
	TCafObject(const TCafObject&);
	TCafObject& operator=(const TCafObject&);
};

#endif
