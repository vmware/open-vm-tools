/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_FS_H__
#   define __COMPAT_FS_H__

#include <linux/fs.h>

/*
 * 2.6.5+ kernels define FS_BINARY_MOUNTDATA. Since it didn't exist and
 * wasn't used prior, it's safe to define it to zero.
 */

#ifndef FS_BINARY_MOUNTDATA
#define FS_BINARY_MOUNTDATA 0
#endif

/*
 * MAX_LFS_FILESIZE wasn't defined until 2.5.4.
 */
#ifndef MAX_LFS_FILESIZE
#   include <linux/pagemap.h>
#   if BITS_PER_LONG == 32
#      define MAX_LFS_FILESIZE       (((u64)PAGE_CACHE_SIZE << (BITS_PER_LONG - 1)) - 1)
#   elif BITS_PER_LONG == 64
#      define MAX_LFS_FILESIZE       0x7fffffffffffffffUL
#   endif
#endif


/*
 * sendfile as a VFS op was born in 2.5.30. Unfortunately, it also changed
 * signatures, first in 2.5.47, then again in 2.5.70, then again in 2.6.8.
 * Luckily, the 2.6.8+ signature is the same as the 2.5.47 signature.  And
 * as of 2.6.23-rc1 sendfile is gone, replaced by splice_read...
 *
 * Let's not support sendfile from 2.5.30 to 2.5.47, because the 2.5.30
 * signature is much different and file_send_actor isn't externed.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
#define VMW_SENDFILE_NONE
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 8)
#define VMW_SENDFILE_NEW
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 70)
#define VMW_SENDFILE_OLD
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 47)
#define VMW_SENDFILE_NEW
#else
#define VMW_SENDFILE_NONE
#endif

/*
 * splice_read is there since 2.6.17, but let's avoid 2.6.17-rcX kernels...
 * After all nobody is using splice system call until 2.6.23 using it to
 * implement sendfile.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
#define VMW_SPLICE_READ 1
#endif

/*
 * Filesystems wishing to use generic page cache read/write routines are
 * supposed to implement aio_read and aio_write (calling into
 * generic_file_aio_read() and generic_file_aio_write() if necessary).
 *
 * The VFS exports do_sync_read() and do_sync_write() as the "new"
 * generic_file_read() and generic_file_write(), but filesystems need not
 * actually implement read and write- the VFS will automatically call
 * do_sync_write() and do_sync_read() when applications invoke the standard
 * read() and write() system calls.
 *
 * In 2.6.19, generic_file_read() and generic_file_write() were removed,
 * necessitating this change. AIO dates as far back as 2.5.42, but the API has
 * changed over time, so for simplicity, we'll only enable it from 2.6.19 and
 * on.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
# define VMW_USE_AIO
#endif


/*
 * The alloc_inode and destroy_inode VFS ops didn't exist prior to 2.4.21.
 * Without these functions, file systems can't embed inodes.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 21)
# define VMW_EMBED_INODE
#endif


/*
 * iget() was removed from the VFS as of 2.6.25-rc1. The replacement for iget()
 * is iget_locked() which was added in 2.5.17.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 17)
# define VMW_USE_IGET_LOCKED
#endif

/*
 * parent_ino was born in 2.5.5. For older kernels, let's use 2.5.5
 * implementation. It uses the dcache lock which is OK because per-dentry
 * locking appeared after 2.5.5.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 5)
#define compat_parent_ino(dentry) parent_ino(dentry)
#else
#define compat_parent_ino(dentry)                                             \
({                                                                            \
   ino_t res;                                                                 \
   spin_lock(&dcache_lock);                                                   \
   res = dentry->d_parent->d_inode->i_ino;                                    \
   spin_unlock(&dcache_lock);                                                 \
   res;                                                                       \
})
#endif


/*
 * putname changed to __putname in 2.6.6.
 */
#define compat___getname() __getname()
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 6)
#define compat___putname(name) putname(name)
#else
#define compat___putname(name) __putname(name)
#endif


/*
 * inc_nlink, drop_nlink, and clear_nlink were added in 2.6.19.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#define compat_inc_nlink(inode) ((inode)->i_nlink++)
#define compat_drop_nlink(inode) ((inode)->i_nlink--)
#define compat_clear_nlink(inode) ((inode)->i_nlink = 0)
#else
#define compat_inc_nlink(inode) inc_nlink(inode)
#define compat_drop_nlink(inode) drop_nlink(inode)
#define compat_clear_nlink(inode) clear_nlink(inode)
#endif


/*
 * i_size_write and i_size_read were introduced in 2.6.0-test1 
 * (though we'll look for them as of 2.6.1). They employ slightly different
 * locking in order to guarantee atomicity, depending on the length of a long,
 * whether the kernel is SMP, or whether the kernel is preemptible. Prior to
 * i_size_write and i_size_read, there was no such locking, so that's the
 * behavior we'll emulate.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 1)
#define compat_i_size_read(inode) ((inode)->i_size)
#define compat_i_size_write(inode, size) ((inode)->i_size = size)
#else
#define compat_i_size_read(inode) i_size_read(inode)
#define compat_i_size_write(inode, size) i_size_write(inode, size)
#endif


/*
 * filemap_fdatawrite was introduced in 2.5.12. Prior to that, modules used
 * filemap_fdatasync instead. In 2.4.18, both filemap_fdatawrite and 
 * filemap_fdatawait began returning status codes. Prior to that, they were 
 * void functions, so we'll just have them return 0.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 18)
#define compat_filemap_fdatawrite(mapping)                                    \
({                                                                            \
   int result = 0;                                                            \
   filemap_fdatasync(mapping);                                                \
   result;                                                                    \
})
#define compat_filemap_fdatawait(mapping)                                     \
({                                                                            \
   int result = 0;                                                            \
   filemap_fdatawait(mapping);                                                \
   result;                                                                    \
})
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 12)
#define compat_filemap_fdatawrite(mapping) filemap_fdatasync(mapping)
#define compat_filemap_fdatawait(mapping) filemap_fdatawait(mapping)
#else
#define compat_filemap_fdatawrite(mapping) filemap_fdatawrite(mapping)
#define compat_filemap_fdatawait(mapping) filemap_fdatawait(mapping)
#endif


/*
 * filemap_write_and_wait was introduced in 2.6.6 and exported for module use
 * in 2.6.16. It's really just a simple wrapper around filemap_fdatawrite and 
 * and filemap_fdatawait, which initiates a flush of all dirty pages, then 
 * waits for the pages to flush. The implementation here is a simplified form 
 * of the one found in 2.6.20-rc3.
 *
 * Unfortunately, it just isn't possible to implement this prior to 2.4.5, when
 * neither filemap_fdatawait nor filemap_fdatasync were exported for module
 * use. So we'll define it out and hope for the best.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 5)
#define compat_filemap_write_and_wait(mapping)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#define compat_filemap_write_and_wait(mapping)                                \
({                                                                            \
   int result = 0;                                                            \
   if (mapping->nrpages) {                                                    \
      result = compat_filemap_fdatawrite(mapping);                            \
      if (result != -EIO) {                                                   \
         int result2 = compat_filemap_fdatawait(mapping);                     \
         if (!result) {                                                       \
            result = result2;                                                 \
         }                                                                    \
      }                                                                       \
   }                                                                          \
   result;                                                                    \
})
#else
#define compat_filemap_write_and_wait(mapping) filemap_write_and_wait(mapping)
#endif


/*
 * invalidate_remote_inode was introduced in 2.6.0-test5. Prior to that, 
 * filesystems wishing to invalidate pages belonging to an inode called 
 * invalidate_inode_pages.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#define compat_invalidate_remote_inode(inode) invalidate_inode_pages(inode)
#else
#define compat_invalidate_remote_inode(inode) invalidate_remote_inode(inode)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
#define VMW_FSYNC_OLD
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
typedef umode_t compat_umode_t;
#else
typedef int compat_umode_t;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
#define d_make_root(inode) ({                      \
   struct dentry * ____res = d_alloc_root(inode);  \
   if (!____res) {                                 \
      iput(inode);                                 \
   }                                               \
   ____res;                                        \
})
#endif
#endif /* __COMPAT_FS_H__ */
