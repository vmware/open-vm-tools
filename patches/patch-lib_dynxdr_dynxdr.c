$NetBSD$

--- lib/dynxdr/dynxdr.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/dynxdr/dynxdr.c
@@ -134,7 +134,8 @@ DynXdrPutBytes(XDR *xdrs,               
  */
 
 static u_int
-DynXdrGetPos(DYNXDR_GETPOS_CONST XDR *xdrs) // IN
+DynXdrGetPos(XDR *xdrs) // IN
+//DYNXDR_GETPOS_CONST XDR *xdrs) // IN
 {
    DynXdrData *priv = (DynXdrData *) xdrs->x_private;
    return (u_int) DynBuf_GetSize(&priv->data);
