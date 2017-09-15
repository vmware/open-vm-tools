/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef FIELDIMPL_H_
#define FIELDIMPL_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of a Field object
 */
class FieldImpl : public Field {
public:
	FieldImpl();
	virtual ~FieldImpl();

public: // Field
	AmqpFieldType getAmqpType() const;

	GVariant* getValue() const;

	void setTypeAndValue(AmqpFieldType type, GVariant *value);

private:
	AmqpFieldType _type;
	GVariant *_value;
	CAF_CM_DECLARE_NOCOPY(FieldImpl);
};
CAF_DECLARE_SMART_POINTER(FieldImpl);

}}

#endif
