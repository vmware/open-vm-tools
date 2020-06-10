/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

#ifndef _SERVICEDISCOVERY_H_
#define _SERVICEDISCOVERY_H_

/**
 * @file serviceDiscovery.h
 *
 * Common declarations that aid in sending services information
 * from 'serviceDiscovery' plugin in 'VMware Tools' to the host.
 */


/*
 * Namespace DB used for service discovery
 */
#define SERVICE_DISCOVERY_NAMESPACE_DB_NAME "com.vmware.vrops.sdmp"

/*
 * ready - used to identify if data is succesfully written to Namespace DB
 * signal - signal send by sdmp client for plugin to start data collection
 */
#define SERVICE_DISCOVERY_KEY_READY "ready"
#define SERVICE_DISCOVERY_KEY_SIGNAL "signal"

/*
 * keys for types of service data collected by plugin.
 */
#define SERVICE_DISCOVERY_KEY_PROCESSES "listening-process-info"
#define SERVICE_DISCOVERY_KEY_CONNECTIONS "connection-info"
#define SERVICE_DISCOVERY_KEY_PERFORMANCE_METRICS                              \
   "listening-process-perf-metrics"
#define SERVICE_DISCOVERY_KEY_VERSIONS "versions"

/*
 * keys for types of service data collected by plugin from Windows guest
 */
#define SERVICE_DISCOVERY_WIN_KEY_RELATIONSHIP "pid-to-ppid"
#define SERVICE_DISCOVERY_WIN_KEY_NET "net"
#define SERVICE_DISCOVERY_WIN_KEY_IIS_PORTS "iis-ports-info"

#endif /* _SERVICEDISCOVERY_H_ */
