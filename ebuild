# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

DESCRIPTION="a lightweight, unobtrusive, Dvorak-optimized editor"
SRC_URI="http://downloads.sourceforge.net/aoeui/${P}.tgz"
# ? use_mirror=internap
HOMEPAGE="http://aoeui.sourceforge.net"
LICENSE="GPL-2"
KEYWORDS="x86"
SLOT="0"
IUSE=""

src_compile() {
	cd ${S}
	emake OPTI="${CFLAGS}" || die
}

src_install() {
	make DESTDIR=${D} MANDIR=/share/man install || die
}
