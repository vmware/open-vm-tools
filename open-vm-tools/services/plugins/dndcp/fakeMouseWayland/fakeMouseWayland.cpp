/*********************************************************
 * Copyright (C) 2018-2019 VMware, Inc. All rights reserved.
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
 * @file fakeMouseWayland.cpp --
 *
 *    Implement the methods that simulates the mouse motion.
 */

#define G_LOG_DOMAIN "dndcp"

#include "fakeMouseWayland.h"

#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/uinput.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define UINPUT_DND_POINTER_NAME "VMware DnD UInput pointer"


// The handle for the uinput device.
static int uinput_fd = -1;

// Indicates if the uinput device is created
static bool isInit = false;

/*
 *-----------------------------------------------------------------------------
 *
 * FakeMouse_IsInit --
 *
 *      Check if the uinput device is created.
 *
 * Results:
 *      True if the uinput device is created.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
FakeMouse_IsInit()
{
   return isInit;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FakeMouse_Init --
 *
 *      Initialize the uinput device.
 *
 * Results:
 *      True if the uinput device is created successfully.
 *
 * Side effects:
 *      uinput device is created if succeed.
 *
 *-----------------------------------------------------------------------------
 */

bool
FakeMouse_Init(int fd,       // fd for uinput
               int width,    // width of the screen
               int height)   // height of the screen
{
   if (FakeMouse_IsInit()) {
      return true;
   }

   g_debug("%s: Init the uinput device. fd:%d, w:%d, h:%d\n",
           __FUNCTION__, fd, width, height);

   uinput_fd = fd;
   if (uinput_fd == -1) {
      return false;
   }

   /*
    * The uinput old interface is used for compatibility.
    * For more information please refer to:
    * https://www.kernel.org/doc/html/v4.12/input/uinput.html
    */
   struct uinput_user_dev dev;
   memset(&dev, 0, sizeof(dev));
   snprintf(dev.name, UINPUT_MAX_NAME_SIZE, UINPUT_DND_POINTER_NAME);

   dev.absmin[ABS_X] = 0;
   dev.absmax[ABS_X] = width - 1;
   dev.absmin[ABS_Y] = 0;
   dev.absmax[ABS_Y] = height - 1;

   if (write(uinput_fd, &dev, sizeof(dev)) < 0) {
      g_debug("%s: Failed to write\n", __FUNCTION__);
      goto exit;
   }
   if (ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS) < 0) {
      g_debug("%s: Failed to register UI_SET_EVBIT EV_ABS\n", __FUNCTION__);
      goto exit;
   }
   if (ioctl(uinput_fd, UI_SET_ABSBIT, ABS_X) < 0) {
      g_debug("%s: Failed to register UI_SET_ABSBIT ABS_X\n", __FUNCTION__);
      goto exit;
   }
   if (ioctl(uinput_fd, UI_SET_ABSBIT, ABS_Y) < 0) {
      g_debug("%s: Failed to register UI_SET_ABSBIT ABS_Y\n", __FUNCTION__);
      goto exit;
   }
   if (ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY) < 0) {
      g_debug("%s: Failed to register UI_SET_EVBIT EV_KEY\n", __FUNCTION__);
      goto exit;
   }
   if (ioctl(uinput_fd, UI_SET_KEYBIT, BTN_MOUSE) < 0) {
      g_debug("%s: Failed to register UI_SET_KEYBIT BTN_MOUSE\n", __FUNCTION__);
      goto exit;
   }
   if (ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT) < 0) {
      g_debug("%s: Failed to register UI_SET_KEYBIT BTN_LEFT\n", __FUNCTION__);
      goto exit;
   }
   if (ioctl(uinput_fd, UI_DEV_CREATE, 0) < 0) {
      g_debug("%s: Failed to create UInput device\n", __FUNCTION__);
      goto exit;
   }

   /*
    * On UI_DEV_CREATE the kernel will create the device node for this
    * device. Insert a pause here so that userspace has time
    * to detect, initialize the new device, and can start listening to
    * the event, otherwise it will not notice the event we are about
    * to send.
    */
   usleep(100 * 1000);

   isInit = true;
   return true;

exit:
   FakeMouse_Destory();
   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FakeMouse_Update --
 *
 *      Update the width and height properties of  the uinput device.
 *
 * Results:
 *      True if the uinput device is updated successfully.
 *
 * Side effects:
 *      uinput device is updated.
 *
 *-----------------------------------------------------------------------------
 */

bool
FakeMouse_Update(int width,    // width of the screen
                 int height)   // height of the screen
{
   if (!FakeMouse_IsInit()) {
      return false;
   }

   FakeMouse_Destory();
   return FakeMouse_Init(uinput_fd, width, height);
}


/*
 *-----------------------------------------------------------------------------
 *
 * FakeMouse_Destory --
 *
 *      Destory the uinput device.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      uinput device is destory.
 *
 *-----------------------------------------------------------------------------
 */

void
FakeMouse_Destory()
{
   if (!FakeMouse_IsInit()) {
      return;
   }

   if (ioctl(uinput_fd, UI_DEV_DESTROY, 0) < 0) {
      g_debug("%s: Failed to destroy uinput device\n", __FUNCTION__);
   }
   isInit = false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FakeMouse_Move --
 *
 *      Move the pointer to (x, y).
 *
 * Results:
 *      True if success.
 *
 * Side effects:
 *      Pointer position is updated.
 *
 *-----------------------------------------------------------------------------
 */

bool
FakeMouse_Move(int x,    // IN
               int y)    // IN
{
   if (!FakeMouse_IsInit()) {
      return false;
   }

   bool retValue = true;
   struct input_event event;
   struct timeval tv;

   event.type = EV_ABS;
   event.code = ABS_X;
   event.value = x;
   gettimeofday(&tv, NULL);
   event.input_event_sec = tv.tv_sec;
   event.input_event_usec = tv.tv_usec;
   if (write(uinput_fd, &event, sizeof(event)) < 0) {
      g_debug("Line:%d. Function:%s. Failed to write\n", __LINE__, __FUNCTION__);
      retValue = false;
   }

   event.type = EV_ABS;
   event.code = ABS_Y;
   event.value = y;
   gettimeofday(&tv, NULL);
   event.input_event_sec = tv.tv_sec;
   event.input_event_usec = tv.tv_usec;
   if (write(uinput_fd, &event, sizeof(event)) < 0) {
      g_debug("Line:%d. Function:%s. Failed to write\n", __LINE__, __FUNCTION__);
      retValue = false;
   }

   event.type = EV_SYN;
   event.code = SYN_REPORT;
   event.value = 0;
   gettimeofday(&tv, NULL);
   event.input_event_sec = tv.tv_sec;
   event.input_event_usec = tv.tv_usec;
   if (write(uinput_fd, &event, sizeof(event)) < 0) {
      g_debug("Line:%d. Function:%s. Failed to write\n", __LINE__, __FUNCTION__);
      retValue = false;
   }

   return retValue;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FakeMouse_Click --
 *
 *      Simulate the pointer down/up event.
 *
 * Results:
 *      True if success.
 *
 * Side effects:
 *      Pointer button event is updated.
 *
 *-----------------------------------------------------------------------------
 */

bool
FakeMouse_Click(bool down)  // IN
{
   if (!FakeMouse_IsInit()) {
      return false;
   }

   bool retValue = true;
   struct input_event event;
   struct timeval tv;

   event.type = EV_KEY;
   event.code = BTN_LEFT;
   event.value = down;
   gettimeofday(&tv, NULL);
   event.input_event_sec = tv.tv_sec;
   event.input_event_usec = tv.tv_usec;
   if (write(uinput_fd, &event, sizeof(event)) < 0) {
      g_debug("Line:%d. Function:%s. Failed to write\n", __LINE__, __FUNCTION__);
      retValue = false;
   }

   event.type = EV_SYN;
   event.code = SYN_REPORT;
   event.value = 0;
   if (write(uinput_fd, &event, sizeof(event)) < 0) {
      g_debug("Line:%d. Function:%s. Failed to write\n", __LINE__, __FUNCTION__);
      retValue = false;
   }

   /*
    * Insert a pause here so that userspace has time to detect this event,
    * otherwise it will not notice the event we are about to send.
    */
   usleep(100 * 1000);
   return retValue;
}
