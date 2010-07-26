/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_SOCK_H__
#   define __COMPAT_SOCK_H__

#include <linux/stddef.h> /* for NULL */
#include <net/sock.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static inline wait_queue_head_t *sk_sleep(struct sock *sk)
{
    return sk->sk_sleep;
}
#endif


/*
 * Prior to 2.6.24, there was no sock network namespace member. In 2.6.26, it
 * was hidden behind accessor functions so that its behavior could vary
 * depending on the value of CONFIG_NET_NS.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
# define compat_sock_net(sk)            sock_net(sk)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
# define compat_sock_net(sk)            sk->sk_net
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)

#ifndef CONFIG_FILTER
# define sk_filter(sk, skb, needlock)    0
#endif

/* Taken from 2.6.16's sock.h and modified for macro. */
# define compat_sk_receive_skb(sk, skb, nested)         \
   ({                                                   \
     int rc = NET_RX_SUCCESS;                           \
                                                        \
     if (sk_filter(sk, skb, 0)) {                       \
        kfree_skb(skb);                                 \
     } else {                                           \
        skb->dev = NULL;                                \
        bh_lock_sock(sk);                               \
        if (!sock_owned_by_user(sk)) {                  \
           rc = (sk)->sk_backlog_rcv(sk, skb);          \
        } else {                                        \
           sk_add_backlog(sk, skb);                     \
        }                                               \
        bh_unlock_sock(sk);                             \
     }                                                  \
                                                        \
     sock_put(sk);                                      \
     rc;                                                \
    })
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
# define compat_sk_receive_skb(sk, skb, nested) sk_receive_skb(sk, skb)
#else
# define compat_sk_receive_skb(sk, skb, nested) sk_receive_skb(sk, skb, nested)
#endif

#endif /* __COMPAT_SOCK_H__ */
