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

/*
 * Between 2.5.70 and 2.5.71 all sock members were renamed from XXX to sk_XXX.
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 71)
# define compat_sk_backlog_rcv          backlog_rcv
# define compat_sk_destruct             destruct
# define compat_sk_shutdown             shutdown
# define compat_sk_receive_queue        receive_queue
# define compat_sk_sleep                sleep
# define compat_sk_err                  err
# define compat_sk_state_change         state_change
# define compat_sk_data_ready           data_ready
# define compat_sk_write_space          write_space
# define compat_sk_error_report         error_report
# define compat_sk_type                 type
# define compat_sk_refcnt               refcnt
# define compat_sk_state                state
# define compat_sk_error_report         error_report
# define compat_sk_socket               socket
# define compat_sk_ack_backlog          ack_backlog
# define compat_sk_max_ack_backlog      max_ack_backlog
# define compat_sk_user_data            user_data
# define compat_sk_rcvtimeo             rcvtimeo
#else
# define compat_sk_backlog_rcv          sk_backlog_rcv
# define compat_sk_destruct             sk_destruct
# define compat_sk_shutdown             sk_shutdown
# define compat_sk_receive_queue        sk_receive_queue
# define compat_sk_sleep                sk_sleep
# define compat_sk_err                  sk_err
# define compat_sk_state_change         sk_state_change
# define compat_sk_data_ready           sk_data_ready
# define compat_sk_write_space          sk_write_space
# define compat_sk_error_report         sk_error_report
# define compat_sk_type                 sk_type
# define compat_sk_refcnt               sk_refcnt
# define compat_sk_state                sk_state
# define compat_sk_error_report         sk_error_report
# define compat_sk_socket               sk_socket
# define compat_sk_ack_backlog          sk_ack_backlog
# define compat_sk_max_ack_backlog      sk_max_ack_backlog
# define compat_sk_user_data            sk_user_data
# define compat_sk_rcvtimeo             sk_rcvtimeo
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static inline wait_queue_head_t *sk_sleep(struct sock *sk)
{
    return sk->compat_sk_sleep;
}
#endif

/*
 * Prior to 2.5.65, struct sock contained individual fields for certain
 * socket flags including SOCK_DONE. Between 2.5.65 and 2.5.71 these were
 * replaced with a bitmask but the generic bit test functions were used.
 * In 2.5.71, these were replaced with socket specific functions.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 71)
# define compat_sock_test_done(sk)      sock_flag(sk, SOCK_DONE)
# define compat_sock_set_done(sk)       sock_set_flag(sk, SOCK_DONE)
# define compat_sock_reset_done(sk)     sock_reset_flag(sk, SOCK_DONE)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 65)
# define compat_sock_test_done(sk)      test_bit(SOCK_DONE, &(sk)->flags)
# define compat_sock_set_done(sk)       __set_bit(SOCK_DONE, &(sk)->flags)
# define compat_sock_reset_done(sk)     __clear_bit(SOCK_DONE, &(sk)->flags)
#else
# define compat_sock_test_done(sk)      (sk)->done
# define compat_sock_set_done(sk)       ((sk)->done = 1)
# define compat_sock_reset_done(sk)     ((sk)->done = 0)
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 42)
# define compat_sock_owned_by_user(sk)  ((sk)->lock.users != 0)
#else
# define compat_sock_owned_by_user(sk)  sock_owned_by_user(sk)
#endif

/*
 * Up until 2.4.21 for the 2.4 series and 2.5.60 for the 2.5 series,
 * sk_filter() calls were protected with CONFIG_FILTER.  Wrapping our compat
 * definition in a similar check allows us to build on those kernels.
 *
 */
#ifdef CONFIG_FILTER
/*
 * Unfortunately backports for certain kernels require the use of an autoconf
 * program to check the interface for sk_filter().
 */
# ifndef VMW_HAVE_NEW_SKFILTER
/* 
 * Up until 2.4.21 for the 2.4 series and 2.5.60 for the 2.5 series,
 * callers to sk->filter were responsible for ensuring that the filter
 * was not NULL.
 * Additionally, the new version of sk_filter returns 0 or -EPERM on error
 * while the old function returned 0 or 1. Return -EPERM here as well to
 * be consistent.
 */
#  define compat_sk_filter(sk, skb, needlock)           \
    ({                                                  \
       int rc = 0;                                      \
                                                        \
       if ((sk)->filter) {                              \
	  rc = sk_filter(skb, (sk)->filter);            \
          if (rc) {                                     \
             rc = -EPERM;                               \
          }                                             \
       }                                                \
                                                        \
       rc;                                              \
    })
# else
#  define compat_sk_filter(sk, skb, needlock)  sk_filter(sk, skb, needlock)
# endif
#else
# define compat_sk_filter(sk, skb, needlock)   0
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
/* Taken from 2.6.16's sock.h and modified for macro. */
# define compat_sk_receive_skb(sk, skb, nested)         \
   ({                                                   \
     int rc = NET_RX_SUCCESS;                           \
                                                        \
     if (compat_sk_filter(sk, skb, 0)) {                \
        kfree_skb(skb);                                 \
        sock_put(sk);                                   \
     } else {                                           \
        skb->dev = NULL;                                \
        bh_lock_sock(sk);                               \
        if (!compat_sock_owned_by_user(sk)) {           \
           rc = (sk)->compat_sk_backlog_rcv(sk, skb);   \
        } else {                                        \
           sk_add_backlog(sk, skb);                     \
        }                                               \
        bh_unlock_sock(sk);                             \
        sock_put(sk);                                   \
     }                                                  \
                                                        \
     rc;                                                \
    })
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
# define compat_sk_receive_skb(sk, skb, nested) sk_receive_skb(sk, skb)
#else
# define compat_sk_receive_skb(sk, skb, nested) sk_receive_skb(sk, skb, nested)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 72)
/*
 * Before 2.5.72, the helper socket functions for hlist management did not
 * exist, so we use the sklist_ functions instead.  These are not ideal since
 * they grab a system-wide sklist lock despite not needing it since we provide
 * our own list.
 */
#define compat_sk_next next /* for when we find out it became sk_next */
# define compat_sklist_table                    struct sock *
/* This isn't really used in the iterator, but we need something. */
# define compat_sklist_table_entry              struct sock
# define compat_sk_for_each(sk, node, list)     \
   for (sk = *(list), node = NULL; sk != NULL; sk = (sk)->compat_sk_next)
# define compat_sk_add_node(sk, list)           sklist_insert_socket(list, sk)
# define compat_sk_del_node_init(sk, list)      sklist_remove_socket(list, sk)
#else
# define compat_sklist_table                    struct hlist_head
# define compat_sklist_table_entry              struct hlist_node
# define compat_sk_for_each(sk, node, list)     sk_for_each(sk, node, list)
# define compat_sk_add_node(sk, list)           sk_add_node(sk, list)
# define compat_sk_del_node_init(sk, list)      sk_del_node_init(sk)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 6)
# define compat_sock_create_kern sock_create
#else
# define compat_sock_create_kern sock_create_kern
#endif

#endif /* __COMPAT_SOCK_H__ */
