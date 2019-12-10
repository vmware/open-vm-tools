/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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

/**
 * @file vmGuestAppMonitorLib.h
 *
 * This is the VMware Guest Application Monitor Lib,
 * an API used to communicate the health of one or more applications
 * running in a virtual machine to VMware infrastructure.
 *
 * @defgroup VMGuestAppMonitor
 *
 * @brief VMGuestAppMonitorLib A collection of routines used in the guest
 *                             by an application monitoring agent to
 *                             indicate the liveness of a monitored set of
 *                             applications.
 *
 *  @code
 *  VMGuestAppMonitor_Enable();
 *
 *  -- Call at least every 30 seconds
 *  VMGuestAppMonitor_MarkActive();
 *
 *  -- When finished monitoring
 *  VMGuestAppMonitor_Disable();
 *  @endcode
 *
 *  To signal an application failure, simply do not call
 *  VMGuestAppMonitor_MarkActive().
 *
 *  @endcode
 *
 *  @addtogroup VMGuestAppMonitor
 * @{
 */

#ifndef _VM_GUEST_APP_MONITOR_LIB_H_
#define _VM_GUEST_APP_MONITOR_LIB_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * VMGuestAppMonitorLib error codes.
 */

typedef enum {
   VMGUESTAPPMONITORLIB_ERROR_SUCCESS      = 0,  /**< No error. */
   VMGUESTAPPMONITORLIB_ERROR_OTHER,             /**< Other error */
   VMGUESTAPPMONITORLIB_ERROR_NOT_RUNNING_IN_VM, /**< Not running in a VM */
   VMGUESTAPPMONITORLIB_ERROR_NOT_ENABLED,       /**< Monitoring is not enabled */
   VMGUESTAPPMONITORLIB_ERROR_NOT_SUPPORTED,     /**< Monitoring is not supported */
} VMGuestAppMonitorLibError;


/*
 ******************************************************************************
 * VMGuestAppMonitor_Enable --                                          */ /**
 *
 * Enable application monitoring. After this call, the agent must
 * call VMGuestAppMonitor_MarkActive() at least once every 30
 * seconds or the application will be viewed as having failed.
 *
 * @return VMGUESTAPPMONITORLIB_ERROR_SUCCESS if monitoring has been enabled.
 *
 ******************************************************************************
 */

VMGuestAppMonitorLibError
VMGuestAppMonitor_Enable(void);


/*
 ******************************************************************************
 * VMGuestAppMonitor_Disable --                                         */ /**
 *
 * Disable application monitoring.
 *
 * @return  TRUE if monitoring has been disabled.
 *
 ******************************************************************************
 */

VMGuestAppMonitorLibError
VMGuestAppMonitor_Disable(void);


/*
 ******************************************************************************
 * VMGuestAppMonitor_IsEnabled --                                       */ /**
 *
 * Return the current state of application monitoring.
 *
 * @return  1 (TRUE) if monitoring is enabled.
 *
 ******************************************************************************
 */

int
VMGuestAppMonitor_IsEnabled(void);


/*
 ******************************************************************************
 * VMGuestAppMonitor_MarkActive --                                      */ /**
 *
 * Marks the application as active. This function needs to be called
 * at least once every 30 seconds while application monitoring is
 * enabled or HA will determine that the application has failed.
 *
 * @return  VMGUESTAPPMONITORLIB_ERROR_SUCCESS if successful and
 *          VMGUESTAPPMONITORLIB_ERROR_NOT_ENABLED if monitoring is not enabled.
 *
 ******************************************************************************
 */

VMGuestAppMonitorLibError
VMGuestAppMonitor_MarkActive(void);


/*
 ******************************************************************************
 * VMGuestAppMonitor_GetAppStatus --                                    */ /**
 *
 * Return the current status recorded for the application.
 *
 * @return  the application status. The caller must free the result.
 *
 ******************************************************************************
 */

char *
VMGuestAppMonitor_GetAppStatus(void);

/*
 ******************************************************************************
 * VMGuestAppMonitor_PostAppState --                                    */ /**
 *
 * Post application state.
 *
 * @return  VMGUESTAPPMONITORLIB_ERROR_SUCCESS if successful
 *
 ******************************************************************************
 */


VMGuestAppMonitorLibError
VMGuestAppMonitor_PostAppState(const char *state);


/*
 ******************************************************************************
 * VMGuestAppMonitor_Free --                                            */ /**
 *
 * Free the result of VMGuestAppMonitor_GetAppStatus.
 *
 * @param[in] str  Pointer to the memory to be freed.
 *
 ******************************************************************************
 */

void
VMGuestAppMonitor_Free(char *str);

#ifdef __cplusplus
}
#endif

#endif /* _VM_GUEST_APP_MONITOR_LIB_H_ */

/** @} */
