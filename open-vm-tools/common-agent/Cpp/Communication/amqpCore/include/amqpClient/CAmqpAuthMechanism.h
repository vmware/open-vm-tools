/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENT_CAMQPAUTHMECHANISM_H_
#define AMQPCLIENT_CAMQPAUTHMECHANISM_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Manages a set of channels for a connection.
 * Channels are indexed by channel number (<code><b>1.._channelMax</b></code>).
 */
class CAmqpAuthMechanism {
public:
	CAmqpAuthMechanism();
	virtual ~CAmqpAuthMechanism();

public:
	AMQPStatus createClient(
			const std::string& username,
			const std::string& password);

	std::string getUsername() const;
	std::string getPassword() const;

private:
	std::string _username;
	std::string _password;

	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CAmqpAuthMechanism);
};
CAF_DECLARE_SMART_POINTER(CAmqpAuthMechanism);

}}

#endif /* AMQPCLIENT_CAMQPAUTHMECHANISM_H_ */
