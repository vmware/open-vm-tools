/*
 *	 Author: mdonahue
 *  Created: Jan 17, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCONFIGPARAMS_H_
#define CCONFIGPARAMS_H_

#include "Common/IConfigParams.h"

namespace Caf {

/*
 * This class wraps a GHashTable object where the key is a const char* and
 * the value is a GVariant*.
 *
 * The key and value pointers inserted must exist for the lifetime of this object.
 * Use the EKeyManagement and EValueMangement flags to control how this
 * object destroys the contained hash table.
 *
 */
class COMMONAGGREGATOR_LINKAGE CConfigParams : public IConfigParams {
public:
	typedef enum {
		EKeysUnmanaged, // Set if the caller will manage the lifetime of the keys
		EKeysManaged	// Set if this object is to detroy the keys upon destruction
	} EKeyManagement;

	typedef enum {
		EValuesUnmanaged,	// Set if the caller will manage the lifetime of the values
		EValuesManaged	// Set if this object is to detroy the values upon destruction
	} EValueManagement;

	CConfigParams();
	virtual ~CConfigParams();

	void initialize(
		const std::string& sectionName,
		EKeyManagement keyManagement,
		EValueManagement valueManagement);

public: // IConfigParams
	GVariant* lookup(
		const char* key,
		const EParamDisposition disposition = PARAM_REQUIRED) const;

	std::string getSectionName() const;

	void insert(const char* key, GVariant* value);

private:
	static void destroyKeyCallback(gpointer ptr);
	static void destroyValueCallback(gpointer ptr);

private:
	std::string _sectionName;
	GHashTable* _table;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CConfigParams);
};

CAF_DECLARE_SMART_POINTER(CConfigParams);

}

#endif /* CCONFIGPARAMS_H_ */
