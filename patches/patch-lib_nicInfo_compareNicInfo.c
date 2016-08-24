$NetBSD$

--- lib/nicInfo/compareNicInfo.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/nicInfo/compareNicInfo.c
@@ -96,12 +96,20 @@ GuestInfo_IsEqual_DnsConfigInfo(const Dn
 
    RETURN_EARLY_CMP_PTRS(a, b);
 
+   if (*a->domainName && *b->domainName) {
    if (!GuestInfo_IsEqual_DnsHostname(a->hostName, b->hostName) ||
        !GuestInfo_IsEqual_DnsHostname(a->domainName, b->domainName) ||
        a->serverList.serverList_len != b->serverList.serverList_len ||
        a->searchSuffixes.searchSuffixes_len != b->searchSuffixes.searchSuffixes_len) {
       return FALSE;
    }
+   } else {
+   if (!GuestInfo_IsEqual_DnsHostname(a->hostName, b->hostName) ||
+       a->serverList.serverList_len != b->serverList.serverList_len ||
+       a->searchSuffixes.searchSuffixes_len != b->searchSuffixes.searchSuffixes_len) {
+      return FALSE;
+   }
+   }
 
    /*
     * Since the lists' lengths match, search in b for each item in a.  We'll
