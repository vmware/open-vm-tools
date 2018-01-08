/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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
 * msg.h  --
 * 
 *  user interaction through (non-modal) messages and (modal) dialogs
 */

#ifndef _MSG_H_
#define _MSG_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <string.h>
#include <stdarg.h>
#include "err.h"
#include "vm_basic_types.h"
#include "msgid.h"
#include "msgfmt.h"
#include "msgList.h"
#if defined VMX86_SERVER && !defined VMCORE
#include "voblib.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif


#define INVALID_MSG_CODE (-1)

/*
 * Data structures, types, and constants
 */

typedef struct Msg_String {
   const char *idFmt;
} Msg_String;
   
typedef enum MsgSeverity {
   MSG_INFO,
   MSG_INFO_TIMEOUT,
   MSG_WARNING,
   MSG_ERROR,
   MSG_CONFIG_EDITOR,
   MSG_WEB_LINK_GET_LICENSE_ERROR,
   MSG_WEB_LINK_EXTEND_LICENSE_ERROR,
   MSG_WEB_LINK_EXTEND_LICENSE_INFO,
   MSG_WEB_LINK_HOME_PAGE_INFO,
   MSG_NUM_SEVERITIES
} MsgSeverity;

typedef enum HintResult {
   HINT_CONTINUE,
   HINT_CANCEL,
   HINT_NOT_SHOWN
} HintResult;

typedef enum HintOptions {
   HINT_OK,
   HINT_OKCANCEL
} HintOptions;

typedef struct MsgCallback {
   void (*post)(MsgSeverity severity, const char *msgID, const char *message);
   int (*question)(char const * const *names, int defaultAnswer,
                   const char *msgID, const char *message);
   int (*progress)(const char *msgID, const char *message,
		   int percent, Bool cancelButton);
   HintResult (*hint)(HintOptions options,
		      const char *msgID, const char *message);

   void *(*lazyProgressStart)(const char *msgID,
                              const char *message,
                              Bool allowCancel);
   Bool  (*lazyProgress)(void *handle,
                         const char *msgID,
                         const char *message,
                         Bool allowCancel,
                         int percent);
   void  (*lazyProgressEnd)(void *handle);

   void (*postList)(MsgSeverity severity, MsgList *messages);
   int (*questionList)(const Msg_String *buttons, int defaultAnswer,
                       MsgList *messages);
   int (*progressList)(MsgList *messages, int percent, Bool cancelButton);
   HintResult (*hintList)(HintOptions options, MsgList *messages);
   void *(*lazyProgressStartList)(MsgList *messages);
   void (*forceUnblock)(void);
   void (*msgPostHook)(int severity, const MsgList *msgs);
} MsgCallback;

#define MSG_QUESTION_MAX_BUTTONS   10

#define MSG_PROGRESS_START (-1)
#define MSG_PROGRESS_STOP 101

VMX86_EXTERN_DATA Msg_String const Msg_YesNoButtons[];
VMX86_EXTERN_DATA Msg_String const Msg_OKButtons[];
VMX86_EXTERN_DATA Msg_String const Msg_RetryCancelButtons[];
VMX86_EXTERN_DATA Msg_String const Msg_OKCancelButtons[];
VMX86_EXTERN_DATA Msg_String const Msg_RetryAbortButtons[];

VMX86_EXTERN_DATA Msg_String const Msg_Severities[];


/*
 * Functions
 */

void Msg_Append(const char *idFmt,
                ...) PRINTF_DECL(1, 2);
void Msg_VAppend(const char *idFmt, va_list args);
void Msg_AppendStr(const char *id);
void Msg_AppendMsgList(const MsgList *msgs);

static INLINE void
Msg_AppendVobContext(void)
{
#if defined VMX86_SERVER && !defined VMCORE
   /* inline to avoid lib/vob dependency unless necessary */
   MsgList *msgs = NULL;

   VobLib_CurrentContextMsgAppend(&msgs);
   Msg_AppendMsgList(msgs);

   MsgList_Free(msgs);
#endif
}

void Msg_Post(MsgSeverity severity,
              const char *idFmt,
              ...) PRINTF_DECL(2, 3);
void Msg_PostMsgList(MsgSeverity severity,
                     const MsgList *msgs);

char *Msg_Format(const char *idFmt,
                 ...) PRINTF_DECL(1, 2);
char *Msg_VFormat(const char *idFmt,
                  va_list arguments);
unsigned Msg_Question(Msg_String const *buttons,
                      int defaultAnswer,
                      const char *idFmt, ...) PRINTF_DECL(3, 4);

/*
 * Unfortunately, gcc warns about both NULL and "" being passed as format
 * strings, and callers of Msg_Progress() like to do that.  So I'm removing
 * the PRINTF_DECL() for now.  --Jeremy.
 */
int Msg_Progress(int percentDone, Bool cancelButton, const char *idFmt,
                 ...);
int Msg_ProgressScaled(int percentDone, int opsDone, int opsTotal,
                       Bool cancelButton);

void *Msg_LazyProgressStart(Bool allowCancel,
                            const char *idFmt,
                            ...) PRINTF_DECL(2, 3);

Bool Msg_LazyProgress(void *handle,
                      Bool allowCancel,
                      int percent,
                      const char *idFmt,
                      ...) PRINTF_DECL(4, 5);

void Msg_LazyProgressEnd(void *handle);


HintResult Msg_Hint(Bool defaultShow,
                    HintOptions options,
                    const char *idFmt,
                    ...) PRINTF_DECL(3, 4);

HintResult Msg_HintMsgList(Bool defaultShow, HintOptions options,
                           MsgList *msg);
int Msg_CompareAnswer(Msg_String const *buttons, unsigned answer,
                      const char *string);
char *Msg_GetString(const char *idString);
char *Msg_GetStringSafe(const char *idString);
char *Msg_GetPlainButtonText(const char *idString);
char *Msg_GetLocale(void);
void Msg_SetLocale(const char *locale, const char *binaryName);
void Msg_SetLocaleEx(const char *locale, const char *binaryName,
                     const char *baseDirPath);
char *Msg_FormatFloat(double value, unsigned int precision);
char *Msg_FormatSizeInBytes(uint64 size);
Bool Msg_LoadMessageFile(const char *locale, const char *fileName);
void Msg_ForceUnblock(void);


/*
 * Message buffer management
 */

const char *Msg_GetMessages(void);
const char *Msg_GetMessagesAndReset(void);
void Msg_LogAndReset(void);
MsgList *Msg_GetMsgList(void);
MsgList *Msg_GetMsgListAndReset(void);
char *Msg_LocalizeList(const MsgList *messages);
void Msg_Reset(Bool log);
Bool Msg_Present(void);
void Msg_ExitThread(void);
void Msg_Exit(void);


/*
 * Don't know, don't care. -- edward
 */

#define MSG_POST_NOMEM() \
   Msg_Post(MSG_ERROR, MSGID(msg.noMem) "Cannot allocate memory.\n")

#ifndef VMX86_DEBUG
   #define MSG_CHECK_ORPHANED_MESSAGES(id, fmt, arg)
#elif defined VMX86_DEVEL
   #define MSG_CHECK_ORPHANED_MESSAGES(id, fmt, arg) ( \
      Msg_Present() ? \
	 Msg_Post(MSG_INFO, \
		  id fmt "THIS MESSAGE ON DEVEL BUILDS only - " \
		  "Please file bug about orphan Msg_Append\n", \
		  arg) : \
	 (void) 0 \
   )
#else
   #define MSG_CHECK_ORPHANED_MESSAGES(id, fmt, arg) ( \
      Msg_Present() ? \
	 (Log(fmt, arg), \
	  Msg_Reset(TRUE)) : \
	 (void) 0 \
   )
#endif


/*
 * To implement message dialogs
 */

void Msg_SetCallback(MsgCallback *cb);
void Msg_SetThreadCallback(MsgCallback *cb);

void Msg_GetCallback(MsgCallback *cb);
void Msg_GetThreadCallback(MsgCallback *cb);


/*
 * Conversion functions
 */

#define Msg_ErrString() (Err_ErrString())
#define Msg_Errno2String(errorNumber) ( \
    Err_Errno2String(errorNumber) )

#ifdef _WIN32
const char *Msg_HResult2String(long hr);
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // ifndef _MSG_H_
