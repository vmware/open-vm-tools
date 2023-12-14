/*********************************************************
 * Copyright (c) 2007-2021 VMware, Inc. All rights reserved.
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
 * processPosix.c --
 *
 *      Implementation of the POSIX process wrapper.
 */

#include "imgcust-common/log.h"
#include "imgcust-common/process.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>

#include "util.h"

typedef struct _ProcessInternal {
   pid_t pid;
   int stdoutFd;
   int stderrFd;
   char* stdoutStr;
   char* stderrStr;
   int exitCode;
   char** args;
   LogFunction log;
} ProcessInternal;

typedef enum _ReadStatus {
   READSTATUS_UNDEFINED,
   READSTATUS_DONE,
   READSTATUS_PENDING,
   READSTATUS_WAITING_EOF,
   READSTATUS_ERROR
} ReadStatus;

static void
ProcessRead(ProcessInternal *p, ReadStatus *status, Bool out, Bool readToEof);


/*
 *------------------------------------------------------------------------------
 *
 * Process_Create --
 *
 *      Create and initialize a process object.
 *
 *------------------------------------------------------------------------------
 */

ProcessError
Process_Create(ProcessHandle *h, char *args[], void *logPtr)
{
   int i, numArgs;
   int err = -1;
   ProcessInternal *p;
   LogFunction log = (LogFunction)logPtr;
   log(log_info, "sizeof ProcessInternal is %d", sizeof(ProcessInternal));
   p = (ProcessInternal*) calloc(1, sizeof(ProcessInternal));
   if (p == NULL) {
      log(log_error, "Error allocating memory for process");
      goto error;
   }
   p->stdoutStr = malloc(sizeof(char));
   if (p->stdoutStr == NULL) {
      log(log_error, "Error allocating memory for process stdout");
      goto error;
   }
   p->stdoutStr[0] = '\0';
   p->stderrStr = malloc(sizeof(char));
   if (p->stderrStr == NULL) {
      log(log_error, "Error allocating memory for process stderr");
      goto error;
   }
   p->stderrStr[0] = '\0';

   p->stdoutFd = -1;
   p->stderrFd = -1;

   numArgs = 0;
   while (args[numArgs] != NULL) {
      numArgs++;
   }

   p->args = malloc((1 + numArgs) * sizeof(char*));
   if (p->args == NULL) {
      log(log_error, "Error allocating memory for process args");
      goto error;
   }
   for (i = 0; i < numArgs; i++) {
      p->args[i] = strdup(args[i]);
      if (p->args[i] == NULL) {
         log(log_error, "Error allocating memory for duplicate args");
         goto error;
      }
   }
   p->args[numArgs] = NULL;
   p->log = log;
   *h = (ProcessHandle)p;
   err = 0;

error:
   if (err != 0) {
      if (p != NULL) {
         Process_Destroy((ProcessHandle)p);
      }
      exit(1);
   }
   return PROCESS_SUCCESS;
}


/*
 *------------------------------------------------------------------------------
 *
 * Process_RunToComplete --
 *
 *      Runs a process until complete, collecting stdout and stderr into the
 *      process object.
 *
 *------------------------------------------------------------------------------
 */

ProcessError
Process_RunToComplete(ProcessHandle h, unsigned long timeoutSec)
{
   int stdout[2];
   int stderr[2];
   int flags;
   ProcessInternal* p;
   // poll for the process to complete and read the output
   const unsigned int OneSecMicroSec = 1000000;
   const unsigned int LoopSleepMicrosec = OneSecMicroSec / 10;
   const unsigned long timeoutLoopSleeps =
      timeoutSec * (OneSecMicroSec / LoopSleepMicrosec);
   unsigned long elapsedTimeLoopSleeps;

   ReadStatus res_stdout = READSTATUS_UNDEFINED;
   ReadStatus res_stderr = READSTATUS_UNDEFINED;

   Bool processExitedAbnormally = FALSE;

   p = (ProcessInternal*)h;

   stdout[0] = stdout[1] = 0;
   if (pipe(stdout) < 0) {
      p->log(log_error, "Failed to create pipe for stdout:%s", strerror(errno));
      return PROCESS_FAILED;
   }

   stderr[0] = stderr[1] = 0;
   if (pipe(stderr) < 0) {
      p->log(log_error, "Failed to create pipe for stderr,%s", strerror(errno));
      close(stdout[0]);
      close(stdout[1]);
      return PROCESS_FAILED;
   }

   p->pid = fork();
   if (p->pid == -1) {
      p->log(log_error, "Failed to fork: %s", strerror(errno));
      close(stdout[0]);
      close(stdout[1]);
      close(stderr[0]);
      close(stderr[1]);
      return PROCESS_FAILED;
   } else if (p->pid == 0) {
      // we're in the child. close the read ends of the pipes and exec
      close(stdout[0]);
      close(stderr[0]);
      dup2(stdout[1], STDOUT_FILENO);
      dup2(stderr[1], STDERR_FILENO);
      execv(p->args[0], p->args);
      p->log(log_error, "execv failed to run (%s), errno=(%d), "
             "error message:(%s)", p->args[0], errno, strerror(errno));

      // exec failed
      close(stdout[1]);
      close(stderr[1]);
      exit(127);
   }

   // close write ends of pipes and make reads nonblocking
   close(stdout[1]);
   close(stderr[1]);

   p->stdoutFd = stdout[0];
   flags = fcntl(p->stdoutFd, F_GETFL);
   if (fcntl(p->stdoutFd, F_SETFL, flags | O_NONBLOCK) == -1) {
      p->log(log_warning, "Failed to set stdoutFd status flags, (%s)",
             strerror(errno));
   }

   p->stderrFd = stderr[0];
   flags = fcntl(p->stderrFd, F_GETFL);
   if (fcntl(p->stderrFd, F_SETFL, flags | O_NONBLOCK) == -1) {
      p->log(log_warning, "Failed to set stderrFd status flags, (%s)",
             strerror(errno));
   }

   elapsedTimeLoopSleeps = 0;

   while (1) {
      int processStatus;
      int processFinished = (waitpid(p->pid, &processStatus, WNOHANG) > 0);

      if (processFinished) {
         if (WIFEXITED(processStatus)) {
            p->exitCode = WEXITSTATUS(processStatus);
            p->log(log_info,
                   "Process exited normally after %d seconds, returned %d",
                   elapsedTimeLoopSleeps * LoopSleepMicrosec / OneSecMicroSec,
                   p->exitCode);
         } else if (WIFSIGNALED(processStatus)) {
            p->exitCode = 127;
            p->log(log_error,
                   "Process exited abnormally after %d sec, uncaught signal %d",
                   elapsedTimeLoopSleeps * LoopSleepMicrosec / OneSecMicroSec,
                   WTERMSIG(processStatus));
            processExitedAbnormally = TRUE;
         }

         break;
      } else {
         if (timeoutLoopSleeps == elapsedTimeLoopSleeps) {
            p->log(log_error, "Timed out waiting for process exit, canceling...");
            kill(p->pid, SIGKILL);
         }

         // Empty the pipes.
         ProcessRead(p, &res_stdout, TRUE, FALSE);
         if (res_stdout == READSTATUS_ERROR) {
            p->log(log_error, "Error while reading process output, canceling...");
            kill(p->pid, SIGKILL);
         }

         ProcessRead(p, &res_stderr, FALSE, FALSE);
         if (res_stderr == READSTATUS_ERROR) {
            p->log(log_error, "Error while reading process output, canceling...");
            kill(p->pid, SIGKILL);
         }

         usleep(LoopSleepMicrosec);
         elapsedTimeLoopSleeps++;
      }
   }

   // Process completed. Now read all the output to EOF.
   // PR 2367614, set readToEof to TRUE only if process exits normally.
   // Otherwise just empty the pipe to avoid being blocked by read operation.
   ProcessRead(p, &res_stdout, TRUE, !processExitedAbnormally);
   if (res_stdout == READSTATUS_ERROR) {
      p->log(log_error, "Error while reading process stdout, canceling...");
   }

   ProcessRead(p, &res_stderr, FALSE, !processExitedAbnormally);
   if (res_stderr == READSTATUS_ERROR) {
      p->log(log_error, "Error while reading process stderr, canceling...");
   }

   close(stdout[0]);
   close(stderr[0]);
   return PROCESS_SUCCESS;
}


/*
 *------------------------------------------------------------------------------
 *
 * ProcessRead --
 *
 *      Read redirected stdout or stderr.
 *
 * status - as IN  - holds the result from previous read operation.
 *          as OUT - returns the status from the read operation.
 *
 * There are two modes:
 * readToEof = TRUE - block until the read returns 0 (EOF). This signifies
 *                    that the process has exited and there's nothing left.
 * readToEof = FALSE- Just empty the pipe. This will return even if we get
 *             EAGAIN back from the read. Use this in the midst of the poll
 *             loop so the pipe doesn't fill up and block the process.
 *
 *------------------------------------------------------------------------------
 */

static void
ProcessRead(ProcessInternal *p, ReadStatus *status, Bool stdout, Bool readToEof)
{
   char buf[1024];
   size_t currSize, newSize;
   char** saveTo;
   int fd;
   char* stdstr = stdout ? "stdout" : "stderr";

   // which fd do we read and which pointer do we save to?
   fd = stdout ? p->stdoutFd : p->stderrFd;
   saveTo = stdout ? &p->stdoutStr : &p->stderrStr;

   // if there's output waiting, read it out. FDs should already be non-blocking
   do {
      ssize_t count = read(fd, buf, sizeof buf);

      if (count > 0) {
         // save output
         currSize = strlen(*saveTo);
         newSize = count + currSize;
         *saveTo = Util_SafeRealloc(*saveTo, newSize + 1);
         memcpy(*saveTo + currSize, buf, count);
         (*saveTo)[newSize] = '\0';
         p->log(log_info, "Saving output from %s", stdstr);
      } else if (count == 0) {
         if (*status != READSTATUS_DONE) {
            // we're done
            p->log(log_info, "No more output from %s", stdstr);
            *status = READSTATUS_DONE;
         }

         return;
      } else if (count < 0) {
         if (errno == EAGAIN && readToEof) {
            if (*status != READSTATUS_WAITING_EOF) {
               // waiting for more output, sleep briefly and try again
               p->log(log_info, "Pending output from %s till EOF, trying again",
                  stdstr);
               *status = READSTATUS_WAITING_EOF;
            }

            usleep(1000);
         } else if (errno == EAGAIN && !readToEof) {
            if (*status != READSTATUS_PENDING) {
               // caller doesn't want to wait until EOF
               p->log(log_info, "Returning, pending output from %s", stdstr);
               *status = READSTATUS_PENDING;
            }

            return;
         } else {
            // error
            p->log(log_error, "Failed to read from %s: %s",
                   stdstr, strerror(errno));

            *status = READSTATUS_ERROR;
            return;
         }
      }
   } while (1);
}


/*
 *------------------------------------------------------------------------------
 *
 * Process_GetStdout --
 *
 *      Returns process's standard output.
 *
 *------------------------------------------------------------------------------
 */

const char *
Process_GetStdout(ProcessHandle h)
{
   ProcessInternal *p = (ProcessInternal *)h;
   return p->stdoutStr;
}


/*
 *------------------------------------------------------------------------------
 *
 * Process_GetStderr --
 *
 *      Returns process's standard error output.
 *
 *------------------------------------------------------------------------------
 */

const char *
Process_GetStderr(ProcessHandle h)
{
   ProcessInternal *p = (ProcessInternal *)h;
   return p->stderrStr;
}


/*
 *------------------------------------------------------------------------------
 *
 * Process_GetExitCode --
 *
 *      Returns process's exit code.
 *
 *------------------------------------------------------------------------------
 */

int
Process_GetExitCode(ProcessHandle h)
{
   ProcessInternal *p = (ProcessInternal *)h;
   return p->exitCode;
}


/*
 *------------------------------------------------------------------------------
 *
 * Process_Destroy --
 *
 *      Destroys the process and returns result of the operation.
 *
 *------------------------------------------------------------------------------
 */

ProcessError
Process_Destroy(ProcessHandle h)
{
   ProcessInternal* p;
   p = (ProcessInternal*)h;
   if (p->stdoutFd >= 0) {
      close(p->stdoutFd);
   }
   if (p->stderrFd >= 0) {
      close(p->stderrFd);
   }
   free(p->stdoutStr);
   free(p->stderrStr);
   if (p->args != NULL) {
      int i;
      for (i = 0; p->args[i] != NULL; i++) {
         free(p->args[i]);
      }
      free(p->args);
   }
   free(p);
   return PROCESS_SUCCESS;
}
