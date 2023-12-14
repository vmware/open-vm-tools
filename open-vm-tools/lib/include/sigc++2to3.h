/*********************************************************
 * Copyright (C) 2022 VMware, Inc. All rights reserved.
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
 * sigc++2to3.h --
 *
 *     A header file that contain definations that will allow some sigc++2 APIs
 *     to work when builing with sigc++3.
 */

#pragma once


#include <type_traits>
#include <sigc++config.h>
#include <sigc++/sigc++.h>


#if SIGCXX_MAJOR_VERSION >= 3

namespace sigc {


// Bridge slot syntax change
template <typename T_ret, typename... T_args>
class slot : public slot<T_ret(T_args...)> {
private:
   using _true_slot = slot<T_ret(T_args...)>;
public:
   inline slot() = default;
   inline slot(const _true_slot& src) : _true_slot(src) {}
   template<typename T_functor>
   inline slot(const T_functor& func) : _true_slot(func) {}
};

template <typename T_ret>
using slot0 = slot<T_ret()>;
template <typename T_ret, typename T_arg1>
using slot1 = slot<T_ret(T_arg1)>;
template <typename T_ret, typename T_arg1, typename T_arg2>
using slot2 = slot<T_ret(T_arg1, T_arg2)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3>
using slot3 = slot<T_ret(T_arg1, T_arg2, T_arg3)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4>
using slot4 = slot<T_ret(T_arg1, T_arg2, T_arg3, T_arg4)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4, typename T_arg5>
using slot5 = slot<T_ret(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4, typename T_arg5, typename T_arg6>
using slot6 = slot<T_ret(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4, typename T_arg5, typename T_arg6, typename T_arg7>
using slot7 = slot<T_ret(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7)>;


// Bridge signal syntax change
template <typename T_ret, typename... T_args>
class signal : public signal<T_ret(T_args...)>, public trackable {
public:
   signal() = default;

   decltype(auto) make_slot() const {
      return mem_fun(*this, &signal::emit);
   }
};

template <typename T_ret>
using signal0 = signal<T_ret()>;
template <typename T_ret, typename T_arg1>
using signal1 = signal<T_ret(T_arg1)>;
template <typename T_ret, typename T_arg1, typename T_arg2>
using signal2 = signal<T_ret(T_arg1, T_arg2)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3>
using signal3 = signal<T_ret(T_arg1, T_arg2, T_arg3)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4>
using signal4 = signal<T_ret(T_arg1, T_arg2, T_arg3, T_arg4)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4, typename T_arg5>
using signal5 = signal<T_ret(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4, typename T_arg5, typename T_arg6>
using signal6 = signal<T_ret(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6)>;
template <typename T_ret, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4, typename T_arg5, typename T_arg6, typename T_arg7>
using signal7 = signal<T_ret(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7)>;


//Bridge bound_mem_functor
template <typename T_ret, typename T_obj>
using bound_mem_functor0 = bound_mem_functor<T_ret (T_obj::*)()>;
template <typename T_ret, typename T_obj, typename T_arg1>
using bound_mem_functor1 = bound_mem_functor<T_ret (T_obj::*)(T_arg1), T_arg1>;
template <typename T_ret, typename T_obj, typename T_arg1, typename T_arg2>
using bound_mem_functor2 = bound_mem_functor<T_ret (T_obj::*)(T_arg1, T_arg2), T_arg1, T_arg2>;
template <typename T_ret, typename T_obj, typename T_arg1, typename T_arg2, typename T_arg3>
using bound_mem_functor3 = bound_mem_functor<T_ret (T_obj::*)(T_arg1, T_arg2, T_arg3), T_arg1, T_arg2, T_arg3>;
template <typename T_ret, typename T_obj, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4>
using bound_mem_functor4 = bound_mem_functor<T_ret (T_obj::*)(T_arg1, T_arg2, T_arg3, T_arg4), T_arg1, T_arg2, T_arg3, T_arg4>;
template <typename T_ret, typename T_obj, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4, typename T_arg5>
using bound_mem_functor5 = bound_mem_functor<T_ret (T_obj::*)(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5), T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>;
template <typename T_ret, typename T_obj, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4, typename T_arg5, typename T_arg6>
using bound_mem_functor6 = bound_mem_functor<T_ret (T_obj::*)(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6), T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>;
template <typename T_ret, typename T_obj, typename T_arg1, typename T_arg2, typename T_arg3, typename T_arg4, typename T_arg5, typename T_arg6, typename T_arg7>
using bound_mem_functor7 = bound_mem_functor<T_ret (T_obj::*)(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7), T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>;



// Add old mem_fun API for pointer
template<typename T_return, typename T_obj, typename... T_arg>
inline decltype(auto)
mem_fun(T_obj *obj, T_return (T_obj::*func)(T_arg...))
{
   return bound_mem_functor<T_return (T_obj::*)(T_arg...), T_arg...>(*obj, func);
}

template<typename T_return, typename T_obj, typename... T_arg>
inline decltype(auto)
mem_fun(const T_obj *obj, T_return (T_obj::*func)(T_arg...) const)
{
   return bound_mem_functor<T_return (T_obj::*)(T_arg...) const, T_arg...>(*obj, func);
}


// Stub sigc::ref impl
template <typename T>
inline decltype(auto) ref(T& t) { return std::ref(t); }
template <typename T>
inline decltype(auto) ref(const T& t) { return std::cref(t); }


} //namespace sigc

#endif // SIGCXX_MAJOR_VERSION >= 3
