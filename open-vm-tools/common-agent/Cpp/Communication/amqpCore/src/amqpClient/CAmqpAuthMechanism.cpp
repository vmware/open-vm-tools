/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "amqpClient/CAmqpAuthMechanism.h"

using namespace Caf::AmqpClient;

CAmqpAuthMechanism::CAmqpAuthMechanism() :
	CAF_CM_INIT("CAmqpAuthMechanism") {
	CAF_CM_INIT_THREADSAFE;
}

CAmqpAuthMechanism::~CAmqpAuthMechanism() {
}

AMQPStatus CAmqpAuthMechanism::createClient(
		const std::string& username,
		const std::string& password) {
	CAF_CM_FUNCNAME_VALIDATE("createClient");
	CAF_CM_VALIDATE_STRING(username);
	// password is optional

	_username = username;
	_password = password;

	return AMQP_ERROR_OK;
}

std::string CAmqpAuthMechanism::getUsername() const {
	CAF_CM_FUNCNAME_VALIDATE("getUsername");
	CAF_CM_VALIDATE_STRING(_username);
	return _username;
}

std::string CAmqpAuthMechanism::getPassword() const {
	// password is optional
	return _password;
}
