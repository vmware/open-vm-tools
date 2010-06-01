/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 * wiperPosix.c --
 *
 *      Linux/Solaris library for wiping a virtual disk.
 *
 */

#if !defined(__linux__) && !defined(sun) && !defined(__FreeBSD__) && !defined(__APPLE__)
#error This file should not be compiled on this platform.
#endif

#include <stdio.h>
#include <sys/stat.h>
#if defined(__linux__) || defined(sun)
# include <sys/vfs.h>
#elif defined(__FreeBSD__) || defined(__APPLE__)
# include <sys/param.h>
# include <sys/ucred.h>
# include <sys/mount.h>
# include <fstab.h>
# if defined(__FreeBSD_version) && __FreeBSD_version >= 500000
#  include <libgen.h>
# endif /* __FreeBSD_version >= 500000 */
#endif
#include <unistd.h>

#include "vmware.h"
#include "wiper.h"
#include "util.h"
#include "str.h"
#include "fileIO.h"
#include "mntinfo.h"
#include "posix.h"
#include "util.h"


/* Number of bytes per disk sector */
#define WIPER_SECTOR_SIZE 512

/* Number of disk sectors to write per write system call.

   The bigger it is, the less calls we do and the faster we are.

   This value has been empirically determined to give maximum performance.

--hpreg
*/
#define WIPER_SECTOR_STEP 128

#if defined(sun) || defined(__linux__)
# define PROCFS "proc"
#elif defined(__FreeBSD__) || defined(__APPLE__)
# define PROCFS "procfs"
#endif


/* Types */
typedef enum {
   WIPER_PHASE_CREATE,
   WIPER_PHASE_FILL,
} WiperPhase;

typedef struct File {
   unsigned char name[NATIVE_MAX_PATH];
   FileIODescriptor fd;
   uint64 size;
   struct File *next;
} File;

/* Internal definition of the wiper state */
typedef struct WiperState {
   /* State machine */
   WiperPhase phase;
   /* WiperPartition to wipe */
   const WiperPartition *p;
   /* File we are currently wiping */
   File *f;
   /* Serial number of the next wiper file to create */
   unsigned int nr;
   /*  Buffer to write in each sector of a wiper file */
   unsigned char buf[WIPER_SECTOR_STEP * WIPER_SECTOR_SIZE];
   /* Effective user id */
   uid_t euid;
} WiperState;

#ifdef sun
typedef struct WiperDiskString {
   char *name;
   size_t nameLen;
} WiperDiskString;
#endif


/* Variables */
static Bool initDone = FALSE;


/* Local functions */
static INLINE Bool WiperIsDiskDevice(MNTINFO *mnt, struct stat *s);
static void WiperPartitionFilter(WiperPartition *item, MNTINFO *mnt);
static unsigned char *WiperGetSpace(WiperState *state, uint64 *free, uint64 *total);
static void WiperClean(WiperState *state);

/*
 *-----------------------------------------------------------------------------
 *
 * WiperIsDiskDevice --
 *
 *      Determines whether a device is a disk device.
 *
 * Results:
 *      TRUE if disk device, otherwise FALSE.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(sun) /* SunOS { */
static INLINE Bool
WiperIsDiskDevice(MNTINFO *mnt,         // IN: file system being considered
                  struct stat *s)       // IN: stat(2) info of fs source
{
   char resolvedPath[PATH_MAX];

   ASSERT(mnt);
   ASSERT(s);

   /*
    * resolvepath() will provide the actual path in /devices of this mount
    * point's device.  The final path component is the node name and will start
    * with "cmdk@" for IDE disks and "sd@" for SCSI disks.
    */
#  define SOL_DEVICE_ROOT  "/devices/"
#  define SOL_SCSI_STR     "sd@"
#  define SOL_IDE_STR      "cmdk@"
   if (resolvepath(MNTINFO_NAME(mnt), resolvedPath, sizeof resolvedPath) != -1) {
      if (strncmp(resolvedPath, SOL_DEVICE_ROOT, sizeof SOL_DEVICE_ROOT - 1) == 0) {
         uint32 i;
         char *name;
         WiperDiskString diskDevices[] = {
            { SOL_SCSI_STR, sizeof SOL_SCSI_STR - 1 },
            { SOL_IDE_STR,  sizeof SOL_IDE_STR - 1  },
         };

         name = basename(resolvedPath);

         for (i = 0; i < ARRAYSIZE(diskDevices); i++) {
            if (strncmp(name, diskDevices[i].name, diskDevices[i].nameLen) == 0) {
               return TRUE;
            }
         }
      }
   }

   return FALSE;
}

#elif defined(__linux__) /* } linux { */

static INLINE Bool
WiperIsDiskDevice(MNTINFO *mnt,         // IN: file system being considered
                  struct stat *s)       // IN: stat(2) info of fs source
{
   int majorN;

   majorN = major(s->st_rdev);
   if (! (   (majorN == 3)  /* First MFM, RLL and IDE hard disk/CD-ROM
                              interface. Inside a VM, this is simply the First
                              IDE hard disk/CD-ROM interface because we don't
                              support others */
             || (majorN == 22) /* Second IDE hard disk/CD-ROM interface */
             || (majorN == 8)  /* SCSI disk devices */
             || (majorN == 43) /* Network block device */
             || (majorN == 259) /* Disks in 2.6.27 */)) {
      return FALSE;
   }

   return TRUE;
}

#elif defined(__FreeBSD__) || defined(__APPLE__) /* } FreeBSD { */

static INLINE Bool
WiperIsDiskDevice(MNTINFO *mnt,         // IN: file system being considered
                  struct stat *s)       // IN: stat(2) info of fs source
{
   Bool retval = FALSE;

   /*
    * The following code doesn't really apply to Apple's Mac OS X.  (E.g.,
    * OS X uses a different device and naming scheme.)  However, this
    * function, as a whole, does not even apply to OS X, so this caveat is
    * only minor.
    */
#if __FreeBSD_version < 500000
   /*
    * Before FreeBSD 5, device nodes had static major/minor numbers.
    * (FreeBSD 5 included devfs which got rid of this concept.)  So
    * we'll stat(2) the mount source and test its major numbers
    * against the values lifted from
    *   http://www.freebsd.org/cgi/cvsweb.cgi/src/sys/i386/conf/Attic/majors.i386 (rev 1.65)
    *
    * Devnode Type      Description                             Major
    * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    * block             "Winchester" (IDE, ATA, /dev/wd*)       0
    * character         "                                       3
    * block             SCSI "direct access" (/dev/da*)         4
    * character         "                                       13
    */
   int maj = major(s->st_rdev);
   if ((S_ISBLK(s->st_mode) && ((maj == 0) || (maj == 4))) ||
       (S_ISCHR(s->st_mode) && ((maj == 3) || (maj == 13)))) {
      retval = TRUE;
   }
#else /* Also the Apple case */
   /*
    * Begin by testing whether file system source is really a character
    * device node.  (FreeBSD killed off block devices long ago.)  Next,
    * simply discriminate by device node name:
    *   /dev/ad* = ATA disk, /dev/da* = SCSI disk
    */
#define MASK_ATA_DISK    "ad"
#define MASK_SCSI_DISK   "da"
   if (S_ISCHR(s->st_mode)) {
      char *name = basename(MNTINFO_NAME(mnt));
      if ((strncmp(name, MASK_ATA_DISK, sizeof MASK_ATA_DISK - 1) == 0) ||
          (strncmp(name, MASK_SCSI_DISK, sizeof MASK_SCSI_DISK - 1) == 0)) {
         retval = TRUE;
      }
   }
#endif /* __FreeBSD_version */

   return retval;
}
#endif /* } */

/*
 *-----------------------------------------------------------------------------
 *
 * WiperPartitionFilter --
 *
 *      Determine whether or not we know how to wipe a partition.
 *
 * Results:
 *      None
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
WiperPartitionFilter(WiperPartition *item,         // IN/OUT
                     MNTINFO *mnt)                 // IN
{
   struct stat s;
   const char *comment = NULL;

   item->type = PARTITION_UNSUPPORTED;

   /*
    * Let's ignore remote filesystems before we do a stat(2) on the actual
    * mountpoint. This should prevent a deadlock in guestd for guests that
    * still use an HGFS pserver (Solaris).
    */
   if (strcmp(MNTINFO_FSTYPE(mnt), "autofs") == 0) {
      /* XXX Should we look at autofs' config files? --hpreg */
      comment = "Not implemented. Contact VMware";

   } else if (strcmp(MNTINFO_FSTYPE(mnt), "vmhgfs") == 0) {
      comment = "Remote partition";

   } if (strcmp(MNTINFO_FSTYPE(mnt), "nfs") == 0) {
      comment = "Remote filesystem";

   } if (strcmp(MNTINFO_FSTYPE(mnt), "smbfs") == 0) {
      comment = "Remote filesystem";

   } if (strcmp(MNTINFO_FSTYPE(mnt), "swap") == 0) {
      comment = "Swap partition";

   } if (strcmp(MNTINFO_FSTYPE(mnt), PROCFS) == 0) {
      comment = "Proc partition";

   } if (strcmp(MNTINFO_FSTYPE(mnt), "devpts") == 0) {
      comment = "Devpts partition";

   } if (Posix_Stat(MNTINFO_NAME(mnt), &s) < 0) {
      comment = "Unknown device";

#if defined(sun) || defined(__linux__)
   } else if (! S_ISBLK(s.st_mode)) {
      comment = "Not a block device";
#endif

   } else if (!WiperIsDiskDevice(mnt, &s)) {
      comment = "Not a disk device";

   } if (MNTINFO_MNT_IS_RO(mnt)) {
      comment = "Not writable";

   } if (strcmp(MNTINFO_FSTYPE(mnt), "ext2") == 0) {
      item->type = PARTITION_EXT2;

   } if (strcmp(MNTINFO_FSTYPE(mnt), "ext3") == 0) {
      item->type = PARTITION_EXT3;

   } if (strcmp(MNTINFO_FSTYPE(mnt), "reiserfs") == 0) {
      item->type = PARTITION_REISERFS;

   } if (strcmp(MNTINFO_FSTYPE(mnt), "ntfs") == 0) {
      item->type = PARTITION_NTFS;

   } if (strcmp(MNTINFO_FSTYPE(mnt), "vfat") == 0) {
      item->type = PARTITION_FAT;

   } if (strcmp(MNTINFO_FSTYPE(mnt), "ufs") == 0) {
      item->type = PARTITION_UFS;

   } if (strcmp(MNTINFO_FSTYPE(mnt), "pcfs") == 0) {
      item->type = PARTITION_PCFS;

   } else {
      comment = "Unknown filesystem. Contact VMware";
   }

   if (item->type == PARTITION_UNSUPPORTED) {
      ASSERT(comment);
      item->comment = Util_SafeStrdup(comment);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * WiperSinglePartition_Open --
 *
 *      Return information about the input 'mountPoint' partition.
 *
 * Results:
 *      WiperPartition * on success.
 *      NULL on failure.
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

WiperPartition *
WiperSinglePartition_Open(const char *mountPoint)      // IN
{
   char *mntpt = NULL;
   MNTHANDLE fp;
   int len = 0;
   DECLARE_MNTINFO(mnt);
   WiperPartition *p = NULL;

   ASSERT(initDone);

   fp = OPEN_MNTFILE("r");
   if (fp == NULL) {
      Log("Could not open %s\n", MNTFILE);
      return NULL;
   }

   mntpt = Util_SafeStrdup(mountPoint);
   /*
    * Remove any trailing DIRSEPC from mntpt
    * for correct comparison with /etc/mtab strings.
    */
   {
      char *tmp = &mntpt[strlen(mntpt) - 1];
      if (*tmp == DIRSEPC) {
         *tmp = '\0';
      }
   }

   len = strlen(mntpt);
   while (GETNEXT_MNTINFO(fp, mnt)) {
      if (strncmp(MNTINFO_MNTPT(mnt), mntpt, len) == 0) {

         p = WiperSinglePartition_Allocate();
         if (p == NULL) {
            Log("Not enough memory while opening a partition.\n");
         } else if (Str_Snprintf(p->mountPoint, NATIVE_MAX_PATH,
                                 "%s", MNTINFO_MNTPT(mnt)) == -1) {
            Log("NATIVE_MAX_PATH is too small.\n");
            WiperSinglePartition_Close(p);
            p = NULL;
         } else {
            WiperPartitionFilter(p, mnt);
         }

         goto out;
      }
   }

   Log("Could not find a mount point for %s in %s\n", mntpt, MNTFILE);

 out:
   free(mntpt);
   (void) CLOSE_MNTFILE(fp);
   return p;
}

/*
 *-----------------------------------------------------------------------------
 *
 * WiperSinglePartition_GetSpace --
 *
 *      Get the free space left and the total space (in bytes) on a partition.
 *
 * Results:
 *      "" on success.
 *      The description of the error on failure.
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

unsigned char *
WiperSinglePartition_GetSpace(const WiperPartition *p,  // IN
                              uint64 *free,       // OUT
                              uint64 *total)      // OUT
{
#ifdef sun
   struct statvfs statfsbuf;
#else
   struct statfs statfsbuf;
#endif
   ASSERT(p);

#ifdef sun
   if (statvfs(p->mountPoint, &statfsbuf) < 0) {
#else
   if (Posix_Statfs(p->mountPoint, &statfsbuf) < 0) {
#endif
      return "Unable to statfs() the mount point";
   }

   if (geteuid()== 0) {
      *free = (uint64)statfsbuf.f_bfree * statfsbuf.f_bsize;
   } else {
      *free = (uint64)statfsbuf.f_bavail * statfsbuf.f_bsize;
   }
   *total = (uint64)statfsbuf.f_blocks * statfsbuf.f_bsize;

   return "";
}

/*
 *-----------------------------------------------------------------------------
 *
 * WiperPartition_Open --
 *
 *      Return information about wipable and non-wipable partitions
 *
 * Results:
 *      The partition list on success
 *      NULL on failure
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
WiperPartition_Open(WiperPartition_List *pl)
{
   MNTHANDLE fp;
   DECLARE_MNTINFO(mnt);
   Bool rc = TRUE;

   ASSERT(initDone);

   DblLnkLst_Init(&pl->link);

   /* Basically call functions to parse /etc/mtab ... */
   fp = OPEN_MNTFILE("r");
   if (fp == NULL) {
      Log("Unable to open mount file.\n");
      return FALSE;
   }

   while (GETNEXT_MNTINFO(fp, mnt)) {
      WiperPartition *part = WiperSinglePartition_Allocate();

      if (part == NULL) {
         Log("Not enough memory while opening a partition.\n");
         rc = FALSE;
         break;
      }

      if (Str_Snprintf(part->mountPoint, NATIVE_MAX_PATH, "%s",
                       MNTINFO_MNTPT(mnt)) == -1) {
         Log("NATIVE_MAX_PATH is too small.\n");
         WiperSinglePartition_Close(part);
         rc = FALSE;
         break;
      }

      WiperPartitionFilter(part, mnt);
      DblLnkLst_LinkLast(&pl->link, &part->link);
   }

   if (!rc)
      WiperPartition_Close(pl);

   (void) CLOSE_MNTFILE(fp);
   return rc;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Wiper_Start --
 *
 *      Allocate and initialize the wiper state
 *
 * Results:
 *      A Wiper_State on success
 *      NULL on failure
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Wiper_State *
Wiper_Start(const WiperPartition *p,             // IN
            unsigned int maxWiperFileSize)       // IN : unused
{
   WiperState *state;

   state = (WiperState *)malloc(sizeof *state);
   if (state == NULL) {
      return NULL;
   }

   /* Initialize the state */
   state->phase = WIPER_PHASE_CREATE;
   state->p = p;
   state->f = NULL;
   state->nr = 0;
   memset(state->buf, 0, WIPER_SECTOR_STEP * WIPER_SECTOR_SIZE);
   state->euid = geteuid();

   return (void *)state;
}


/*
 *-----------------------------------------------------------------------------
 *
 * WiperGetSpace --
 *
 *      Get the free space left and the total space (in bytes) on a partition.
 *
 * Results:
 *      "" on success.
 *      The description of the error on failure.
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static unsigned char *
WiperGetSpace(WiperState *state,  // IN
              uint64 *free,       // OUT
              uint64 *total)      // OUT
{
   ASSERT(state);
   return WiperSinglePartition_GetSpace(state->p, free, total);
}


/*
 *-----------------------------------------------------------------------------
 *
 * WiperClean --
 *
 *      Remove all created files.
 *
 * Results:
 *      None
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
WiperClean(WiperState *state)      // IN/OUT
{
   ASSERT(state);

   while (state->f != NULL) {
      File *next;

      FileIO_Close(&state->f->fd);
      next = state->f->next;
      free(state->f);
      state->f = next;
   }

   free(state);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Wiper_Next --
 *
 *      Do the next piece of work to wipe
 *
 * Note: Try to make sure that the execution of this function does not take
 *       more than 1/5 second, so that the user still has some feeling of
 *       interactivity
 *
 * Results:
 *      "" on success. 'progress' is the updated progress indicator (between 0
 *                   and 100 included. 100 means that the job is done, the
 *                   wiper state is destroyed)
 *      The description of the error on failure
 *
 * Side Effects:
 *      The wiper state is updated
 *
 *-----------------------------------------------------------------------------
 */

unsigned char *
Wiper_Next(Wiper_State **s,         // IN/OUT
           unsigned int *progress)  // OUT
{
   WiperState **state;
   uint64 free;
   uint64 total;
   unsigned char *error;

   ASSERT(s);
   ASSERT(*s);
   state = (WiperState **)s;

   error = WiperGetSpace(*state, &free, &total);
   if (*error != '\0') {
      WiperClean(*state);
      *state = NULL;
      return error;
   }

   /* Disk space is an important system resource. Don't fill the partition
      completely */
   if (free <= (((uint64)5) << 20) /* 5 MB */) {
      /* We are done */
      WiperClean(*state);
      *state = NULL;
      *progress = 100;
      return "";
   }

   /* We are not done */
   switch ((*state)->phase) {
   case WIPER_PHASE_CREATE:
      {
         File *new;

         new = (File *)malloc(sizeof *new);
         if (new == NULL) {
            WiperClean(*state);
            *state = NULL;
            return "Not enough memory";
         }

         /*
          * Create a new file
          */

         /* We name it just under the mount point so that we are sure that
            the file is on the right partition. */
         for (;;) {
            FileIOResult fret;
            FileIO_Invalidate(&new->fd);

            if (Str_Snprintf(new->name, NATIVE_MAX_PATH, "%s/wiper%d",
                             (*state)->p->mountPoint, (*state)->nr++) == -1) {
               Log("NATIVE_MAX_PATH is too small\n");
               ASSERT(0);
            }

            fret = FileIO_Open(&new->fd,
                               new->name,
                               FILEIO_OPEN_ACCESS_WRITE
                               | FILEIO_OPEN_DELETE_ASAP,
                               FILEIO_OPEN_CREATE_SAFE);
            if (fret == FILEIO_SUCCESS) {
               break;
            }

            if (fret != FILEIO_OPEN_ERROR_EXIST) {
               WiperClean(*state);
               *state = NULL;
               return "error.create";
            }
         }
         new->size = 0;

         new->next = (*state)->f;
         (*state)->f = new;
      }
      (*state)->phase = WIPER_PHASE_FILL;
      break;

   case WIPER_PHASE_FILL:
      {
         unsigned int i;

         /* Do several write system calls per call to Wiper_Next() */
         for (i = 0; i < (2 << 20) /* 2 MB */
                         / (WIPER_SECTOR_STEP * WIPER_SECTOR_SIZE); i++) {
            FileIOResult fret;

            if ((*state)->f->size + WIPER_SECTOR_STEP * WIPER_SECTOR_SIZE >=
                (((uint64)2) << 30) /* 2 GB */) {
               /* The file is going to be larger than what most filesystems
                  can support. Create a new file */
               (*state)->phase = WIPER_PHASE_CREATE;
               break;
            }

            fret = FileIO_Write(&(*state)->f->fd, (*state)->buf,
                                WIPER_SECTOR_STEP * WIPER_SECTOR_SIZE, NULL);

            /*
             * We distiguish errors from FilieIO_Write.
             */
            if (fret != FILEIO_SUCCESS) {

               /* The file is too big even though its size is less than 2GB */
               if (fret == FILEIO_WRITE_ERROR_FBIG) {
                  (*state)->phase = WIPER_PHASE_CREATE;

                  break;
               }

               /*
                * The disk is full (there may be other process is consuming space),
                * or the user runs out of his disk quota.
                */
               if (fret == FILEIO_WRITE_ERROR_NOSPC) {
                  WiperClean(*state);
                  *state = NULL;
                  *progress = 100;
                  return "";
               }

               /* Otherwise, it is a real error */
               WiperClean(*state);
               *state = NULL;
               return fret==FILEIO_WRITE_ERROR_DQUOT ? "User's disk quota exceeded" :
                                                       "Unable to write to a wiper file";
            }

            (*state)->f->size += WIPER_SECTOR_STEP * WIPER_SECTOR_SIZE;
         }
      }
      break;

   default:
      Log("state is %u\n", (*state)->phase);
      ASSERT(0);
      break;
   }

   *progress = 99 - 99 * free / total;
   return "";
}


/*
 *-----------------------------------------------------------------------------
 *
 * Wiper_Cancel --
 *
 *      Cancel the wipe operation and destroy the associated wiper state
 *
 * Results:
 *      "" on success
 *      The description of the error on failure
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

unsigned char *
Wiper_Cancel(Wiper_State **s)      // IN/OUT
{
   if (s && *s) {
      WiperClean((WiperState *)*s);
      *s = NULL;
   }
   return "";
}


/*
 *-----------------------------------------------------------------------------
 *
 * Wiper_Init --
 *
 *      On Linux and Solaris, this function is defined only to provide a
 *      uniform interface to the library.
 *
 * Results:
 *      Always TRUE.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Wiper_Init(WiperInitData *clientData)
{
   return initDone = TRUE;
}
