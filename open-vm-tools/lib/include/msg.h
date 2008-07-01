/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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

typedef struct Msg_List Msg_List;
struct Msg_List {
   Msg_List *next;
   char *id;
   char *format;
   MsgFmt_Arg *args;
   int numArgs;
};

typedef struct MsgCallback {
   void (*post)(MsgSeverity severity, const char *msgID, const char *message);
   int (*question)(char const * const *names, int defaultAnswer,
                   const char *msgID, const char *message);
   int (*progress)(const char *msgID, const char *message,
		   int percent, Bool cancelButton);
   HintResult (*hint)(HintOptions options,
		      const char *msgID, const char *message);

   void *(*lazyProgressStart)(const char *msgID, const char *message);
   void  (*lazyProgress)(void *handle, int percent);
   void  (*lazyProgressEnd)(void *handle);

   void (*postList)(MsgSeverity severity, Msg_List *messages);
   int (*questionList)(const Msg_String *buttons, int defaultAnswer,
                       Msg_List *messages);
   int (*progressList)(Msg_List *messages, int percent, Bool cancelButton);
   HintResult (*hintList)(HintOptions options, Msg_List *messages);
   void *(*lazyProgressStartList)(Msg_List *messages);
} MsgCallback;

#define MSG_QUESTION_MAX_BUTTONS   10

#define MSG_PROGRESS_START (-1)
#define MSG_PROGRESS_STOP 101

EXTERN Msg_String const Msg_YesNoButtons[];
EXTERN Msg_String const Msg_OKButtons[];
EXTERN Msg_String const Msg_RetryCancelButtons[];
EXTERN Msg_String const Msg_OKCancelButtons[];
EXTERN Msg_String const Msg_RetryAbortButtons[];

EXTERN Msg_String const Msg_Severities[];


/*
 * Functions
 */

EXTERN void Msg_Append(const char *idFmt, ...)
       PRINTF_DECL(1, 2);
EXTERN void Msg_Post(MsgSeverity severity, const char *idFmt, ...)
       PRINTF_DECL(2, 3);
EXTERN void Msg_PostMsgList(MsgSeverity severity, Msg_List *msg);

EXTERN char *Msg_Format(const char *idFmt, ...)
       PRINTF_DECL(1, 2);
EXTERN char *Msg_VFormat(const char *idFmt, va_list arguments);
EXTERN unsigned Msg_Question(Msg_String const *buttons,
                             int defaultAnswer, const char *idFmt, ...)
       PRINTF_DECL(3, 4);
EXTERN void Msg_AppendMsgList(char* id,
                              char* fmt,
                              MsgFmt_Arg *args,
                              int numArgs);
EXTERN Msg_List *Msg_VCreateMsgList(const char *idFmt, va_list args);

/*
 * Unfortunately, gcc warns about both NULL and "" being passed as format
 * strings, and callers of Msg_Progress() like to do that.  So I'm removing
 * the PRINTF_DECL() for now.  --Jeremy.
 */
EXTERN int Msg_Progress(int percentDone, Bool cancelButton, const char *idFmt,
                        ...);
EXTERN int Msg_ProgressScaled(int percentDone, int opsDone, int opsTotal,
                              Bool cancelButton);

EXTERN void *Msg_LazyProgressStart(const char *idFmt, ...)
       PRINTF_DECL(1, 2);
EXTERN void Msg_LazyProgress(void *handle, int percent);
EXTERN void Msg_LazyProgressEnd(void *handle);


EXTERN HintResult Msg_Hint(Bool defaultShow, HintOptions options,
                           const char *idFmt, ...)
       PRINTF_DECL(3, 4);
EXTERN HintResult Msg_HintMsgList(Bool defaultShow, HintOptions options, Msg_List *msg);
EXTERN int Msg_CompareAnswer(Msg_String const *buttons, unsigned answer,
			     const char *string);
EXTERN char *Msg_GetString(const char *idString);
EXTERN char *Msg_GetStringSafe(const char *idString);
EXTERN char *Msg_GetPlainButtonText(const char *idString);
EXTERN const char *Msg_GetLocale(void);
EXTERN void Msg_SetLocale(const char *locale, const char *binaryName);
EXTERN char *Msg_GetMessageFilePath(const char *locale, const char *binaryName,
				    const char *extension);
EXTERN char *Msg_FormatFloat(double value, unsigned int precision);
EXTERN char *Msg_FormatSizeInBytes(uint64 size);
EXTERN Bool Msg_LoadMessageFile(const char *locale, const char *fileName);


/*
 * Message buffer management
 */

EXTERN const char *Msg_GetMessages(void);
EXTERN const char *Msg_GetMessagesAndReset(void);
EXTERN Msg_List *Msg_GetMsgList(void);
EXTERN Msg_List *Msg_GetMsgListAndReset(void);
EXTERN void Msg_FreeMsgList(Msg_List *messages);
EXTERN char *Msg_LocalizeList(Msg_List *messages);
EXTERN void Msg_Reset(Bool log);
EXTERN Bool Msg_Present(void);
EXTERN void Msg_Exit(void);


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

EXTERN void Msg_SetCallback(MsgCallback *cb);
EXTERN void Msg_GetCallback(MsgCallback *cb);


/*
 * Conversion functions
 */

#define Msg_ErrString() (Err_ErrString())
#define Msg_Errno2String(errorNumber) ( \
    Err_Errno2String(errorNumber) )

#ifdef _WIN32
EXTERN const char *Msg_HResult2String(long hr);
#endif


#endif // ifndef _MSG_H_
