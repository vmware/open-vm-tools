/*********************************************************
 * Copyright (C) 2012-2016 VMware, Inc. All rights reserved.
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

/*
 * rabbitmqProxyConst.h --
 *
 *    constants used in RabbitMQ Proxy DataMap etc.
 */

#ifndef _RABBITMQ_PROXY_CONST_H_
#define _RABBITMQ_PROXY_CONST_H_

#include "dataMap.h"

#define HOST_RABBITMQ_PROXY_LISTEN_PATH    "/var/run/rabbitmqproxy.uds"

/*
 * NOTE: changing the following IDs may break datamap encoding compatibility.
 */

/* field IDs */
enum {
   RMQPROXYDM_FLD_COMMAND              = 1,
   RMQPROXYDM_FLD_GUEST_CONN_ID        = 2,
   RMQPROXYDM_FLD_PAYLOAD              = 3,
   RMQPROXYDM_FLD_GUEST_VER_ID         = 4,
   RMQPROXYDM_FLD_QUEUE_PREFIX_ID      = 5,
   RMQPROXYDM_FLD_VC_UUID              = 6,
};


/* command types */
enum {
   COMMAND_DATA        = 1,
   COMMAND_CONNECT     = 2,
   COMMAND_CLOSE       = 3,
};

#endif  /* _RABBITMQ_PROXY_CONST_H_ */
