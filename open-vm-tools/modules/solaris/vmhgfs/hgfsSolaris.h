/*********************************************************
 * Copyright (C) 2004-2016 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/


/*
 * hgfsSolaris.h --
 *
 * Contains declarations needed in entire Solaris HGFS module.
 *
 */

/* XXX: Split the non-kernel parts off into another header */

#ifndef __HGFS_H_
#define __HGFS_H_

/*
 * System includes
 */

/*
 * Kernel system includes
 */
#ifdef _KERNEL

/*
 * These must be in this order, so we include them here, and only here, so
 * that this ordering is guaranteed.
 *
 * <sys/types.h> defines min and max macros (that shouldn't be used because
 * they're broken) and <sys/conf.h> includes <sys/systm.h> which defines
 * functions named min and max.  The function definitions must come first.
 */
#include <sys/conf.h>           /* D_NEW flag and other includes */
#include <sys/types.h>          /* various typedefs */

#endif /* _KERNEL */

/*
 * VMware includes
 */
#include "hgfsProto.h"
#include "hgfsState.h"
#include "filesystem.h"

/*
 * Kernel VMware includes
 */
#ifdef _KERNEL

#include "dbllnklst.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"

#endif /* _KERNEL */


/*
 * Macros
 */

#define HGFS_PAYLOAD_MAX(reply)         (HGFS_PACKET_MAX - sizeof *reply)
#define HGFS_FS_NAME                    "vmhgfs"
#define HGFS_BLOCKSIZE                  1024


/*
 * Kernel only macros
 */
#ifdef _KERNEL

/* Determines size of request pool */
#define HGFS_MAX_OUTSTANDING_REQS       4

/* HGFS cmn_err() levels */
#define HGFS_ERROR                      (CE_WARN)

/* Internal error code(s) */
#define HGFS_ERR                        (-1)
#define HGFS_ERR_NULL_INPUT             (-50)
#define HGFS_ERR_NODEV                  (-51)
#define HGFS_ERR_INVAL                  (-52)

/*
 * Don't change this to KM_NOSLEEP without first making sure that we handle
 * the possibility of kmem_zalloc() failing: KM_SLEEP guarantees it won't fail
 */
#define HGFS_ALLOC_FLAG                 (KM_SLEEP)

/* Accessing root inode and vnode */
#define HGFS_ROOT_VNODE(sip)            (sip->rootVnode)


#endif /* _KERNEL */


/*
 * Structures
 */

/* We call them *Header in the Solaris code for clarity. */
typedef HgfsReply HgfsReplyHeader;
typedef HgfsRequest HgfsRequestHeader;

/*
 * Kernel only structures and variables
 */
#ifdef _KERNEL

/*
 * Each request will traverse through this set of states.  See
 * docs/request-lifecycle.txt for an explanation of these and the API for
 * interacting with requests.
 */
typedef enum {
   HGFS_REQ_UNUSED = 1,
   HGFS_REQ_ALLOCATED,
   HGFS_REQ_SUBMITTED,
   HGFS_REQ_ABANDONED,
   HGFS_REQ_ERROR,
   HGFS_REQ_COMPLETED
} HgfsReqState;


/*
 * General request structure.  Specific requests and replies are placed in the
 * packet of this structure.
 */
typedef struct HgfsReq {
   DblLnkLst_Links listNode;            /* Node to connect the request to one of
                                         * the lists (free or pending) */
   kcondvar_t condVar;                  /* Condition variable to wait for and
                                           signal presence of reply. Used with
                                           the reqMutex in HgfsSuperInfo. */
   HgfsReqState state;                  /* Indicates state of request */
   uint32_t id;                         /* The unique identifier of this request */
   uint32_t packetSize;                 /* Total size of packet */
   char packet[HGFS_PACKET_MAX];        /* Contains both requests and replies */
} HgfsReq;


/*
 * The global state structure for the entire module.  This is allocated in
 * HgfsDevAttach() and deallocated in HgfsDevDetach().
 *
 * Note that reqMutex and reqFreeList are also used for synchronization between
 * the filesystem and driver.  See docs/synchronization.txt for details.
 */
typedef struct HgfsSuperInfo {
   kmutex_t reqMutex;                   /* Serializes sending of requests */

   /* Free request list */
   DblLnkLst_Links reqFreeList;         /* Anchor for free request list */
   kmutex_t reqFreeMutex;               /* For protection of reqFreeList */
   kcondvar_t reqFreeCondVar;           /* For waiting on free request list */

   /* For filesystem */
   struct vfs *vfsp;                    /* Our filesystem structure */
   struct vnode *rootVnode;             /* Root vnode of the filesystem */
   HgfsFileHashTable fileHashTable;     /* File hash table */

   int (*sendRequest)(HgfsReq *req);    /* Current transport's sent method */
   void (*cancelRequest)(HgfsReq *req); /* Current transport's cancel method */
   Bool (*transportInit)(void);
   void (*transportCleanup)(void);
} HgfsSuperInfo;


/*
 * Global Variables
 */

/* Pool of request structures */
HgfsReq requestPool[HGFS_MAX_OUTSTANDING_REQS];

/*
 * Used to access shared state of driver and filesystem.  superInfoHead is
 * a pointer to state managed by Solaris, hgfsInstance is the index into this state
 * list, and is set in HgfsDevAttach().
 *
 * Note that both the driver and filesystem use ddi_get_soft_state() to get
 * a pointer to the superinfo.  Both use superInfoHead, but the device uses the
 * instance number derived from passed in arguments and the filesystem uses
 * hgfsInstance.  This is not a problem as long as the instance number cannot
 * change, which /should/ be guaranteed, and there is only a single instance,
 * which cannot happen.
 */
void *superInfoHead;
int hgfsInstance;

#endif /* _KERNEL */

#endif /* __HGFS_H_ */
