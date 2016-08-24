$NetBSD$

--- lib/include/vm_product.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/vm_product.h
@@ -514,6 +514,8 @@
 #  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for Mac OS X"
 #elif defined __ANDROID__
 #  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for Android"
+#elif defined(__NetBSD__)
+#  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for NetBSD"
 #else
 #  ifdef VMX86_TOOLS
 #    error "Define a product string for this platform."
