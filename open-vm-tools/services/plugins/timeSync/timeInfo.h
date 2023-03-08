/*********************************************************
 * Copyright (C) 2022 VMware, Inc. All rights reserved.
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

#ifndef _TIMEINFO_H_
#define _TIMEINFO_H_

/**
 * @file timeInfo.h
 *
 * Functions and definitions related to TimeInfo.
 */

void TimeInfo_Init(ToolsAppCtx *ctx);
void TimeInfo_Shutdown(void);
gboolean TimeInfo_TcloHandler(RpcInData *data);

#endif /* _TIMEINFO_H_ */

