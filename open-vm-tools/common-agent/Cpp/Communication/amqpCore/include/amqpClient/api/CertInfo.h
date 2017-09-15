/*
 *  Created on: May 4, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_CERTINFO_H_
#define AMQPCLIENTAPI_CERTINFO_H_

namespace Caf { namespace AmqpClient {

/**
 * @author bwilliams
 * @brief A representation of Cert Info
 */
class CertInfo {
public:
	CertInfo();
	virtual ~CertInfo();

public:
	void initialize(
			const std::string& caCertPath,
			const std::string& clientCertPath,
			const std::string& clientKeyPath);

	std::string getCaCertPath() const;

	std::string getClientCertPath() const;

	std::string getClientKeyPath() const;

private:
	bool _isInitialized;
	std::string _caCertPath;
	std::string _clientCertPath;
	std::string _clientKeyPath;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CertInfo);
};
CAF_DECLARE_SMART_POINTER(CertInfo);

}}

#endif
