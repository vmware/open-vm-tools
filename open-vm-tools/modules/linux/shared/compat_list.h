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

#ifndef __COMPAT_LIST_H__
#   define __COMPAT_LIST_H__

#include <linux/list.h>

/*
 * list_add_tail is with us since 2.4.0, or something like that.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#define list_add_tail(newe, head) do {  \
   struct list_head *__h = (head);      \
   __list_add((newe), __h->prev, __h);  \
} while (0)
#endif

/*
 * list_for_each_safe() showed up in 2.4.10, but it may be backported so we
 * just check for its existence.
 */
#ifndef list_for_each_safe
# define list_for_each_safe(pos, n, head) \
         for (pos = (head)->next, n = pos->next; pos != (head); \
                 pos = n, n = pos->next)
#endif

/*
 * list_for_each_entry() showed up in 2.4.20, but it may be backported so we
 * just check for its existence.
 */
#ifndef list_for_each_entry
# define list_for_each_entry(pos, head, member) \
         for (pos = list_entry((head)->next, typeof(*pos), member); \
              &pos->member != (head); \
              pos = list_entry(pos->member.next, typeof(*pos), member))
#endif

#endif /* __COMPAT_LIST_H__ */
