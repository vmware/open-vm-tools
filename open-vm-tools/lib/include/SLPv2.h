/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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

#ifndef _SLPV2_H_
#define _SLPV2_H_

struct sockaddr_in;

/*
 * These functions are used by a client to discover services.
 */

typedef void (*SLPv2DiscoveryCallbackProcType)(struct sockaddr_in *sin,
                                               int sin_len,
                                               const char *url,
                                               const char *attributes,
                                               Bool *cancelSearch,
                                               void *context);

void SLPv2_DiscoverServices(char *serviceType,
                            int32 timeoutInUSecs,
                            SLPv2DiscoveryCallbackProcType callBackProc,
                            void *callBackProcContext);


/*
 * These functions are used by a server to manage a list of local service 
 * names that it advertises to the network.
 */

Bool SLPv2Service_Initialize(void);

void SLPv2Service_Shutdown(void);

Bool SLPv2Service_Announce(char *servicename, char *serviceProperties, int options);


#endif // _SLPV2_H_
