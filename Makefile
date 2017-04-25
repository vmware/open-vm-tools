# $NetBSD$

DISTNAME=	open-vm-tools-10.0.5-3227872
CATEGORIES=	sysutils
MASTER_SITES=	http://downloads.sourceforge.net/project/open-vm-tools/open-vm-tools/stable-10.0.x/

MAINTAINER=	apoorva@brkt.com
HOMEPAGE=	http://downloads.sourceforge.net/project/open-vm-tools/open-vm-tools/stable-10.0.x/
COMMENT=	TODO: Short description of the package
#LICENSE=	# TODO: (see mk/license.mk)

GNU_CONFIGURE=	yes
USE_LIBTOOL=    yes
USE_TOOLS+=	pkg-config autoconf automake
USE_LANGUAGES=	c c++

PKGCONFIG_OVERRIDE+=	libDeployPkg/libDeployPkg.pc.in
PKGCONFIG_OVERRIDE+=	libguestlib/vmguestlib.pc.in

INSTALLATION_DIRS+=             ${PKG_SYSCONFDIR} share/examples/vmware-tools
PKG_SYSCONFSUBDIR=              vmware-tools
EGDIR=                          ${PREFIX}/share/examples/vmware-tools
CONF_FILES_PERMS+=              ${EGDIR}/statechange.subr ${PKG_SYSCONFDIR}/statechange.subr ${REAL_ROOT_USER} ${REAL_ROOT_GROUP} 755
CONF_FILES_PERMS+=              ${EGDIR}/poweroff-vm-default ${PKG_SYSCONFDIR}/poweroff-vm-default ${REAL_ROOT_USER} ${REAL_ROOT_GROUP} 755
CONF_FILES_PERMS+=              ${EGDIR}/poweron-vm-default ${PKG_SYSCONFDIR}/poweron-vm-default ${REAL_ROOT_USER} ${REAL_ROOT_GROUP} 755
CONF_FILES_PERMS+=              ${EGDIR}/resume-vm-default ${PKG_SYSCONFDIR}/resume-vm-default ${REAL_ROOT_USER} ${REAL_ROOT_GROUP} 755
CONF_FILES_PERMS+=              ${EGDIR}/suspend-vm-default ${PKG_SYSCONFDIR}/suspend-vm-default ${REAL_ROOT_USER} ${REAL_ROOT_GROUP} 755
RCD_SCRIPTS=                    vmtools

pre-configure:
	cd ${WRKSRC} && autoreconf -f

.include "../../devel/glib2/buildlink3.mk"
.include "../../textproc/icu/buildlink3.mk"
.include "../../net/libdnet/buildlink3.mk"
.include "../../mk/bsd.pkg.mk"
.include "options.mk"
