/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "amqpClient/api/CertInfo.h"

using namespace Caf::AmqpClient;

CertInfo::CertInfo() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CertInfo") {}

CertInfo::~CertInfo() {}

void CertInfo::initialize(
		const std::string& caCertPath,
		const std::string& clientCertPath,
		const std::string& clientKeyPath) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(caCertPath);
	CAF_CM_VALIDATE_STRING(clientCertPath);
	CAF_CM_VALIDATE_STRING(clientKeyPath);

	_caCertPath = caCertPath;
	_clientCertPath = clientCertPath;
	_clientKeyPath = clientKeyPath;

	_isInitialized = true;
}

std::string CertInfo::getCaCertPath() const {
	CAF_CM_FUNCNAME_VALIDATE("getCaCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _caCertPath;
}

std::string CertInfo::getClientCertPath() const {
	CAF_CM_FUNCNAME_VALIDATE("getClientCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _clientCertPath;
}

std::string CertInfo::getClientKeyPath() const {
	CAF_CM_FUNCNAME_VALIDATE("getClientKeyPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _clientKeyPath;
}
