#!/bin/sh
set -e
aclocal -I m4
autoheader
autoconf
md5sum configure.ac m4/* >| autofiles.sums
