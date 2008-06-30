/* **********************************************************
 * Copyright 2008 VMware, Inc.  All rights reserved. -- VMware Confidential
 * **********************************************************/

/*
 * nicinfo.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    provide information about the guest NICs.
 */

/*
 * Enumerates the different versions of the messages. Starting at 2, since
 * version one is legacy code we can't change.
 */
enum NicInfoVersion {
    NIC_INFO_V2 = 2
};

/*
 * These are arbitrary limits to avoid possible DoS attacks.
 * The IP limit is large enough to hold an IP address (either v4 or v6).
 */
const NICINFO_MAX_IP_LEN   = 64;
const NICINFO_MAX_IPS      = 64;
const NICINFO_MAX_NICS     = 16;

/* MAC Addresses are "AA:BB:CC:DD:EE:FF" = 18 bytes. */
const NICINFO_MAC_LEN      = 18;

struct VmIpAddress {
   uint32      addressFamily;
   Bool        dhcpEnabled;
   char        ipAddress[NICINFO_MAX_IP_LEN];
   char        subnetMask[NICINFO_MAX_IP_LEN];
};

struct GuestNic {
   char                 macAddress[NICINFO_MAC_LEN];
   struct VmIpAddress   ips<NICINFO_MAX_IPS>;
};

/*
 * This structure is not entirely necessary, but it makes the generated
 * code nicer to code to.
 */
struct GuestNicList {
   struct GuestNic nics<NICINFO_MAX_NICS>;
};

/*
 * This defines the protocol for a "nic info" message. The union allows
 * us to create new versions of the protocol later by creating new values
 * in the NicInfoVersion enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail
 * if an unknown version is provided on the wire.
 */
union GuestNicProto switch (NicInfoVersion ver) {
case NIC_INFO_V2:
   struct GuestNicList *nicsV2;
};

