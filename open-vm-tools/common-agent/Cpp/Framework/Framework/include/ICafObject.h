/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _ICafObject_H_
#define _ICafObject_H_

/**
 * @brief
 * The base interface all classes must support in order to support lifetime management
 * and interface retrieval.
 * <p>
 * Object lifetime is managed through the AddRef() and Release() methods.  Pointers
 * to supported interfaces on the object - in the form of TCafSmartPtr - are
 * retrieved using the QueryInterface() method.
 * <p>
 * Interfaces and classes are candidates for QueryInterface() if they declare
 * a unique identifier for the interface or class.  The identifier is a
 * UUID and is declared using the CAF_DECL_UUID macro.
 * <p>
 * Example interface declaration:
 * <pre>
 * <code>
 * struct __declspcec(novtable) IMyInterface {
 *
 * 	CAF_DECL_UUID("6AECA0A4-C6B1-4A43-9769-C5A8F56F0B52")
 *
 * 	virtual void Foo() = 0;
 * };
 * </code>
 * </pre>
 */
struct __declspec(novtable) ICafObject {
	CAF_DECL_UUID("d285ff70-2314-11e0-ac64-0800200c9a66")

	/** @brief Increment the object's reference count */
	virtual void AddRef() = 0;

	/**
	 * @brief Decrement the object's reference count
	 * <p>
	 * The object will be destroyed when the count reaches zero.
	 */
	virtual void Release() = 0;

	/**
	 * @brief Retrieve an interface on the object
	 * @param IID the interface's UUID
	 * @param ppv the address into which the retrieve interface's pointer
	 * will be stored
	 */
	virtual void QueryInterface(const IID&, void** ppv) = 0;
};

#endif
