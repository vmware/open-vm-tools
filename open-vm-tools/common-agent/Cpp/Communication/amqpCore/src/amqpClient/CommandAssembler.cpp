/*
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */
#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/amqpImpl/IContentHeader.h"
#include "amqpClient/amqpImpl/IMethod.h"
#include "amqpClient/CommandAssembler.h"
#include "Exception/CCafException.h"

using namespace Caf::AmqpClient;

CommandAssembler::CommandAssembler() :
	_isInitialized(false),
	_state(EXPECTING_METHOD),
	_remainingBodyBytes(0),
	_bodyLength(0),
	CAF_CM_INIT("CommandAssembler") {
}

CommandAssembler::~CommandAssembler() {
}

void CommandAssembler::init() {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_isInitialized = true;
}

bool CommandAssembler::handleFrame(const SmartPtrCAmqpFrame& frame) {
	CAF_CM_FUNCNAME("handleFrame");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_PTR(frame);

	switch (_state) {
	case EXPECTING_METHOD:
		consumeMethodFrame(frame);
		break;

	case EXPECTING_CONTENT_HEADER:
		consumeHeaderFrame(frame);
		break;

	case EXPECTING_CONTENT_BODY:
		consumeBodyFrame(frame);
		break;

	default:
		CAF_CM_EXCEPTIONEX_VA2(
				IllegalStateException,
				0,
				"Bad command state [channel=%d][state=%d]",
				frame->getChannel(),
				_state);
		break;
	}
	return isComplete();
}

bool CommandAssembler::isComplete() {
	CAF_CM_FUNCNAME_VALIDATE("isComplete");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return (_state == COMPLETE);
}

SmartPtrIMethod CommandAssembler::getMethod() {
	CAF_CM_FUNCNAME_VALIDATE("getMethod");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _method;
}

SmartPtrCDynamicByteArray CommandAssembler::getContentBody() {
	CAF_CM_FUNCNAME_VALIDATE("getContentBody");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return coalesceContentBody();
}

SmartPtrIContentHeader CommandAssembler::getContentHeader() {
	CAF_CM_FUNCNAME_VALIDATE("getContentHeader");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _contentHeader;
}

void CommandAssembler::consumeBodyFrame(const SmartPtrCAmqpFrame& frame) {
	CAF_CM_FUNCNAME("consumeBodyFrame");
	if (frame->getFrameType() == AMQP_FRAME_BODY) {
		const amqp_bytes_t * const fragment = frame->getBodyFragment();
		_remainingBodyBytes -= static_cast<uint32>(fragment->len);
		updateContentBodyState();
		appendBodyFragment(fragment);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				AmqpExceptions::UnexpectedFrameException,
				0,
				"Expected an AMQP body frame. Received type %d",
				frame->getFrameType());
	}
}

void CommandAssembler::consumeHeaderFrame(const SmartPtrCAmqpFrame& frame) {
	CAF_CM_FUNCNAME("consumeHeaderFrame");
	if (frame->getFrameType() == AMQP_FRAME_HEADER) {
		_contentHeader = AMQPImpl::headerFromFrame(frame);
		_remainingBodyBytes = static_cast<uint32>(_contentHeader->getBodySize());
		updateContentBodyState();
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				AmqpExceptions::UnexpectedFrameException,
				0,
				"Expected an AMQP header frame. Received type %d",
				frame->getFrameType());
	}
}

void CommandAssembler::consumeMethodFrame(const SmartPtrCAmqpFrame& frame) {
	CAF_CM_FUNCNAME("consumeMethodFrame");
	if (frame->getFrameType() == AMQP_FRAME_METHOD) {
		_method = AMQPImpl::methodFromFrame(frame->getPayloadAsMethod());
		_state = _method->hasContent() ? EXPECTING_CONTENT_HEADER : COMPLETE;
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				AmqpExceptions::UnexpectedFrameException,
				0,
				"Expected an AMQP method frame. Received type %d",
				frame->getFrameType());
	}
}

void CommandAssembler::updateContentBodyState() {
	_state = (_remainingBodyBytes > 0) ? EXPECTING_CONTENT_BODY : COMPLETE;
}

void CommandAssembler::appendBodyFragment(const amqp_bytes_t * const fragment) {
	if (fragment && fragment->len) {
		SmartPtrCDynamicByteArray fragmentData;
		fragmentData.CreateInstance();
		fragmentData->allocateBytes(fragment->len);
		fragmentData->memCpy(fragment->bytes, fragment->len);
		_bodyCollection.push_back(fragmentData);
		_bodyLength += static_cast<uint32>(fragment->len);
	}
}

SmartPtrCDynamicByteArray CommandAssembler::coalesceContentBody() {
	SmartPtrCDynamicByteArray body;
	body.CreateInstance();
	if (_bodyLength) {
		if (_bodyCollection.size() == 1) {
			body = _bodyCollection.front();
		} else {
			body->allocateBytes(_bodyLength);
			for (TSmartIterator<CBodyCollection> fragment(_bodyCollection);
					fragment;
					fragment++) {
				body->memAppend(fragment->getPtr(), fragment->getByteCount());
				*fragment = NULL;
			}
		}
		_bodyCollection.clear();
		_bodyCollection.push_back(body);
	}
	return body;
}
