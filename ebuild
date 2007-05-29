# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

DESCRIPTION="a lightweight, unobtrusive, Dvorak-optimized editor"
HOMEPAGE="http://aoeui.sourceforge.net"
SRC_URI="mirror://sourceforge/aoeui/${P}.tgz"
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~x86 ~amd64"
IUSE=""

src_install() {
	emake DESTDIR="${D}" install || die
}
