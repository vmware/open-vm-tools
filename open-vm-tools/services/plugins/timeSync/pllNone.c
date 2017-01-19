/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * @file pllNone.c
 *
 * Null implementation of the NTP PLL.
 */

#include "timeSync.h"

#include "vm_assert.h"


/*
 ******************************************************************************
 * TimeSync_PLLSupported --                                             */ /**
 *
 * Report whether the platform supports an NTP style Type-II Phase Locked
 * Loop for correcting the time.
 *
 * @return TRUE iff NTP Phase Locked Loop is supported.
 *
 ******************************************************************************
 */

Bool
TimeSync_PLLSupported(void)
{
   return FALSE;
}


/*
 ******************************************************************************
 * TimeSync_PLLSetFrequency --                                          */ /**
 *
 * Set the frequency of the PLL.  
 *
 * @param[in] ppmCorrection  The parts per million error that should be 
 *                           corrected.  This value is the ppm shifted 
 *                           left by 16 to match NTP.
 *
 * @return FALSE
 *
 ******************************************************************************
 */

Bool
TimeSync_PLLSetFrequency(int64 ppmCorrection)
{
   NOT_IMPLEMENTED();
   return FALSE;
}


/*
 ******************************************************************************
 * TimeSync_PLLUpdate --                                                */ /**
 *
 * Updates the PLL with a new offset.
 *
 * @param[in] offset         The offset between the host and the guest.
 *
 * @return FALSE
 *
 ******************************************************************************
 */

Bool
TimeSync_PLLUpdate(int64 offset)
{
   NOT_IMPLEMENTED();
   return FALSE;
}
