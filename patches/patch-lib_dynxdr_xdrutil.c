$NetBSD$

--- lib/dynxdr/xdrutil.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/dynxdr/xdrutil.c
@@ -102,7 +102,8 @@ XdrUtil_Deserialize(const void *data,  /
    ASSERT(dest != NULL);
 
    xdrmem_create(&xdrs, (char *) data, dataLen, XDR_DECODE);
-   ret = (Bool) proc(&xdrs, dest, 0);
+   ret = (Bool) proc(&xdrs, dest);
+//, 0);
    xdr_destroy(&xdrs);
 
    if (!ret) {
