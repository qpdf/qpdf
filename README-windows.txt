Common Setup
============

To be able to build qpdf and run its test suite, you must have either
Cygwin or MSYS from MinGW (>= 1.0.11) installed.  If you want to build
with Microsoft Visual C++, either Cygwin or MSYS will do.  If you want
to build with MinGW, you must use MSYS rather than Cygwin.

As of this writing, the image comparison tests confuse ghostscript in
cygwin, but there's a chance they might work at some point.  If you
want to run them, you need ghostscript and tiff utils as well.  Then
omit --disable-test-compare-images from the configure statements given
below.  The image comparison tests have not been tried under MSYS.

Jian Ma <stronghorse@tom.com> has generously provided a port of QPDF
that works with Microsoft VC6.  Several changes are required, but they
are well documented in his port.  You can find the VC6 port in the
contrib area of the qpdf download area.


External Libraries
==================

In order to build qpdf, you must have copies of zlib and pcre.  The
easy way to get them is to download them from the qpdf download area.
There are packages called external-libs-bin.zip and
external-libs-src.zip.  If you are building with MSVC 9 (.NET 2008) or
MINGW 4.4, you can just extract the external-libs-bin.zip zip file
into the top-level qpdf source tree.  It will create a directory
called external-libs which contains header files and precompiled
libraries.  Passing --enable-external-libs to ./configure (which is
done automatically if you follow the instructions below) is sufficient
to find them.

You can also obtain pcre and zlib directly on your own and install
them.  If you are using mingw, you can just set CPPFLAGS, LDFLAGS, and
LIBS when you run ./configure so that it can find the header files and
libraries.  If you are building with msvc and you want to do this, it
probably won't work because ./configure doesn't know how to interpret
LDFLAGS and LIBS properly for MSVC (though qpdf's own build system
does).  In this case, you can probably get away with cheating by
passing --enable-external-libs to ./configure and then just editing
CPPFLAGS, LDFLAGS, LIBS in the generated autoconf.mk file.  Note that
you should use UNIX-like syntax (-I, -L, -l) even though this is not
what cl takes on the command line.  qpdf's build rules will fix it.


Building with MinGW
===================

QPDF is known to build and pass its test suite with MSYS-1.0.11 and
gcc 4.4.0 with C++ support.  If you also have ActiveState Perl in your
path and the external-libs distribution described above, you can fully
configure, build, and test qpdf in this environment.  You will most
likely not be able to build qpdf with mingw using cygwin.

From your MSYS prompt, run

  ./config-mingw

and then

  make

Note that ./config-mingw just runs ./configure with specific
arguments, so you can look at it, make adjustments, and manually run
configure instead.

Add the absolute path to the libqpdf/build directory to your PATH.
Make sure you can run the qpdf command by typing qpdf/build/qpdf and
making sure you get a help message rather than an error loading the
DLL or no output at all.  Run the test suite by typing

  make check

If all goes well, you should get a passing test suite.

To create an installation directory, run make install.  This will
create install-mingw/qpdf-VERSION and populate it.  The binary
download of qpdf for Windows with mingw is created from this
directory.


Building with MSVC .NET 2008 Express
====================================

These instructions would likely work with newer version of MSVC or
with full version of MSVC.  They may also work with .NET 2005.  They
have only been tested with .NET 2008 Express (VC9.0).  You may follow
these instructions from either Cygwin or from MSYS, though only MSYS
is regularly tested.

You should first set up your environment to be able to run MSVC from
the command line.  There is usually a batch file included with MSVC
that does this.  From that cmd prompt, you can start your cygwin
shell.

Configure as follows:

  ./config-msvc

and then

  make

Note that ./config-msvc just runs ./configure with specific arguments,
so you can look at it, make adjustments, and manually run configure
instead.

NOTE: automated dependencies are not generated with the msvc build.
If you're planning on making modifications, you should probably work
with mingw.  If there is a need, I can add dependency information to
the msvc build, but since I only use it for generating release
versions, I haven't bothered.

Once built, add the full path to the libqpdf/build directory to your
path and run

  make check

to run the test suite.

If you are building with MSVC and want to debug a crash in MSVC's
debugger, first start an instance of Visual C++.  Then run qpdf.  When
the abort/retry/ignore dialog pops up, first attach the process from
within visual C++, and then click Retry in qpdf.

A release version of qpdf is built by default.  If you want to link
against debugging libraries, you will have to change /MD to /MDd in
make/msvc.mk.  Note that you must redistribute the Microsoft runtime
DLLs.  Linking with static runtime (/MT) won't work; see "Static
Runtime" below for details.


Runtime DLLs
============

Both build methods create executables and DLLs that are dependent on
the compiler's runtime DLLs.  When you run make install, the
installation process will automatically detect the DLLs and copy them
into the installation bin directory.  Look at the copy_dlls script for
details on how this is accomplished.

Redistribution of the runtime DLL is unavoidable as of this writing;
see "Static Runtime" below for details.


Static Runtime
==============

Building the DLL and executables with static runtime does not work
with either Visual C++ .NET 2008 (a.k.a. vc9) using /MT or with mingw
(at least as of 4.4.0) using -static-libgcc.  The reason is that, in
both cases, there is static data involved with exception handling, and
when the runtime is linked in statically, exceptions cannot be thrown
across the DLL to EXE boundary.  Since qpdf uses exception handling
extensively for error handling, we have no choice but to redistribute
the C++ runtime DLLs.  Maybe this will be addressed in a future
version of the compilers.
