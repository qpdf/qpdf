To build from source for Linux or other UNIX/UNIX-like systems, it is
generally sufficient to download just the source qpdf-<version>.tar.gz
file.

For Windows, there are several additional files that you might want to
download.

 * qpdf-<version>-bin-mingw.zip

   If you just want to use the qpdf commandline program or use the
   qpdf DLL's C-language interface, you can download this file.  You
   can also download this version if you are using MINGW's gcc 4.4 and
   want to program using the C++ interface.

 * qpdf-<version>-bin-msvc.zip

   If you want to program using qpdf's C++ interface and you are using
   Microsoft Visual C++ .NET 2008 (VC9), you can download this file.

 * qpdf-external-libs-bin.zip

   If you want to build qpdf for Windows yourself with either MINGW's
   gcc 4.4 or VC9, you can download this file and extract it inside
   the qpdf source distribution.  Please refer to README-windows.txt
   in the qpdf source distribution for additional details.

 * qpdf-external-libs-src.zip

   If you want to build the external libraries on your own (for
   Windows or anything else), you can download this archive.  In
   addition to including unmodified distributions of pcre and zlib, it
   includes a README file and some scripts to help you build them for
   Windows.
