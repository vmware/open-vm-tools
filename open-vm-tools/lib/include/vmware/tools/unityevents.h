/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

#ifndef _UNITYEVENTS_H_
#define _UNITYEVENTS_H_

/**
 * @file unityevents.h
 *
 * Public interface for the "unity" plugin. This plugin (via libunity) exposes
 * an event published when unity is entered and exited.
 *
 * @addtogroup vmtools_plugins
 * @{
 */

/**
 * Signal sent when unity is entered or exited.
 *
 * @param[in]  src      The source object.
 * @param[in]  enter    If TRUE, unity has been entered. If FALSE, it
 *                      has been exited.
 */
#define UNITY_SIG_ENTER_LEAVE_UNITY "unity_enter_leave_unity"

/** @} */

#endif /* _UNITYEVENTS_H_ */

