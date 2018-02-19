To build from source for Linux or other UNIX/UNIX-like systems, it is generally sufficient to download just the source `qpdf-<version>.tar.gz` file.

Virtually all Linux distributions include packages for qpdf. If you'd like to run the latest version of qpdf on an AppImage, you can download `qpdf-<version>-x86_64.AppImage`.

For Windows, there are several additional files that you might want to download.

* `qpdf-<version>-bin-mingw32.zip`

   If you just want to use the qpdf command line program or use the qpdf DLL's C-language interface, you can download this file.  You can also download this version if you are using MINGW's gcc and want to program using the C++ interface.

* `qpdf-<version>-bin-mingw64.zip`

   A 64-bit version built with mingw.  Use this for 64-bit Windows systems.  The 32-bit version will also work on Windows 64-bit. Both the 32-bit and the 64-bit version support files over 2 GB in size, but you may find it easier to integrate this with your own software if you use the 64-bit version.

* `qpdf-<version>-bin-msvc32.zip`

  If you want to program using qpdf's C++ interface and you are using Microsoft Visual C++ 2015 in 32-bit mode, you can download this file.

* `qpdf-<version>-bin-msvc64.zip`

  If you want to program using qpdf's C++ interface and you are using Microsoft Visual C++ 2015 in 64-bit mode, you can download this file.

* `qpdf-external-libs-bin.zip`

  If you want to build qpdf for Windows yourself with either MINGW or MSVC 2015, you can download this file and extract it inside the qpdf source distribution.  Please refer to README-windows.md in the qpdf source distribution for additional details.  Note that you need the 2017-08-21 version or later to be able to build qpdf 7.0 or newer. Generally grab the `external-libs` distribution that was the latest version at the time of the release of whichever version of qpdf you are building.

* `qpdf-external-libs-src.zip`

  If you want to build the external libraries on your own (for Windows or anything else), you can download this archive. In addition to including an unmodified distribution `zlib` and the `jpeg` library, it includes a `README` file and some scripts to help you build it for Windows. You will also have to provide those.

If you want to build on Windows, please see also README-windows.md in the qpdf source distribution.

