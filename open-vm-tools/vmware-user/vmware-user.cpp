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

/**
 * @file vmware-user.cpp
 *
 * The linux vmware-user app. It's a hidden window app that is supposed
 * to run on session start. It handles tools features which we want
 * active all the time, but don't want to impose a visible window on the
 * user.
 */

/*
 *-----------------------------------------------------------------------------
 *
 * main --
 *
 *      This is main
 *
 * Results:
 *      Returns either EXIT_SUCCESS or EXIT_FAILURE appropriately.
 *
 * Side effects:
 *      The linux toolbox ui will run and do a variety of tricks for your
 *      amusement.
 *
 *-----------------------------------------------------------------------------
 */

int
main(int argc,         // IN
     char *argv[],     // IN
     char *envp[])     // IN
{
   return 0;
}
