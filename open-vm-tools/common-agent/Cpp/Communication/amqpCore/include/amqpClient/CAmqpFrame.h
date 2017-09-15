/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENT_CAMQPFRAME_H_
#define AMQPCLIENT_CAMQPFRAME_H_

#include "amqp.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Manages a set of channels for a connection.
 * Channels are indexed by channel number (<code><b>1.._channelMax</b></code>).
 */
class CAmqpFrame {
public:
	CAmqpFrame();
	virtual ~CAmqpFrame();

public:
	void initialize(
			const amqp_frame_t& frame);

	uint8_t getFrameType() const;
	amqp_channel_t getChannel() const;

	const amqp_method_t* const getPayloadAsMethod() const;

	uint16_t getHeaderClassId() const;
	uint64_t getHeaderBodySize() const;
	const amqp_basic_properties_t* const getHeaderProperties() const;

	const amqp_bytes_t* const getBodyFragment() const;

	void log(const std::string& prefix) const;

private:
	bool _isInitialized;

	uint8_t _frameType;
	amqp_channel_t _channel;

	amqp_method_t _method;

	uint16_t _propertiesClassId;
	uint64_t _propertiesBodySize;
	amqp_basic_properties_t* _propertiesDecoded;

	amqp_bytes_t _bodyFragment;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CAmqpFrame);
};
CAF_DECLARE_SMART_POINTER(CAmqpFrame);

}}

#endif /* AMQPCLIENT_CAMQPFRAME_H_ */
