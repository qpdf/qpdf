To build from source for Linux or other UNIX/UNIX-like systems, it is generally sufficient to download just the source `qpdf-<version>.tar.gz` file.

Windows Binaries

You can download Windows binaries that are statically linked with qpdf's external dependencies and use the OpenSSL crypto provider. There are several options:

* `qpdf-<version>-bin-mingw32.zip` - 32-bit executables that should work on basically any Windows system, including 64-bit systems. The 32-bit executables are capable of handling files larger than 2 GB. If you just want to use the qpdf command line program or use the qpdf DLL's C-language interface, you can download this file.  You can also download this version if you are using MINGW's gcc and want to program using the C++ interface.

* `qpdf-<version>-bin-mingw64.zip` - A 64-bit version built with mingw.  Use this for 64-bit Windows systems.  The 32-bit version will also work on Windows 64-bit. Both the 32-bit and the 64-bit version support files over 2 GB in size, but you may find it easier to integrate this with your own software if you use the 64-bit version.

* `qpdf-<version>-bin-msvc32.zip` - If you want to program using qpdf's C++ interface and you are using a recent version of Microsoft Visual C++ in 32-bit mode, you can download this file.

* `qpdf-<version>-bin-msvc64.zip` - If you want to program using qpdf's C++ interface and you are using a recent version of Microsoft Visual C++ in 64-bit mode, you can download this file.

Linux Binaries

Virtually all Linux distributions include packages for qpdf. There is also a PPA for Ubuntu at https://launchpad.net/~qpdf/+archive/ubuntu/qpdf that includes the latest version of qpdf for recent versions of Ubuntu. However, there are some downloads available for Linux as well.

* `qpdf-<version>-x86_64.AppImage` - If you'd like to run the latest version of qpdf as an [AppImage](https://appimage.org/), you can download this. This is a self-contained executable that you make symlink `qpdf` to and run on most reasonably recent Linux distributions. See README-appimage.md in the qpdf source distribution for additional details, or run the AppImage with the `--ai-usage` argument to get help specific to the AppImage.

* `qpdf-<version>-bin-linux-x86_64.zip` - This is a (nearly) stand-alone Linux binary, built using an Ubuntu LTS release. It contains the qpdf executables and shared libraries as well as dependent shared libraries that would not typically be present on a minimal system. This can be used to include qpdf in a minimal environment such as a docker container. It is also known to work as a layer in AWS Lambda and was initially created for that purpose.

Windows Build Support

If you are building on Windows and want to use pre-built external static libraries, you should obtain current versions from https://github.com/qpdf/external-libs/releases. The `external-libs` directory contains older versions that will not work with qpdf versions >= 10.0.2. Please see README-windows.md in the qpdf source distribution.
