/*
 *  Created on: Jun 4, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CACHINGCONNECTIONFACTORYOBJ_H_
#define CACHINGCONNECTIONFACTORYOBJ_H_

#include "IBean.h"

#include "amqpCore/CachingConnectionFactory.h"
#include "amqpClient/api/Connection.h"
#include "amqpCore/ConnectionListener.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObj
 * @brief An implementation of Caf::AmqpIntegration::ConnectionFactory that returns
 * the same connections from all calls, ignores calls to
 * Caf::AmqpClient::Connection::close and caches Caf::AmqpClient::Channel.
 * <p>
 * By default, only one channel will be cached, with additional requested channels
 * being created and disposed on demand.  Consider raising the cache size in
 * high-concurrency environments.
 * <p>
 * <b>NOTE: This factory requires explicit closing of all channels obtained from its
 * shared connection.</b>  Failure to close channels will disable channel reuse.
 * <p>
 * CachingConnectionFactory objects are created by inserting the following into
 * the application context:
 * <pre>
 *
 * \<bean
 * 	id="connectionFactory"
 * 	class="com.vmware.caf.comm.integration.amqp.caching.connection.factory">
 * 	\<property name="host" value="some.broker.host"/>
 * 	\<property name="connectionTimeout" value="4000"/>
 * 	\<property name="channelCacheSize" value="5"/>
 * \</bean>
 * </pre>
 * Properties:
 * <table border="1">
 * <tr><th>Property</th><th>Description</th></tr>
 * <tr><td>host</td>
 * <td>The broker host. By default the machine's host name (or <i>localhost</i>
 * if the host name cannot be determined).</td></tr>
 * <tr><td>port</td>
 * <td>The broker port. By default AmqpClient::DEFAULT_AMQP_PORT</td></tr>
 * <tr><td>virtualHost</td>
 * <td>The virtual host on the broker. By default AmqpClient::DEFAULT_VHOST</td></tr>
 * <tr><td>connectionTimeout</td>
 * <td>The connection timeout in milliseconds. A value of <i>zero</i> means
 * to wait indefinitely. By default 10 seconds.</td></tr>
 * <tr><td>channelCacheSize</td>
 * <td>The number of channels to cache. By default 1.</td></tr>
 * </table>
 */
class CachingConnectionFactoryObj :
	public TCafSubSystemObjectRoot<CachingConnectionFactoryObj>,
	public IBean,
	public ConnectionFactory
	{
	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdAmqpCachingConnectionFactory)

	CAF_BEGIN_INTERFACE_MAP(CachingConnectionFactoryObj)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(ConnectionFactory)
	CAF_END_INTERFACE_MAP()

public:
	CachingConnectionFactoryObj();
	virtual ~CachingConnectionFactoryObj();

public: // IBean
	void initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties);

	void terminateBean();

public: // ConnectionFactory
	SmartPtrConnection createConnection();
	std::string getProtocol();
	std::string getHost();
	uint32 getPort();
	std::string getVirtualHost();
	std::string getUsername();
	std::string getPassword();
	std::string getCaCertPath();
	std::string getClientCertPath();
	std::string getClientKeyPath();
	uint16 getRetries();
	uint16 getSecondsToWait();
	void addConnectionListener(const SmartPtrConnectionListener& listener);

private:
	SmartPtrCachingConnectionFactory _factory;
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CachingConnectionFactoryObj);
};

}}

#endif /* CACHINGCONNECTIONFACTORYOBJ_H_ */
