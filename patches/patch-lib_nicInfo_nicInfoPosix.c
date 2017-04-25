$NetBSD$

--- lib/nicInfo/nicInfoPosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/nicInfo/nicInfoPosix.c
@@ -34,7 +34,7 @@
 #include <sys/socket.h>
 #include <sys/stat.h>
 #include <errno.h>
-#if defined(__FreeBSD__) || defined(__APPLE__)
+#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
 # include <sys/sysctl.h>
 # include <ifaddrs.h>
 # include <net/if.h>
@@ -62,6 +62,13 @@
 #   include <net/if.h>
 #endif
 
+#if defined(__NetBSD__)
+#include <resolv.h>
+//#include <res_update.h>
+#endif
+
+static struct __res_state res;
+
 /*
  * resolver(3) and IPv6:
  *
@@ -137,7 +144,7 @@ static int ReadInterfaceDetails(const st
 static Bool RecordResolverInfo(NicInfoV3 *nicInfo);
 static void RecordResolverNS(DnsConfigInfo *dnsConfigInfo);
 static Bool RecordRoutingInfo(NicInfoV3 *nicInfo);
-#if !defined(__FreeBSD__) && !defined(__APPLE__) && !defined(USERWORLD)
+#if !defined (__NetBSD__) && !defined(__FreeBSD__) && !defined(__APPLE__) && !defined(USERWORLD)
 static int GuestInfoGetIntf(const struct intf_entry *entry, void *arg);
 #endif
 #endif
@@ -280,7 +287,7 @@ GuestInfoGetNicInfo(NicInfoV3 *nicInfo) 
  *
  ******************************************************************************
  */
-#if defined(__FreeBSD__) || defined(__APPLE__) || defined(USERWORLD)
+#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__APPLE__) || defined(USERWORLD)
 
 char *
 GuestInfoGetPrimaryIP(void)
@@ -471,7 +478,13 @@ RecordResolverInfo(NicInfoV3 *nicInfo)  
    char namebuf[DNSINFO_MAX_ADDRLEN + 1];
    char **s;
 
-   if (res_init() == -1) {
+//   if (res_init() == -1) {
+//      return FALSE;
+//   }
+
+   if ((res.options & RES_INIT) == 0 && res_ninit(&res) < 0) {
+      g_message("%s: Cannot fetch the resolv information",
+                __FUNCTION__);
       return FALSE;
    }
 
@@ -492,7 +505,7 @@ RecordResolverInfo(NicInfoV3 *nicInfo)  
     */
    dnsConfigInfo->domainName =
       Util_SafeCalloc(1, sizeof *dnsConfigInfo->domainName);
-   *dnsConfigInfo->domainName = Util_SafeStrdup(_res.defdname);
+   *dnsConfigInfo->domainName = Util_SafeStrdup(res.defdname);
 
    /*
     * Name servers.
@@ -502,7 +515,7 @@ RecordResolverInfo(NicInfoV3 *nicInfo)  
    /*
     * Search suffixes.
     */
-   for (s = _res.dnsrch; *s; s++) {
+   for (s = res.dnsrch; *s; s++) {
       DnsHostname *suffix;
 
       /* Check to see if we're going above our limit. See bug 605821. */
@@ -521,7 +534,6 @@ RecordResolverInfo(NicInfoV3 *nicInfo)  
     * "Commit" dnsConfigInfo to nicInfo.
     */
    nicInfo->dnsConfigInfo = dnsConfigInfo;
-
    return TRUE;
 
 fail:
@@ -545,17 +557,23 @@ fail:
 static void
 RecordResolverNS(DnsConfigInfo *dnsConfigInfo) // IN
 {
-   int i;
+  int i;
+
+  if ((res.options & RES_INIT) == 0 && res_ninit(&res) < 0) {
+      g_message("%s: Cannot fetch the resolv information",
+                __FUNCTION__);
+      return;
+   }
 
 #if defined RESOLVER_IPV6_GETSERVERS
    {
       union res_sockaddr_union *ns;
-      ns = Util_SafeCalloc(_res.nscount, sizeof *ns);
-      if (res_getservers(&_res, ns, _res.nscount) != _res.nscount) {
+      ns = Util_SafeCalloc(res.nscount, sizeof *ns);
+      if (res_getservers(&res, ns, res.nscount) != res.nscount) {
          g_warning("%s: res_getservers failed.\n", __func__);
          return;
       }
-      for (i = 0; i < _res.nscount; i++) {
+      for (i = 0; i < res.nscount; i++) {
          struct sockaddr *sa = (struct sockaddr *)&ns[i];
          if (sa->sa_family == AF_INET || sa->sa_family == AF_INET6) {
             TypedIpAddress *ip;
@@ -579,7 +597,7 @@ RecordResolverNS(DnsConfigInfo *dnsConfi
        * Name servers (IPv4).
        */
       for (i = 0; i < MAXNS; i++) {
-         struct sockaddr_in *sin = &_res.nsaddr_list[i];
+         struct sockaddr_in *sin = &res.nsaddr_list[i];
          if (sin->sin_family == AF_INET) {
             TypedIpAddress *ip;
 
@@ -600,7 +618,7 @@ RecordResolverNS(DnsConfigInfo *dnsConfi
        * Name servers (IPv6).
        */
       for (i = 0; i < MAXNS; i++) {
-         struct sockaddr_in6 *sin6 = _res._u._ext.nsaddrs[i];
+         struct sockaddr_in6 *sin6 = res._u._ext.nsaddrs[i];
          if (sin6) {
             TypedIpAddress *ip;
 
@@ -621,7 +639,6 @@ RecordResolverNS(DnsConfigInfo *dnsConfi
 #endif                                  // if !defined RESOLVER_IPV6_GETSERVERS
 }
 
-
 #ifdef USE_SLASH_PROC
 /*
  ******************************************************************************
@@ -847,7 +864,7 @@ RecordRoutingInfo(NicInfoV3 *nicInfo)
 }
 #endif                                          // else
 
-#if !defined(__FreeBSD__) && !defined(__APPLE__) && !defined(USERWORLD)
+#if !defined(__NetBSD__) && !defined(__FreeBSD__) && !defined(__APPLE__) && !defined(USERWORLD)
 /*
  ******************************************************************************
  * GuestInfoGetIntf --                                                   */ /**
