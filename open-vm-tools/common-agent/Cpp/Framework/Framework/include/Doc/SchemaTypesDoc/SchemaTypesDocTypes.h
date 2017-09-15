/*
 *	 Author: bwilliams
 *  Created: April 6, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef SCHEMATYPESDOCTYPES_H_
#define SCHEMATYPESDOCTYPES_H_

#include "Doc/SchemaTypesDoc/SchemaTypesDocTypes.h"

namespace Caf {
	typedef enum {
		OPERATOR_NONE,
		OPERATOR_EQUAL
	} OPERATOR_TYPE;

	typedef enum {
		ARITY_NONE,
		ARITY_UNSIGNED_BYTE = 2
	} ARITY_TYPE;

	typedef enum {
		VALIDATOR_NONE,
		VALIDATOR_ENUM,
		VALIDATOR_RANGE,
		VALIDATOR_REGEX,
		VALIDATOR_CUSTOM
	} VALIDATOR_TYPE;
}

#endif /* SCHEMATYPESDOCTYPES_H_ */
