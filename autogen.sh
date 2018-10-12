#!/bin/sh
set -e
aclocal -I m4
autoheader
autoconf
sha256sum configure.ac m4/* >| autofiles.sums
