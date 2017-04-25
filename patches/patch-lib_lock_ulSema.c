$NetBSD$

--- lib/lock/ulSema.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/lock/ulSema.c
@@ -283,7 +283,9 @@ MXUserTimedDown(NativeSemaphore *sema,  
    endTime.tv_nsec = (long int) (endNS % MXUSER_A_BILLION);
 
    do {
-      err = (sem_timedwait(sema, &endTime) == -1) ? errno : 0;
+      // This is wrong used sem_timedwait after patching the fix to avatar
+      //err = (sem_timedwait(sema, &endTime) == -1) ? errno : 0;
+      err = (sem_trywait(sema) == -1) ? errno : 0;
 
       if (err == 0) {
          *downOccurred = TRUE;
