/*********************************************************
 * Copyright (C) 2016-2021 VMware, Inc. All rights reserved.
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
/* Authors:
 * Thomas Hellstrom <thellstrom@vmware.com>
 */

#ifndef _RESOLUTION_DL_H_
#define _RESOLUTION_DL_H_
#ifdef ENABLE_RESOLUTIONKMS

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#ifndef HAVE_LIBUDEV

#include <stdint.h>

struct udev;
struct udev_device;
struct udev_enumerate;

/*
 * This struct holds the function pointers we use in libudev.1. Libudev is
 * Lgpl 2.1 (or later) licensed, and according to lgpl 2.1, section 5, while
 * we use materials copied from header files, we restrict ourselves to
 * data structure layouts and small macros and thus, while this should be
 * considered a derivative work, the use of the object file is unrestricted.
 * An executable linked with libudev is however subject to lgpl license
 * restrictions.
 */
struct Udev1Interface {
    const char *
    (*_device_get_devnode)(struct udev_device *);
    struct udev_device *
    (*_device_get_parent_with_subsystem_devtype)(struct udev_device *,
						 const char *, const char *);
    const char *
    (*_device_get_sysattr_value)(struct udev_device *, const char *);
    struct udev_device *
    (*_device_new_from_syspath)(struct udev *, const char *);
    struct udev_device *
    (*_device_unref)(struct udev_device *);
    int
    (*_enumerate_add_match_property)(struct udev_enumerate *,const char *,
				      const char *);
    int
    (*_enumerate_add_match_subsystem)(struct udev_enumerate *,const char *);
    struct udev_list_entry *
    (*_enumerate_get_list_entry)(struct udev_enumerate *);
    struct udev_enumerate *
    (*_enumerate_new)(struct udev *udev);
    int
    (*_enumerate_scan_devices)(struct udev_enumerate *);
    struct udev_enumerate *
    (*_enumerate_unref)(struct udev_enumerate *);
    const char *
    (*_list_entry_get_name)(struct udev_list_entry *);
    struct udev_list_entry *
    (*_list_entry_get_next)(struct udev_list_entry *);
    struct udev *
    (*_new)(void);
    struct udev *
    (*_unref)(struct udev *);
};

#define udevi_list_entry_foreach(_udevi, list_entry, first_entry)	\
    for (list_entry = first_entry;					\
	 list_entry != NULL;						\
	 list_entry = (_udevi)->_list_entry_get_next(list_entry))

/*
 * This struct is originally defined in xf86drm.h which is MIT licenced.
 * However, this should not be considered a substantial part of the software,
 * and this struct is not subject to the license header of this file.
 */
typedef struct _drmVersion {
    int     version_major;        /* Major version */
    int     version_minor;        /* Minor version */
    int     version_patchlevel;   /* Patch level */
    int     name_len;	          /* Length of name buffer */
    char    *name;	          /* Name of driver */
    int     date_len;             /* Length of date buffer */
    char    *date;                /* User-space buffer to hold date */
    int     desc_len;	          /* Length of desc buffer */
    char    *desc;                /* User-space buffer to hold desc */
} drmVersion, *drmVersionPtr;

#define DRM_VMW_UPDATE_LAYOUT        20

/*
 * These structs are originally defined in vmwgfx_drm.h, which is dual
 * MIT and GPLv2 licenced. However, VMWare Inc. owns the copyright and it's
 * should not be considered a substantial part of the software.
 * However this struct is not subject to the license header of this file.
 */
/**
 * struct drm_vmw_rect
 *
 * Defines a rectangle. Used in the overlay ioctl to define
 * source and destination rectangle.
 */

struct drm_vmw_rect {
	int32_t x;
	int32_t y;
	uint32_t w;
	uint32_t h;
};

/**
 * struct drm_vmw_update_layout_arg
 *
 * @num_outputs: number of active connectors
 * @rects: pointer to array of drm_vmw_rect cast to an uint64_t
 *
 * Input argument to the DRM_VMW_UPDATE_LAYOUT Ioctl.
 */
struct drm_vmw_update_layout_arg {
	uint32_t num_outputs;
	uint32_t pad64;
	uint64_t rects;
};

/*
 * This struct holds the function pointers we use in libdrm.2. The
 * libdrm API is MIT licenced and what we do here should not be considered
 * a substantial part of the software:
 * However this struct is not subject to the license header of this file.
 */
struct Drm2Interface {
    int (*Open)(const char *, const char *);
    int (*Close)(int);
    drmVersionPtr (*GetVersion)(int fd);
    void (*FreeVersion)(drmVersionPtr);
    int (*DropMaster)(int fd);
    int (*CommandWrite)(int fd, unsigned long drmCommandIndex, void *data,
			unsigned long size);
};

extern struct Udev1Interface *udevi;
extern struct Drm2Interface *drmi;

void resolutionDLClose(void);
int resolutionDLOpen(void);

#define udev_device_get_devnode \
    udevi->_device_get_devnode
#define udev_device_get_parent_with_subsystem_devtype \
    udevi->_device_get_parent_with_subsystem_devtype
#define udev_device_get_sysattr_value \
    udevi->_device_get_sysattr_value
#define udev_device_new_from_syspath \
    udevi->_device_new_from_syspath
#define udev_device_unref \
    udevi->_device_unref
#define udev_enumerate_add_match_property \
    udevi->_enumerate_add_match_property
#define udev_enumerate_add_match_subsystem \
    udevi->_enumerate_add_match_subsystem
#define udev_enumerate_get_list_entry \
    udevi->_enumerate_get_list_entry
#define udev_enumerate_new \
    udevi->_enumerate_new
#define udev_enumerate_scan_devices \
    udevi->_enumerate_scan_devices
#define udev_enumerate_unref \
    udevi->_enumerate_unref
#define udev_list_entry_get_name \
    udevi->_list_entry_get_name
#define udev_list_entry_get_next \
    udevi->_list_entry_get_next
#define udev_new \
    udevi->_new
#define udev_unref \
    udevi->_unref
#define udev_list_entry_foreach(_a, _b)\
    udevi_list_entry_foreach(udevi, _a, _b)

#define drmOpen \
    drmi->Open
#define drmClose \
    drmi->Close
#define drmGetVersion \
    drmi->GetVersion
#define drmFreeVersion \
    drmi->FreeVersion
#define drmDropMaster \
    drmi->DropMaster
#define drmCommandWrite \
    drmi->CommandWrite

#else /* HAVE_LIBUDEV */

#include <libudev.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <vmwgfx_drm.h>

/* Work around an incorrect define in old libdrm */
#undef DRM_VMW_UPDATE_LAYOUT
#define DRM_VMW_UPDATE_LAYOUT        20

static inline void resolutionDLClose(void) {}
static inline int resolutionDLOpen(void) {return 0;}

#endif /* HAVE_LIBUDEV */
#endif /* ENABLE_RESOLUTIONKMS */
#endif
