/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * imgcust-api.h --
 *
 *      C interface to package deployment.
 */

#ifndef IMGCUST_API_H
#define IMGCUST_API_H

#ifdef WIN32
/*
 * We get warnings c4251 and c4275 when exporting c++ classes that
 * inherit from STL classes or use them as members. We can't
 * export these classes or the client will get duplicate symbols,
 * so we disable the warning.
 */
#pragma warning( disable : 4251 )
#pragma warning( disable : 4275 )

// if _IMGCUST_DLL is defined, we export functions/classes with this prefix
#ifdef _IMGCUST_DLL
   #define IMGCUST_API __declspec(dllexport)
#else
   #define IMGCUST_API __declspec(dllimport)
#endif
#else // linux
#define IMGCUST_API __attribute__ ((visibility ("default")))
#endif // WIN32

#endif // IMGCUST_API_H
