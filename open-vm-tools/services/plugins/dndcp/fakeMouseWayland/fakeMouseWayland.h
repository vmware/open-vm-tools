/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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
 * @file fakeMouseWayland.h
 *
 *    Implement the methods that simulates the mouse motion.
 *
 */

#ifndef __FAKE_MOUSE_WAYLAND_H__
#define __FAKE_MOUSE_WAYLAND_H__

bool FakeMouse_Init(int fd, int width, int height);
bool FakeMouse_IsInit();
bool FakeMouse_Update(int width, int height);
void FakeMouse_Destory();
bool FakeMouse_Move(int x, int y);
bool FakeMouse_Click(bool down);

#endif // __FAKE_MOUSE_WAYLAND_H__
