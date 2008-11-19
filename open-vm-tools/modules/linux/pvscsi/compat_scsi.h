/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

#ifndef __COMPAT_SCSI_H__
#   define __COMPAT_SCSI_H__


/* The scsi_bufflen() API appeared somewhere in time --hpreg */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
#   define scsi_bufflen(cmd) ((cmd)->request_bufflen)
#   define scsi_sg_count(cmd) ((cmd)->use_sg)
#   define scsi_sglist(cmd) ((struct scatterlist *)(cmd)->request_buffer)
#   define scsi_set_resid(cmd, _resid) ((cmd)->resid = _resid)
#endif

/*
 * Using scsi_sglist to access the request buffer looks strange
 * so instead we define this macro.  What happened is later kernel
 * put all SCSI data in sglists, since it simplifies passing buffers
 */
#define scsi_request_buffer(cmd) scsi_sglist(cmd)

#endif /* __COMPAT_SCSI_H__ */
