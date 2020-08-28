/*********************************************************
 * Copyright (C) 2007-2016,2019 VMware, Inc. All rights reserved.
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
 * syncDriverIoc.h --
 *
 * ioctl commands used by the sync driver on Unix systems.
 *
 * SYNC_IOC_FREEZE:     Freezes the provided paths.
 * SYNC_IOC_THAW:       Thaws frozen block devices after a FREEZE ioctl.
 * SYNC_IOC_QUERY:      Returns the total number of frozen devices (not
 *                      specific to the fd used).
 */

#ifndef _SYNCDRIVERIOC_H_
#define _SYNCDRIVERIOC_H_

#ifdef __linux__

# include <linux/ioctl.h>

# define SYNC_IOC_FREEZE      _IOW(0xF5,0x01,const char *)
# define SYNC_IOC_THAW        _IO(0xF5,0x02)
# define SYNC_IOC_QUERY       _IOR(0xF5,0x03,int)

#else

# error "Driver not yet implemented for this OS."

#endif

#endif /* _SYNCDRIVERIOC_H_ */

