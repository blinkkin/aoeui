# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

DESCRIPTION="a lightweight, unobtrusive, Dvorak-optimized editor"
HOMEPAGE="http://sites.google.com/site/aoeuiandasdfg/"
SRC_URI="http://aoeui.googlecode.com/files/${P}.tgz"

inherit eutils

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE=""

src_unpack() {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}"/lutil.patch
}

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed"
}
