$NetBSD$

--- libvmtools/i18n.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ libvmtools/i18n.c
@@ -523,6 +523,7 @@ MsgLoadCatalog(const char *path)
    localPath = VMTOOLS_GET_FILENAME_LOCAL(path, NULL);
    ASSERT(localPath != NULL);
 
+   g_message("got to localpath %s\n", localPath);
    stream = g_io_channel_new_file(localPath, "r", &err);
    VMTOOLS_RELEASE_FILENAME_LOCAL(localPath);
 
@@ -532,6 +533,7 @@ MsgLoadCatalog(const char *path)
       return NULL;
    }
 
+   g_message("got to localpath %s\n", localPath);
    dict = HashTable_Alloc(8, HASH_STRING_KEY | HASH_FLAG_COPYKEY, g_free);
    ASSERT_MEM_ALLOC(dict);
 
@@ -626,6 +628,7 @@ MsgLoadCatalog(const char *path)
          break;
       }
 
+      g_message("got to localpath %s\n", localPath);
       if (name != NULL) {
          ASSERT(value);
 
@@ -745,6 +748,7 @@ VMTools_BindTextDomain(const char *domai
    file = g_strdup_printf("%s%smessages%s%s%s%s.vmsg",
                           catdir, DIRSEPS, DIRSEPS, lang, DIRSEPS, domain);
 
+   g_message("got file %s dfltdir %s catdir %s\n", file, dfltdir, catdir);
    if (!File_IsFile(file)) {
       /*
        * If we couldn't find the catalog file for the user's language, see if
@@ -762,6 +766,7 @@ VMTools_BindTextDomain(const char *domai
       }
    }
 
+   g_message("got file %s dfltdir %s catdir %s\n", file, dfltdir, catdir);
    catalog = MsgLoadCatalog(file);
 
    if (catalog == NULL) {
