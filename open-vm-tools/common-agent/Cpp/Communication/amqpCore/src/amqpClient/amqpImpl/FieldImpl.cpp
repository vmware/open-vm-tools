/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "amqpClient/amqpImpl/FieldImpl.h"

using namespace Caf::AmqpClient;

FieldImpl::FieldImpl() :
		_type(AMQP_FIELD_TYPE_NOTSET),
		_value(NULL) {
}

FieldImpl::~FieldImpl() {
	if (_value) {
		g_variant_unref(_value);
	}
}

Field::AmqpFieldType FieldImpl::getAmqpType() const {
	return _type;
}

GVariant* FieldImpl::getValue() const {
	return _value;
}

void FieldImpl::setTypeAndValue(AmqpFieldType type, GVariant* value) {
	if (_value) {
		g_variant_unref(_value);
	}
	_type = type;
	_value = g_variant_ref_sink(value);
}
