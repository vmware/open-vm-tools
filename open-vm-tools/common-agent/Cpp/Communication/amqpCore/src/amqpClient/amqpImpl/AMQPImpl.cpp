/*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/amqpImpl/IContentHeader.h"
#include "amqpClient/amqpImpl/IMethod.h"
#include "Exception/CCafException.h"
#include "AMQPImpl.h"
#include "amqpImplInt.h"

namespace Caf { namespace AmqpClient {
typedef Caf::AmqpClient::SmartPtrIMethod (*FN_CREATOR)();
typedef std::map<amqp_method_number_t, FN_CREATOR> MethodCreatorMap;
typedef MethodCreatorMap::value_type creatorEntry;
}}

using namespace Caf::AmqpClient;

const creatorEntry creatorEntries[] = {
		creatorEntry(AMQP_BASIC_GET_OK_METHOD, BasicGetOkMethod::Creator),
		creatorEntry(AMQP_BASIC_GET_EMPTY_METHOD, BasicGetEmptyMethod::Creator),
		creatorEntry(AMQP_BASIC_CONSUME_OK_METHOD, BasicConsumeOkMethod::Creator),
		creatorEntry(AMQP_BASIC_DELIVER_METHOD, BasicDeliverMethod::Creator),
		creatorEntry(AMQP_BASIC_CANCEL_OK_METHOD, BasicCancelOkMethod::Creator),
		creatorEntry(AMQP_BASIC_RETURN_METHOD, BasicReturnMethod::Creator),
		creatorEntry(AMQP_BASIC_RECOVER_OK_METHOD, BasicRecoverOkMethod::Creator),
		creatorEntry(AMQP_BASIC_QOS_OK_METHOD, BasicQosOkMethod::Creator),
		creatorEntry(AMQP_CHANNEL_OPEN_OK_METHOD, ChannelOpenOkMethod::Creator),
		creatorEntry(AMQP_CHANNEL_CLOSE_OK_METHOD, ChannelCloseOkFromServerMethod::Creator),
		creatorEntry(AMQP_CHANNEL_CLOSE_METHOD, ChannelCloseMethod::Creator),
		creatorEntry(AMQP_EXCHANGE_DECLARE_OK_METHOD, ExchangeDeclareOkMethod::Creator),
		creatorEntry(AMQP_EXCHANGE_DELETE_OK_METHOD, ExchangeDeleteOkMethod::Creator),
		creatorEntry(AMQP_QUEUE_DECLARE_OK_METHOD, QueueDeclareOkMethod::Creator),
		creatorEntry(AMQP_QUEUE_DELETE_OK_METHOD, QueueDeleteOkMethod::Creator),
		creatorEntry(AMQP_QUEUE_PURGE_OK_METHOD, QueuePurgeOkMethod::Creator),
		creatorEntry(AMQP_QUEUE_BIND_OK_METHOD, QueueBindOkMethod::Creator),
		creatorEntry(AMQP_QUEUE_UNBIND_OK_METHOD, QueueUnbindOkMethod::Creator)
		};

const MethodCreatorMap creatorMap(
	creatorEntries,
	creatorEntries + (sizeof(creatorEntries)/sizeof(creatorEntries[0])));

SmartPtrIMethod AMQPImpl::methodFromFrame(const amqp_method_t * const method) {
	CAF_CM_STATIC_FUNC("AMQPImpl", "methodFromFrame");
	CAF_CM_VALIDATE_PTR(method);

	SmartPtrIMethod methodObj;
	MethodCreatorMap::const_iterator methodCreator = creatorMap.find(method->id);
	if (methodCreator != creatorMap.end()) {
		methodObj = (*(methodCreator->second))();
	} else {
		const uint16 classId = (uint16)((method->id & 0xffff0000) >> 16);
		const uint16 methodId = (uint16)(method->id & 0x0000ffff);
		CAF_CM_EXCEPTIONEX_VA2(
				AmqpExceptions::UnknownClassOrMethodException,
				0,
				"[class=0x%04X][id=0x%04X]",
				classId,
				methodId);
	}
	methodObj->init(method);
	return methodObj;
}

SmartPtrIContentHeader AMQPImpl::headerFromFrame(
	const SmartPtrCAmqpFrame& frame) {
	CAF_CM_STATIC_FUNC("AMQPImpl", "headerFromFrame");
	CAF_CM_VALIDATE_PTR(frame);

	SmartPtrIContentHeader header;
	if (frame->getHeaderClassId() == AMQP_BASIC_CLASS) {
		SmartPtrBasicProperties propsObj;
		propsObj.CreateInstance();
		propsObj->init(frame);
		header = propsObj;
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				AmqpExceptions::UnknownClassOrMethodException,
				0,
				"[class=0x%04X]",
				frame->getHeaderClassId());
	}

	return header;
}
