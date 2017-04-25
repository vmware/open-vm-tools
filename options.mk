# $NetBSD: options.mk,v 1.2 2010/12/16 11:52:15 obache Exp $

CONFIGURE_ARGS+=		--without-x
CONFIGURE_ARGS+=		--enable-deploypkg=no
CONFIGURE_ARGS+=		--without-xerces
CONFIGURE_ARGS+=		--without-kernel-modules
CONFIGURE_ARGS+=		--disable-grabbitmqproxy
CONFIGURE_ARGS+=		--disable-vgauth
