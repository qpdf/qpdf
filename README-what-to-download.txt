To build from source for Linux or other UNIX/UNIX-like systems, it is
generally sufficient to download just the source qpdf-<version>.tar.gz
file.

For Windows, there are several additional files that you might want to
download.

 * qpdf-<version>-bin-mingw32.zip

   If you just want to use the qpdf command line program or use the
   qpdf DLL's C-language interface, you can download this file.  You
   can also download this version if you are using MINGW's gcc 4.6 (or
   a binary compatible version) and want to program using the C++
   interface.

 * qpdf-<version>-bin-mingw64.zip

   A 64-bit version built with mingw.  Use this for 64-bit Windows
   systems.  The 32-bit version will also work on Windows 64-bit.
   Both the 32-bit and the 64-bit version support files over 2 GB in
   size, but you may find it easier to integrate this with your own
   software if you use the 64-bit version.

 * qpdf-<version>-bin-msvc32.zip

   If you want to program using qpdf's C++ interface and you are using
   Microsoft Visual C++ 2010 in 32-bit mode, you can download this
   file.

 * qpdf-<version>-bin-msvc64.zip

   If you want to program using qpdf's C++ interface and you are using
   Microsoft Visual C++ 2010 in 64-bit mode, you can download this
   file.

 * qpdf-external-libs-bin.zip

   If you want to build qpdf for Windows yourself with either MINGW or
   MSVC 2010, you can download this file and extract it inside the
   qpdf source distribution.  Please refer to README-windows.txt in
   the qpdf source distribution for additional details.  Note that you
   need the 2012-06-20 version or later to be able to build qpdf 3.0
   or newer.  The 2009-10-24 version is required for qpdf 2.3.1 or
   older.

 * qpdf-external-libs-src.zip

   If you want to build the external libraries on your own (for
   Windows or anything else), you can download this archive.  In
   addition to including unmodified distributions of pcre and zlib, it
   includes a README file and some scripts to help you build them for
   Windows.


Additionally for Windows, some tests require 3rd party software which
most likely is not available if not provided by an environment like
MSYS, Cygwin or such, but can be downloaded separately as well for an
environment not using both of the former mentioned. Just extract or
install the following software into separate folders each and add the
"bin" folder to your "PATH" env variable to make executables and DLLs
available. If installers are provided, they might do that already by
default.

 * LibJpeg (http://gnuwin32.sourceforge.net/packages/jpeg.htm)

   This archive provides some needed DLLs for LibTiff if you want to
   use the image comparison tests.

 * LibTiff (http://gnuwin32.sourceforge.net/packages/tiff.htm)

   This archive provides some needed binaries and DLLs if you want to
   use the image comparison tests and is only working if DLLs from
   LibJpeg are available as well.

 * GhostScript (http://www.ghostscript.com/download/gsdnld.html)

   GhstScript is needed for image comparison tests and it's important
   that the binary is available as "gs", while its default name is
   "gswin32[c].exe". So you need to provide an additional "gs.exe"
   by copying one of the original files or use "mklink" to create a
   hard-/softlink, or need to provide a custom "gs.cmd" wrapper which
   simply forwards all args to one of the original binarys. Using
   "mkink" with "gswin32c.exe" should be preferred.
