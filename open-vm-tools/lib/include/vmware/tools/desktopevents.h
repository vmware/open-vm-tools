/*********************************************************
 * Copyright (C) 2009-2019 VMware, Inc. All rights reserved.
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


/**
 * Name of the message that can be sent to the vm3dservice user daemons
 * for shutdown.
 */
#define DESKTOP_VM3DSERVICE_SHUTDOWN "VMdesktopEventsVM3DServiceShutdownMsg"

#else   // !WIN32

/**
 * Signal emitted upon X I/O error callback firing.
 *
 * @param[in]   src     The source object.
 * @param[in]   ctx     ToolsAppCtx *: the application context.
 * @param[in]   data    Client data.
 */
#define TOOLS_CORE_SIG_XIOERROR "tcs_de_xioerror"

/**
 * Signal emitted upon SmcCallbacks::save_yourself.
 *
 * @param[in]   src             The source object.
 * @param[in]   ctx             ToolsAppCtx *: the application context.
 * @parma[in]   saveType        Refer to SMlib.xml.
 * @param[in]   shutdown        0 = checkpoint, 1 = shutdown.
 * @param[in]   interactStyle   May interact with user?
 * @param[in]   fast            Shutdown as quickly as possible.
 * @param[in]   data            Client data.
 */
#define TOOLS_CORE_SIG_XSM_SAVE_YOURSELF "tcs_de_xsm_save_yourself"

/**
 * Signal emitted upon SmcCallbacks::die.
 *
 * @param[in]   src     The source object.
 * @param[in]   ctx     ToolsAppCtx *: the application context.
 * @param[in]   data    Client data.
 */
#define TOOLS_CORE_SIG_XSM_DIE "tcs_de_xsm_die"

/**
 * Signal emitted upon SmcCallbacks::save_complete.
 *
 * @param[in]   src     The source object.
 * @param[in]   ctx     ToolsAppCtx *: the application context.
 * @param[in]   data    Client data.
 */
#define TOOLS_CORE_SIG_XSM_SAVE_COMPLETE "tcs_de_xsm_save_complete"

/**
 * Signal emitted upon SmcCallbacks::shutdown_cancelled.
 *
 * @param[in]   src     The source object.
 * @param[in]   ctx     ToolsAppCtx *: the application context.
 * @param[in]   data    Client data.
 */
#define TOOLS_CORE_SIG_XSM_SHUTDOWN_CANCELLED "tcs_de_xsm_shutdown_cancelled"

#endif  // if defined(_WIN32)

/** @} */

#endif /* _DESKTOPEVENTS_H_ */

