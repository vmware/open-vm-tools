/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/



/*
 * hgfs.h --
 *
 * Header file for public common data types used in the VMware
 * Host/Guest File System (hgfs).
 *
 * This file is included by hgfsProto.h, which defines message formats
 * used in the hgfs protocol, and by hgfsDev.h, which defines the
 * interface between the kernel and the hgfs pserver. [bac]
 */


#ifndef _HGFS_H_
# define _HGFS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"
#include "vm_assert.h"


/*
 * Maximum allowed packet size in bytes. All hgfs code should be made
 * safe with respect to this limit.
 */
#define HGFS_PACKET_MAX 6144


/*
 * Open mode
 *
 * These are equivalent to the O_RDONLY, O_WRONLY, O_RDWR open flags
 * in Unix; they specify which type of access is being requested.  These three
 * modes are mutually exclusive and one is required; all other flags are
 * modifiers to the mode and must come afterwards as a bitmask.  Beware that
 * HGFS_OPEN_MODE_READ_ONLY contains the value 0 so simply masking another
 * variable with it to detect its presence is not safe.  The _ACCMODES entry in
 * the enum serves as a bitmask for the others.
 *
 * Changing the order of this enum will break stuff.
 *
 * This definition is used in some places that don't include
 * hgfsProto.h, which is why it is here instead of there.
 */
typedef enum {
   HGFS_OPEN_MODE_READ_ONLY,
   HGFS_OPEN_MODE_WRITE_ONLY,
   HGFS_OPEN_MODE_READ_WRITE,
   HGFS_OPEN_MODE_ACCMODES,
   /* You cannot add anything else here.  Really. */
} HgfsOpenMode;

/*
 * Open flags.
 *
 * Each should be shifted left by HGFS_OPEN_MODE_READ_WRITE plus whatever flag
 * number they are, starting with zero.
 *
 * The sequential flag indicates that reads and writes on this handle should
 * not seek on each operation; instead, the system's file pointer will be used
 * so each operation is performed where the last one finished.  This flag is
 * necessary when reading from or writing to non-seekable files (such as procfs
 * nodes on Linux) but can also lead to inconsistent results if a client shares
 * a handle amongst several of its callers.  This flag should only be used when
 * the client knows the file is non-seekable and the burden of ensuring file
 * handles aren't shared falls upon the hgfs client, not the server.
 */
#define HGFS_OPEN_SEQUENTIAL    (1 << HGFS_OPEN_MODE_READ_WRITE)

/* Masking helpers. */
#define HGFS_OPEN_MODE_ACCMODE(mode)    (mode & HGFS_OPEN_MODE_ACCMODES)
#define HGFS_OPEN_MODE_FLAGS(mode)      (mode & ~HGFS_OPEN_MODE_ACCMODES)

#define HGFS_OPEN_MODE_IS_VALID_MODE(mode)      \
   (HGFS_OPEN_MODE_ACCMODE(mode) == HGFS_OPEN_MODE_READ_ONLY  ||   \
    HGFS_OPEN_MODE_ACCMODE(mode) == HGFS_OPEN_MODE_WRITE_ONLY ||   \
    HGFS_OPEN_MODE_ACCMODE(mode) == HGFS_OPEN_MODE_READ_WRITE)


/*
 * Return status for replies from the server.
 *
 * Changing the order of this enum will break the protocol; new status
 * types should be added at the end.
 *
 * This definition is used in some places that don't include
 * hgfsProto.h, which is why it is here instead of there.
 *
 * XXX: So we have a problem here. At some point, HGFS_STATUS_INVALID_NAME was
 * added to the list of errors. Later, HGFS_STATUS_GENERIC_ERROR was added, but
 * it was added /before/ HGFS_STATUS_INVALID_NAME. Nobody noticed because the
 * error codes travelled from hgfsProto.h to hgfs.h in that same change. Worse,
 * we GA'ed a product (Server 1.0) this way.
 *
 * XXX: I've reversed the order because otherwise new HGFS clients working 
 * against WS55-era HGFS servers will think they got HGFS_STATUS_GENERIC_ERROR 
 * when the server sent them HGFS_STATUS_INVALID_NAME. This was a problem
 * the Linux client converts HGFS_STATUS_GENERIC_ERROR to -EIO, which causes
 * HgfsLookup to fail unexpectedly (normally HGFS_STATUS_INVALID_NAME is
 * converted to -ENOENT, an expected result in HgfsLookup).
 */
typedef enum {
   HGFS_STATUS_SUCCESS,
   HGFS_STATUS_NO_SUCH_FILE_OR_DIR,
   HGFS_STATUS_INVALID_HANDLE,
   HGFS_STATUS_OPERATION_NOT_PERMITTED,
   HGFS_STATUS_FILE_EXISTS,
   HGFS_STATUS_NOT_DIRECTORY,
   HGFS_STATUS_DIR_NOT_EMPTY,
   HGFS_STATUS_PROTOCOL_ERROR,
   HGFS_STATUS_ACCESS_DENIED,
   HGFS_STATUS_INVALID_NAME,
   HGFS_STATUS_GENERIC_ERROR,
   HGFS_STATUS_SHARING_VIOLATION,
   HGFS_STATUS_NO_SPACE,
   HGFS_STATUS_OPERATION_NOT_SUPPORTED,
   HGFS_STATUS_NAME_TOO_LONG,
} HgfsStatus;

/*
 * HGFS RPC commands
 *
 * HGFS servers can run in a variety of places across several different
 * transport layers. These definitions constitute all known RPC commands.
 *
 * For each definition, there is both the server string (the command itself)
 * as well as a client "prefix", which is the command followed by a space.
 * This is provided for convenience, since clients will need to copy both
 * the command and the space into some buffer that is then sent over the
 * backdoor.
 *
 * In Host --> Guest RPC traffic, the host endpoint is TCLO and the guest 
 * endpoint is RpcIn. TCLO is a particularly confusing name choice which dates 
 * back to when the host was to send raw TCL code to the guest (TCL Out == 
 * TCLO).
 *
 * In Guest --> Host RPC traffic, the guest endpoint is RpcOut and the host
 * endpoint is RPCI.
 */

/*
 * When an RPCI listener registers for this command, HGFS requests are expected
 * to be synchronously sent from the guest and replies are expected to be 
 * synchronously returned.
 * 
 * When an RpcIn listener registers for this command, requests are expected to 
 * be asynchronously sent from the host and synchronously returned from the 
 * guest.
 *
 * In short, an endpoint sending this command is sending a request whose reply
 * should be returned synchronously.
 */
#define HGFS_SYNC_REQREP_CMD "f"
#define HGFS_SYNC_REQREP_CLIENT_CMD HGFS_SYNC_REQREP_CMD " "
#define HGFS_SYNC_REQREP_CLIENT_CMD_LEN (sizeof HGFS_SYNC_REQREP_CLIENT_CMD - 1)

/*
 * When an RPCI listener registers for this command, HGFS requests are expected
 * to be synchronously sent from the guest and replies are expected to be 
 * asynchronously returned.
 * 
 * When an RpcIn listener registers for this command, requests are expected to 
 * be asynchronously sent from the host and asynchronously returned from the 
 * guest.
 *
 * In short, an endpoint sending this command is sending a request whose reply
 * should be returned asynchronously.
 */
#define HGFS_ASYNC_REQUEST_CMD "g"
#define HGFS_ASYNC_REQUEST_CLIENT_CMD HGFS_ASYNC_REQUEST_CMD " "
#define HGFS_ASYNC_REQUEST_CLIENT_CMD_LEN (sizeof HGFS_ASYNC_REQUEST_CLIENT_CMD - 1)

/*
 * An endpoint sending this command is sending an asynchronous HGFS reply to a
 * request sent by the HGFS_ASYNC_REQUEST command.
 */
#define HGFS_ASYNC_REPLY_CMD "h"
#define HGFS_ASYNC_REPLY_CLIENT_CMD HGFS_ASYNC_REPLY_CMD " "
#define HGFS_ASYNC_REPLY_CLIENT_CMD_LEN (sizeof HGFS_ASYNC_REPLY_CLIENT_CMD - 1)

/*
 * Ensuring that all commands are the same length eases the implementation of 
 * HGFS client code.
 */
MY_ASSERTS(HGFS_CMD_LENGTHS,
           ASSERT_ON_COMPILE(HGFS_SYNC_REQREP_CLIENT_CMD_LEN ==
                             HGFS_ASYNC_REQUEST_CLIENT_CMD_LEN);
           ASSERT_ON_COMPILE(HGFS_ASYNC_REQUEST_CLIENT_CMD_LEN ==
                             HGFS_ASYNC_REPLY_CLIENT_CMD_LEN);
)

/* 
 * This is just for the sake of macro naming. Since we are guaranteed
 * equal command lengths, defining command length via a generalized macro name
 * will prevent confusion.
 */
#define HGFS_CLIENT_CMD_LEN HGFS_SYNC_REQREP_CLIENT_CMD_LEN

#endif // _HGFS_H_
