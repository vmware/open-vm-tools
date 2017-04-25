$NetBSD$

--- services/plugins/guestInfo/guestInfoServer.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ services/plugins/guestInfo/guestInfoServer.c
@@ -250,6 +250,50 @@ GuestInfoVMSupport(RpcInData *data)
 #endif
 }
 
+static char*
+GuestInfoGetKernelVersion()
+{
+    FILE * fp;
+    char * line = NULL;
+    char *find_start = NULL;
+    char *find_end = NULL;
+    char *version = NULL;
+    char *ver_str;
+    size_t len = 0;
+    size_t size_of_version = 0;
+    ssize_t read;
+
+    fp = fopen("/etc/brkt/images/config", "r");
+    if (fp == NULL)
+        return NULL;
+    while ((read = getline(&line, &len, fp)) != -1) {
+        find_start = strstr(line, "Linux");
+        if (find_start) {
+            find_end = strstr(line, "Kernel Configuration");
+            if (find_end) {
+                size_of_version = ((int) (find_end - find_start));
+                version = strndup(find_start, size_of_version);
+            }
+        }
+    }
+    fclose(fp);
+    if (line) {
+        free(line);
+    }
+    if (version) {
+        ver_str = malloc(sizeof(char)*(9 + strlen(version)));
+        bzero(ver_str, sizeof(char)*(9 + strlen(version)));
+        sprintf(ver_str, "Bracket %s", version);
+    } else {
+        ver_str = malloc(sizeof(char)*8);
+        bzero(ver_str, sizeof(char)*8);
+        sprintf(ver_str, "Bracket");
+    }
+    if (version)
+        free(version);
+    return ver_str;
+}
+
 
 /*
  ******************************************************************************
@@ -271,8 +315,8 @@ GuestInfoGather(gpointer data)
                     // "Host names are limited to 255 bytes"
    char *osString = NULL;
 #if !defined(USERWORLD)
-   gboolean disableQueryDiskInfo;
-   GuestDiskInfo *diskInfo = NULL;
+//   gboolean disableQueryDiskInfo;
+//   GuestDiskInfo *diskInfo = NULL;
 #endif
    NicInfoV3 *nicInfo = NULL;
    ToolsAppCtx *ctx = data;
@@ -290,7 +334,7 @@ GuestInfoGather(gpointer data)
    }
 
    /* Gather all the relevant guest information. */
-   osString = Hostinfo_GetOSName();
+   osString = GuestInfoGetKernelVersion();
    if (osString == NULL) {
       g_warning("Failed to get OS info.\n");
    } else {
@@ -311,6 +355,7 @@ GuestInfoGather(gpointer data)
    free(osString);
 
 #if !defined(USERWORLD)
+/*
    disableQueryDiskInfo =
       g_key_file_get_boolean(ctx->config, CONFGROUPNAME_GUESTINFO,
                              CONFNAME_GUESTINFO_DISABLEQUERYDISKINFO, NULL);
@@ -327,6 +372,7 @@ GuestInfoGather(gpointer data)
          }
       }
    }
+*/
 #endif
 
    if (!System_GetNodeName(sizeof name, name)) {
