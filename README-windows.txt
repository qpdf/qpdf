Common Setup
============

You may need to disable antivirus software to run qpdf's test suite.

To be able to build qpdf and run its test suite, you must have MSYS
from MinGW installed, and you must have ActiveState Perl. The Perl
provided by MSYS won't work reliably. It partially works, but some
tests will fail with it because it doesn't support all the
capabilities required by the test driver. Here's what I did on my
system:

Install ActiveState perl. The versions of perl included with git bash
and mingw are not able to run the test suite.

Install MinGW-w64. From MinGW-w64 download page, grab the installer
and run it. First install the i686 compiler to C:\mingw-w64, and then
install x86_64 compiler to the same location. The installer will
automatically created mingw32 and mingw64 directories under mingw-w64.

Grab the latest mingw-get-inst from the MinGW project. We are using
this for shell and build utilties but not for the compiler. Run the
installer.

Install under basic:
  mingw-developer-toolkit
  msys-base

Use C:\mingw32\msys\1.0\msys.bat to start a shell. In the shell, run

mingw32-get install msys-zip

Add to path in this order:
  C:\mingw32\msys\1.0\bin
  C:\mingw-w64\mingw64\bin
  C:\mingw-w64\mingw32\bin

ensuring that they are after ActiveState perl.

Check to make sure zip and unzip are in your path, make --version
shows at least 3.81, perl --version shows the perl from ActiveState,
and gcc --version is the 64-bit gcc. (zip is not actually needed
unless you are running the tools to create the releases.)

Install suitable Microsoft Visual Studio edition. In early 2016, 2015
community edition with C++ support is fine. It may crash a few times
during installation, but repeating the installation will allow it to
finish, and the resulting software is stable.

To build qpdf, start the msys shell from a command window started from
one of the Visual Studio shell windows.

Image comparison tests are disabled by default, but it is possible to
run them on Windows. To do so, add --enable-test-compare-images from
the configure statements given below and install some additional
third-party dependencies. These may be provided in an environment such
as MSYS or Cygwin or can be downloaded separately for other
environments. You may extract or install the following software into
separate folders each and add the "bin" folder to your "PATH"
environment variable to make executables and DLLs available. If
installers are provided, they might do that already by default.

 * LibJpeg (http://gnuwin32.sourceforge.net/packages/jpeg.htm)

   This archive provides some needed DLLs needed by LibTiff.

 * LibTiff (http://gnuwin32.sourceforge.net/packages/tiff.htm)

   This archive provides some needed binaries and DLLs if you want to
   use the image comparison tests. It depends on some DLLs from
   LibJpeg.

 * GhostScript (http://www.ghostscript.com/download/gsdnld.html)

   GhostScript is needed for image comparison tests. It's important
   that the binary is available as "gs", while its default name is
   "gswin32[c].exe". You can either copy one of the original files,
   use "mklink" to create a hard-/softlink, or provide a custom
   "gs.cmd" wrapper that forwards all arguments to one of the original
   binaries. Using "mklink" with "gswin32c.exe" is probably the best
   choice.


External Libraries
==================

In order to build qpdf, you must have copies of zlib and pcre.  The
easy way to get them is to download them from the qpdf download area.
There are packages called external-libs-bin.zip and
external-libs-src.zip.  If you are building with MSVC 2010 or MINGW,
you can just extract the qpdf-external-libs-bin.zip zip file into the
top-level qpdf source tree.  Note that you need the 2012-06-20 version
(at least) to build qpdf 3.0 or greater since this includes 64-bit
libraries.  It will create a directory called external-libs which
contains header files and precompiled libraries.  Passing
--enable-external-libs to ./configure (which is done automatically if
you follow the instructions below) is sufficient to find them.

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

You can also download qpdf-external-libs-src.zip and follow the
instructions in the README.txt there for how to build external libs.


Building from version control
=============================

If you check out qpdf from version control, you will not have the
files that are generated by autoconf.  If you are not changing these
files, you can grab them from a source distribution or create them
from a system that has autoconf.  To create them from scratch, run
./autogen.sh on a system that has autoconf installed.  Once you have
them, you can run make CLEAN=1 autofiles.zip.  This will create an
autofiles.zip that you can extract on top of a fresh checkout.


Building with MinGW
===================

QPDF is known to build and pass its test suite with mingw-w64 using
the 32-bit and 64-bit compilers from that project (latest version
tested: 5.3.0) and Microsoft Visual C++ 2015, both 32-bit and 64-bit
versions. MSYS plus ActiveState Perl is required to build as well in
order to get make and other related tools. While it is possible that
Cygwin could be used to build native Windows versions of qpdf, this
configuration has not been tested recently.

From your MSYS prompt, run

  ./config-mingw32

or

  ./config-mingw64

and then

  make

Note that ./config-mingw32 and ./configure-mingw64 just run
./configure with specific arguments, so you can look at it, make
adjustments, and manually run configure instead.

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

You can also take a look at make_windows_releases for reference.  This
is how the distributed Windows executables are created.


Building with MSVC 2015
=======================

These instructions would likely work with newer versions of MSVC and
are known to have worked with versions as old as 2008 Express.

You should first set up your environment to be able to run MSVC from
the command line.  There is usually a batch file included with MSVC
that does this.  Make sure that you start a command line environment
configured for whichever of 32-bit or 64-bit output that you intend to
build for.

From that cmd prompt, you can start your msys shell by just running
manually whatever command is associated with your msys shell icon
(such as C:\MinGW\msys\1.0\msys.bat).

Configure as follows:

  ./config-msvc 32

or

  ./config-msvc 64

Note that you must pass the 32/64 option that matches your command
line setup.  The scripts do not presently figure this out.  If you
used the wrong argument, it would probably just build the size you
have in your environment and then install the results in the wrong
place.

Once configured, run

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
version of the compilers.  This has not been retested with the
toolchain versions used to create qpdf 3.0 distributions. (This has
not been revisited since MSVC 2008, but redistrbuting runtime DLLs is
extremely common and should not be a problem.)
