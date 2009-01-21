/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * guestlibV3.x --
 *
 *    Data structures that encode the information sent from VMX to Guest upon a
 *    Guestlib protocol v3 request.
 */

struct GuestLibV3StatUint32 {
   Bool valid;
   uint32 value;
};

struct GuestLibV3StatUint64 {
   Bool valid;
   uint64 value;
};

struct GuestLibV3String {
   Bool valid;
   string value<512>;
};

typedef uint32 GuestLibV3StatCount;

enum GuestLibV3TypeIds {
   /* V2 statistics */
   GUESTLIB_TYPE_RESERVED         = 0,
   GUESTLIB_CPU_RESERVATION_MHZ   = 1,
   GUESTLIB_CPU_LIMIT_MHZ         = 2,
   GUESTLIB_CPU_SHARES            = 3,
   GUESTLIB_CPU_USED_MS           = 4,

   GUESTLIB_HOST_MHZ              = 5,

   GUESTLIB_MEM_RESERVATION_MB    = 6,
   GUESTLIB_MEM_LIMIT_MB          = 7,
   GUESTLIB_MEM_SHARES            = 8,
   GUESTLIB_MEM_MAPPED_MB         = 9,
   GUESTLIB_MEM_ACTIVE_MB         = 10,
   GUESTLIB_MEM_OVERHEAD_MB       = 11,
   GUESTLIB_MEM_BALLOONED_MB      = 12,
   GUESTLIB_MEM_SWAPPED_MB        = 13,
   GUESTLIB_MEM_SHARED_MB         = 14,
   GUESTLIB_MEM_SHARED_SAVED_MB   = 15,
   GUESTLIB_MEM_USED_MB           = 16,

   GUESTLIB_ELAPSED_MS            = 17,
   GUESTLIB_RESOURCE_POOL_PATH    = 18,

   /*------ Add any new statistics above this line. ------- */

   /*------ Bump this when adding to this list. -------*/
   GUESTLIB_MAX_STATISTIC_ID      = 19
};

union GuestLibV3Stat switch (GuestLibV3TypeIds d) {
   case GUESTLIB_CPU_RESERVATION_MHZ:
      struct GuestLibV3StatUint32 cpuReservationMHz;
   case GUESTLIB_CPU_LIMIT_MHZ:
      struct GuestLibV3StatUint32 cpuLimitMHz;
   case GUESTLIB_CPU_SHARES:
      struct GuestLibV3StatUint32 cpuShares;
   case GUESTLIB_CPU_USED_MS:
      struct GuestLibV3StatUint64 cpuUsedMs;

   case GUESTLIB_HOST_MHZ:
      struct GuestLibV3StatUint32 hostMHz;

   case GUESTLIB_MEM_RESERVATION_MB:
      struct GuestLibV3StatUint32 memReservationMB;
   case GUESTLIB_MEM_LIMIT_MB:
      struct GuestLibV3StatUint32 memLimitMB;
   case GUESTLIB_MEM_SHARES:
      struct GuestLibV3StatUint32 memShares;
   case GUESTLIB_MEM_MAPPED_MB:
      struct GuestLibV3StatUint32 memMappedMB;
   case GUESTLIB_MEM_ACTIVE_MB:
      struct GuestLibV3StatUint32 memActiveMB;
   case GUESTLIB_MEM_OVERHEAD_MB:
      struct GuestLibV3StatUint32 memOverheadMB;
   case GUESTLIB_MEM_BALLOONED_MB:
      struct GuestLibV3StatUint32 memBalloonedMB;
   case GUESTLIB_MEM_SWAPPED_MB:
      struct GuestLibV3StatUint32 memSwappedMB;
   case GUESTLIB_MEM_SHARED_MB:
      struct GuestLibV3StatUint32 memSharedMB;
   case GUESTLIB_MEM_SHARED_SAVED_MB:
      struct GuestLibV3StatUint32 memSharedSavedMB;
   case GUESTLIB_MEM_USED_MB:
      struct GuestLibV3StatUint32 memUsedMB;

   case GUESTLIB_ELAPSED_MS:
      struct GuestLibV3StatUint64 elapsedMs;

   case GUESTLIB_RESOURCE_POOL_PATH:
      struct GuestLibV3String resourcePoolPath;
};

