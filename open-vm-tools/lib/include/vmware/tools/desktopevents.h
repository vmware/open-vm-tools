/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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

#ifndef _DESKTOPEVENTS_H_
#define _DESKTOPEVENTS_H_

/**
 * @file desktopevents.h
 *
 * Public interface for the "switchUser" plugin. This plugin exposes some
 * user session-related events to other plugins.
 *
 * Aside from the functionality exposed in this file, the plugin also
 * emits the TOOLS_CORE_SIG_SESSION_CHANGE signal (which is handled
 * by vmtoolsd automatically when it's run from within the SCM;
 * see plugin.h).
 *
 * Currently the plugin is only available on Win32.
 *
 * @addtogroup vmtools_plugins
 * @{
 */

#if defined(_WIN32)

/**
 * Signal sent when a "desktop switch" event is detected.
 *
 * Defined in desktopevents.h.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      ToolsAppCtx *: the application context.
 * @param[in]  data     Client data.
 */
#define TOOLS_CORE_SIG_DESKTOP_SWITCH "tcs_desktop_switch"


/**
 * Name of the message that can be sent to the desktop events window to
 * shut down the vmusr instance.
 */
#define DESKTOP_EVENTS_SHUTDOWN "VMdesktopEventsShutdownMsg"

#endif

/** @} */

#endif /* _DESKTOPEVENTS_H_ */

