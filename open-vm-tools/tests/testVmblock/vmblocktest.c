/*********************************************************
 * Copyright (C) 2006-2017,2019 VMware, Inc. All rights reserved.
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
 * vmblocktest.c --
 *
 *   Test program for the vmblock file system.  Ensures correctness and
 *   stability.
 *
 */

#if !defined(__linux__) && !defined(sun) && !defined(__FreeBSD__) && !defined(vmblock_fuse)
# error "vmblocktest.c needs to be ported to your OS."
#endif

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#ifdef sun
# include <stropts.h>
#endif

#include "vmblock_user.h"
#include "vm_basic_types.h"

#define CONTROLFILE               VMBLOCK_DEVICE
#define CONTROLFILE_MODE          VMBLOCK_DEVICE_MODE
#define REALROOT                   "/tmp/VMwareDnD/"
#define FILENAME                   "/foo"
#define ACCESSORFULLNAME(dir)      VMBLOCK_FS_ROOT "/" dir FILENAME
#define BLOCKERFULLNAME(dir)       REALROOT dir

#define lprintf(...)                                                    \
   do {                                                                 \
      pthread_mutex_lock(&print_lock);                                  \
      printf(__VA_ARGS__);                                              \
      fflush(stdout);                                                   \
      pthread_mutex_unlock(&print_lock);                                \
   } while(0)

#define lfprintf(stream, ...)                                           \
   do {                                                                 \
      pthread_mutex_lock(&print_lock);                                  \
      fprintf(stream, __VA_ARGS__);                                     \
      fflush(stream);                                                   \
      pthread_mutex_unlock(&print_lock);                                \
   } while(0)

#define Log(fmt, args...)          lprintf(fmt, ## args)
#define ERROR(fmt, args...)        lfprintf(stderr, fmt, ## args)
#define THREAD_LOG(fmt, args...)   lprintf(" (%lx) " fmt, (unsigned long)pthread_self(), ## args)
#define THREAD_ERROR(fmt, args...) lfprintf(stderr, " (%"FMTPID") " fmt, getpid(), ## args)

#if defined (linux) || defined(__FreeBSD__)
# define os_thread_yield()      sched_yield()
#elif defined(sun)
# define os_thread_yield()      yield()
#endif

/*
 * This program may optionally throttle accessor thread generation
 * via POSIX semaphores.
 */
#if defined(USE_SEMAPHORES)
sem_t sem;
# define SEM_THREADS    (unsigned int)10
# define SEM_INIT()     sem_init(&sem, 0, SEM_THREADS)
# define SEM_DESTROY()  sem_destroy(&sem)
# define SEM_WAIT()     sem_wait(&sem)
# define SEM_POST()     sem_post(&sem)
# define PTHREAD_SEMAPHORE_CLEANUP()    pthread_cleanup_push(sem_post,  \
                                                             (void *)&sem)
#else
# define SEM_INIT()
# define SEM_DESTROY()
# define SEM_WAIT()
# define SEM_POST()
# define PTHREAD_SEMAPHORE_CLEANUP()
#endif

/* Types */
typedef struct FileState {
   /* Accessors will access the file through the vmblock namespace */
   char *accessorName;
   /* The blocker will add blocks using the real file's name */
   char *blockerName;
   Bool blocked;
   unsigned int waiters;
} FileState;

typedef struct ThreadInfo {
   int blockFd;
   pthread_mutex_t *lock;
   FileState *files;
   size_t filesArraySize;
   unsigned int sleepTime;
} ThreadInfo;

/* Variables */
static Bool programQuit = FALSE;
static FileState files[] = {
   { ACCESSORFULLNAME("0"), BLOCKERFULLNAME("0"), FALSE, 0 },
   { ACCESSORFULLNAME("1"), BLOCKERFULLNAME("1"), FALSE, 0 },
   { ACCESSORFULLNAME("2"), BLOCKERFULLNAME("2"), FALSE, 0 },
   { ACCESSORFULLNAME("3"), BLOCKERFULLNAME("3"), FALSE, 0 },
   { ACCESSORFULLNAME("4"), BLOCKERFULLNAME("4"), FALSE, 0 },
   { ACCESSORFULLNAME("5"), BLOCKERFULLNAME("5"), FALSE, 0 },
   { ACCESSORFULLNAME("6"), BLOCKERFULLNAME("6"), FALSE, 0 },
   { ACCESSORFULLNAME("7"), BLOCKERFULLNAME("7"), FALSE, 0 },
   { ACCESSORFULLNAME("8"), BLOCKERFULLNAME("8"), FALSE, 0 },
   { ACCESSORFULLNAME("9"), BLOCKERFULLNAME("9"), FALSE, 0 },
};

/* Thread entry points */
static void *blocker(void *arg);
static void *accessor(void *arg);

/* Utility functions */
static Bool addBlock(int fd, char const *filename);
static Bool delBlock(int fd, char const *filename);
static Bool listBlocks(int fd);
#ifdef VMBLOCK_PURGE_FILEBLOCKS
static Bool purgeBlocks(int fd);
#endif
static unsigned int getRand(unsigned int max);
static void sighandler(int sig);

pthread_mutex_t print_lock;

/*
 *----------------------------------------------------------------------------
 *
 * main --
 *
 *    Does all necessary setup then starts the blocker thread and continually
 *    starts accessor threads.
 *
 * Results:
 *    EXIT_SUCCESS and EXIT_FAILURE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
main(int argc,
     char *argv[])
{
   int ret = EXIT_SUCCESS;
   int i;
   int blockFd;
   char *progname;
   pthread_t blockerThread;
   pthread_mutex_t filesLock;
   pthread_attr_t attr;
   ThreadInfo info;
   int count;

   progname = basename(argv[0]);
   pthread_mutex_init(&filesLock, NULL);

   SEM_INIT();
   pthread_mutex_init(&print_lock, NULL);

   /* Open device user to add/delete blocks */
   blockFd = open(CONTROLFILE, CONTROLFILE_MODE);
   if (blockFd < 0) {
      ERROR("%s: could not open " CONTROLFILE "\n", progname);
      exit(EXIT_FAILURE);
   }

   /* Provide ability to just list blocks */
   if (argc > 1) {
      if (strcmp(argv[1], "-list") == 0) {
         listBlocks(blockFd);
#ifdef VMBLOCK_PURGE_FILEBLOCKS
      } else if (strcmp(argv[1], "-purge") == 0) {
         purgeBlocks(blockFd);
#endif
      }
      close(blockFd);
      exit(EXIT_SUCCESS);
   }

   /* Create directories/files used during test */
   for (i = 0; i < sizeof files/sizeof files[0]; i++) {
      int err;
      struct stat statbuf;
      char buf[PATH_MAX];

      err = stat(files[i].blockerName, &statbuf);
      if (!err) {
         if (S_ISDIR(statbuf.st_mode)) {
            goto create_file;
         }

         ERROR("%s: file [%s] already exists and is not a directory\n",
               progname, files[i].blockerName);
         goto exit_failure;
      }

      err = mkdir(files[i].blockerName, S_IRWXU | S_IRWXG);
      if (err) {
         ERROR("%s: could not create [%s]\n", progname, files[i].blockerName);
         goto exit_failure;
      }

create_file:
      strncpy(buf, files[i].blockerName, sizeof buf - 1);
      buf[sizeof buf - 1] = '\0';
      strncat(buf, FILENAME, sizeof buf - strlen(files[i].blockerName));
      err = stat(buf, &statbuf);
      if (!err) {
         if (S_ISREG(statbuf.st_mode)) {
            continue;
         }

         ERROR("%s: file [%s] already exists and is not a regular file\n",
               progname, buf);
         goto exit_failure;
      }

      err = creat(buf, S_IRUSR | S_IRGRP);
      if (err < 0) {
         ERROR("%s: could not create [%s]\n", progname, buf);
         goto exit_failure;
      }

      continue;

exit_failure:
      close(blockFd);
      exit(EXIT_FAILURE);
   }

   if (signal(SIGINT, sighandler) == SIG_ERR ||
       signal(SIGTERM, sighandler) == SIG_ERR) {
      ERROR("%s: could not install signal handlers\n", progname);
      close(blockFd);
      exit(EXIT_FAILURE);
   }

   /* Seems cleaner than a bunch of globals ... */
   info.blockFd = blockFd;
   info.lock = &filesLock;
   info.files = files;
   info.filesArraySize = sizeof files/sizeof files[0];
   info.sleepTime = 1;

   /*
    * Start a thread that flips a random file's state, then sleeps for a while
    * and does it again.
    */
   if (pthread_create(&blockerThread, NULL, blocker, &info)) {
      ERROR("%s: could not create blocker thread\n", progname);
      close(blockFd);
      exit(EXIT_FAILURE);
   }

   /*
    * Start a bunch of threads that try to open a random file, check its status
    * once they have it open (to make sure it is not blocked), then close it
    * and exit.
    */
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   count = 0;
   while (!programQuit) {
      pthread_t thread;
      int rv;
      SEM_WAIT();
      /* Create threads until we cannot anymore */
      if ((rv = pthread_create(&thread, &attr, accessor, &info)) != 0) {
         SEM_POST();
         if (rv == EAGAIN || rv == ENOMEM) {
            os_thread_yield();
            continue;
         }
         ERROR("%s: could not create an accessor thread (%d total)\n",
               progname, count);
         ERROR("%s: pthread_create: %s\n", progname, strerror(rv));
         ret = EXIT_FAILURE;
         break;
      }
      count++;
   }

   Log("%s: Not creating any more accessor threads.\n", progname);

   programQuit = TRUE;
   pthread_join(blockerThread, NULL);

   pthread_mutex_destroy(&filesLock);
   close(blockFd);

   Log("%s: Exiting with %s.\n",
       progname, ret == EXIT_SUCCESS ? "success" : "failure");

   exit(ret);
}


/*
 *----------------------------------------------------------------------------
 *
 * blocker --
 *
 *    Entry point for the single blocker thread.  Continuously picks a file at
 *    random and changes its state by adding or deleting a block on that file.
 *
 * Results:
 *    EXIT_SUCCESS and EXIT_FAILURE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void *
blocker(void *arg)  // IN
{
   ThreadInfo *info = (ThreadInfo *)arg;
   unsigned int index;

   if (!info) {
      THREAD_ERROR("blocker: no thread info provided\n");
      pthread_exit((void *)EXIT_FAILURE);
   }

   while (!programQuit) {
      index = getRand(info->filesArraySize - 1);

      pthread_mutex_lock(info->lock);

      if (info->files[index].blocked) {
         info->files[index].blocked = FALSE;
         if ( !delBlock(info->blockFd, info->files[index].blockerName) ) {
            THREAD_ERROR("blocker: could not delete block on [%s]\n",
                         info->files[index].blockerName);
            goto error;
         }
      } else if (info->files[index].waiters == 0) {
         /*
          * Only add a new block if all previous waiters are done.  This
          * ensures we don't get incorrect error messages from accessor threads
          * even though the open(2) and check are not atomic in the accessor.
          */
         info->files[index].blocked = TRUE;
         if ( !addBlock(info->blockFd, info->files[index].blockerName) ) {
            THREAD_ERROR("blocker: could not add block on [%s]\n",
                         info->files[index].blockerName);
            goto error;
         }
      }

      pthread_mutex_unlock(info->lock);

      sleep(info->sleepTime);
   }

   pthread_mutex_lock(info->lock);
   for (index = 0; index < info->filesArraySize; index++) {
      if (info->files[index].blocked) {
         info->files[index].blocked = FALSE;
#ifndef TEST_CLOSE_FD
         THREAD_LOG("blocker: deleting block for [%s]\n",
                    info->files[index].blockerName);
         if ( !delBlock(info->blockFd, info->files[index].blockerName) ) {
            THREAD_ERROR("blocker: could not delete existing block on exit for [%s]\n",
                         info->files[index].blockerName);
         }
#else
         THREAD_LOG("blocker: unmarking block for [%s], left for unblock on release\n",
                    info->files[index].blockerName);
#endif
      }
   }
   pthread_mutex_unlock(info->lock);

   pthread_exit(EXIT_SUCCESS);

error:
   programQuit = TRUE;
   pthread_mutex_unlock(info->lock);
   pthread_exit((void *)EXIT_FAILURE);
}


/*
 *----------------------------------------------------------------------------
 *
 * accessor --
 *
 *    Entry point for accessor threads.  Picks a file at random and attempts to
 *    open it.  Once it is opened, ensures the state of the file is not blocked
 *    and outputs an error message accordingly.
 *
 * Results:
 *    EXIT_SUCCESS and EXIT_FAILURE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void *
accessor(void *arg)  // IN
{
   ThreadInfo *info = (ThreadInfo *)arg;
   unsigned int index;
   int fd;
   uintptr_t ret = EXIT_SUCCESS;

   PTHREAD_SEMAPHORE_CLEANUP();

   if (!info) {
      THREAD_ERROR("blocker: no thread info provided\n");
      pthread_exit((void *)EXIT_FAILURE);
   }

   index = getRand(info->filesArraySize - 1);

   /*
    * We can't hold the lock while calling open(2), since we will deadlock
    * waiting for the blocker thread to remove the block on the file.  So, we
    * bump the waiter count to ensure that a new block is not placed on this
    * file until we have checked its state.  This prevents incorrect error
    * messages that would happen if the blocker placed a new block on the file
    * between our open(2) call returning and acquiring the lock.
    *
    * The fact that we can't hold the lock through open(2) also means that it's
    * not atomic with respect to checking the file's blocked flag.  Given this,
    * it's possible that we'll miss some errors if the block is removed after
    * open(2) returns but before we check the blocked flag -- hopefully running
    * this test for a very long time will be sufficient to catch these cases.
    * (Having the blocker sleep increases the likelihood of seeing such
    * errors.)
    */
   pthread_mutex_lock(info->lock);
   info->files[index].waiters++;
   pthread_mutex_unlock(info->lock);

   fd = open(info->files[index].accessorName, O_RDONLY);
   if (fd < 0) {
      if (errno == EMFILE) {
         /* We already have hit the maximum number of file descriptors */
         pthread_mutex_lock(info->lock);
         info->files[index].waiters--;
         pthread_mutex_unlock(info->lock);
         pthread_exit((void *)EXIT_FAILURE);
      }
      THREAD_ERROR("accessor: could not open file [%s]\n",
                   info->files[index].accessorName);
      THREAD_ERROR("accessor: open: %s\n", strerror(errno));
      pthread_exit((void *)EXIT_FAILURE);
   }

   pthread_mutex_lock(info->lock);
   info->files[index].waiters--;

   if (info->files[index].blocked) {
      THREAD_ERROR("accessor: [ERROR] accessed file [%s] while blocked (%d)\n",
                   info->files[index].accessorName, info->files[index].blocked);
      ret = EXIT_FAILURE;
   }
   pthread_mutex_unlock(info->lock);
   close(fd);

   pthread_exit((void *)ret);
}


/*
 *----------------------------------------------------------------------------
 *
 * addBlock --
 *
 *    Adds a block on the specified filename.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    Future open(2)s on the file will block until delBlock() is called for
 *    that file.
 *
 *----------------------------------------------------------------------------
 */

static Bool
addBlock(int fd,                // IN: fd of control device
         char const *filename)  // IN: filename to add block for
{
   Log("Blocking [%s]\n", filename);
   return VMBLOCK_CONTROL(fd, VMBLOCK_ADD_FILEBLOCK, filename) == 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * delBlock --
 *
 *    Deletes a block on the specified filename.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    Previous open(2)s that had blocked will now complete.
 *
 *----------------------------------------------------------------------------
 */

static Bool
delBlock(int fd,                // IN: fd of control device
         char const *filename)  // IN: filename to delete block for
{
   Log("Unblocking [%s]\n", filename);
   return VMBLOCK_CONTROL(fd, VMBLOCK_DEL_FILEBLOCK, filename) == 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * listBlocks --
 *
 *    Tells the kernel module to list all existing blocks.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
listBlocks(int fd)      // IN: fd of control device
{
   Log("Listing blocks (check kernel log output)\n");
   return VMBLOCK_CONTROL(fd, VMBLOCK_LIST_FILEBLOCKS, NULL) == 0;
}


#ifdef VMBLOCK_PURGE_FILEBLOCKS
/*
 *----------------------------------------------------------------------------
 *
 * purgeBlocks --
 *
 *    Tells the kernel module to purge all existing blocks, regardless of
 *    who opened them.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
purgeBlocks(int fd)      // IN: fd of control device
{
   Log("Purging all blocks\n");
   return VMBLOCK_CONTROL(fd, VMBLOCK_PURGE_FILEBLOCKS, NULL) == 0;
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * getRand --
 *
 *    Retrieves the next random number within the range [0, max].
 *
 * Results:
 *    A random number.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static unsigned int
getRand(unsigned int max)   // IN: max value returnable
{
   return random() % max;
}


/*
 *----------------------------------------------------------------------------
 *
 * sighandler --
 *
 *    Sets the programQuit flag when a signal is received.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Program will quit.
 *
 *----------------------------------------------------------------------------
 */

static void
sighandler(int sig)
{
   programQuit = TRUE;
}
