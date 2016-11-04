/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "amqpClient/CAmqpFrame.h"

using namespace Caf::AmqpClient;

CAmqpFrame::CAmqpFrame() :
	_isInitialized(false),
	_frameType(0),
	_channel(0),
	_propertiesClassId(0),
	_propertiesBodySize(0),
	_propertiesDecoded(NULL),
	CAF_CM_INIT_LOG("CAmqpFrame") {
	_method.decoded = NULL;
	_method.id = 0;
	_bodyFragment.bytes = NULL;
	_bodyFragment.len = 0;
}

CAmqpFrame::~CAmqpFrame() {
}

void CAmqpFrame::initialize(
		const amqp_frame_t& frame) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_channel = frame.channel;
	_frameType = frame.frame_type;

	switch (_frameType) {
		case AMQP_FRAME_METHOD: {
			_method = frame.payload.method;
		}
		break;

		case AMQP_FRAME_HEADER: {
			_propertiesClassId = frame.payload.properties.class_id;
			_propertiesBodySize = frame.payload.properties.body_size;
			CAF_CM_VALIDATE_NOTZERO(static_cast<const int32>(_propertiesBodySize));
			CAF_CM_VALIDATE_PTR(frame.payload.properties.decoded);

			_propertiesDecoded = static_cast<amqp_basic_properties_t*>(
					frame.payload.properties.decoded);
		}
		break;

		case AMQP_FRAME_BODY: {
			_bodyFragment = frame.payload.body_fragment;
		}
		break;

		default:
			CAF_CM_LOG_ERROR_VA1("Unknown frame type - %d", frame.frame_type);
	}

	_isInitialized = true;
}

uint8_t CAmqpFrame::getFrameType() const {
	CAF_CM_FUNCNAME_VALIDATE("getFrameType");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _frameType;
}

amqp_channel_t CAmqpFrame::getChannel() const {
	CAF_CM_FUNCNAME_VALIDATE("getChannel");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _channel;
}

const amqp_method_t* const CAmqpFrame::getPayloadAsMethod() const {
	CAF_CM_FUNCNAME_VALIDATE("getPayloadAsMethod");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_BOOL(_frameType == AMQP_FRAME_METHOD);
	return &_method;
}

uint16_t CAmqpFrame::getHeaderClassId() const {
	CAF_CM_FUNCNAME_VALIDATE("getHeaderClassId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_BOOL(_frameType == AMQP_FRAME_HEADER);
	return _propertiesClassId;
}

uint64_t CAmqpFrame::getHeaderBodySize() const {
	CAF_CM_FUNCNAME_VALIDATE("getHeaderBodySize");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_BOOL(_frameType == AMQP_FRAME_HEADER);
	return _propertiesBodySize;
}

const amqp_basic_properties_t* const CAmqpFrame::getHeaderProperties() const {
	CAF_CM_FUNCNAME_VALIDATE("getPayloadAsMethod");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_BOOL(_frameType == AMQP_FRAME_HEADER);
	return _propertiesDecoded;
}

const amqp_bytes_t* const CAmqpFrame::getBodyFragment() const {
	CAF_CM_FUNCNAME_VALIDATE("getBodyFragment");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_BOOL(_frameType == AMQP_FRAME_BODY);
	return &_bodyFragment;
}

void CAmqpFrame::log(
		const std::string& prefix) const {
	CAF_CM_FUNCNAME_VALIDATE("log");
	CAF_CM_VALIDATE_STRING(prefix);

	switch (_frameType) {
		case AMQP_FRAME_METHOD:
			CAF_CM_LOG_DEBUG_VA4(
					"%s - type: AMQP_FRAME_METHOD, channel: %d, methodId: 0x%08x, methodName: %s",
					prefix.c_str(), _channel, _method.id,
					amqp_method_name(_method.id));
		break;

		case AMQP_FRAME_HEADER:
			CAF_CM_LOG_DEBUG_VA4(
					"%s - type: AMQP_FRAME_HEADER, channel: %d, classId: 0x%04x, bodySize: %d",
					prefix.c_str(), _channel, _propertiesClassId,
					_propertiesBodySize);
		break;

		case AMQP_FRAME_BODY:
			CAF_CM_LOG_DEBUG_VA3(
					"%s - type: AMQP_FRAME_BODY, channel: %d, bodyLen: %d",
					prefix.c_str(), _channel, _bodyFragment.len);
//			AmqpCommon::dumpMessageBody(_bodyFragment.bytes, _bodyFragment.len);
		break;

		default:
			CAF_CM_LOG_ERROR_VA1("Unknown frame type - %d", _frameType);
	}
}
