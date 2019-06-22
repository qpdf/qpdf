#!/bin/sh
set -e
aclocal -I m4
autoheader
autoconf
sha256sum configure.ac aclocal.m4 libqpdf/qpdf/qpdf-config.h.in m4/* >| autofiles.sums
