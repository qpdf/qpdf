To build from source for Linux or other UNIX/UNIX-like systems, it is generally sufficient to download just the source `qpdf-<version>.tar.gz` file.

Windows Binaries

You can download Windows binaries that are statically linked with qpdf's external dependencies and use the OpenSSL crypto provider. There are several options:

* For Windows installers, you can download the `.exe` file for a traditional installer that allows relocation, or you can download the `.zip` file which you can unzip to any location. Note that the `msvc` executables perform slightly better than the `mingw` executables.

* `qpdf-<version>-bin-msvc64.exe` - Use this for 64-bit Windows systems. This is the highest performance Windows release. It is usually the best choice for using qpdf from the command line. It is also the right choice if you are building non-Debug code for 64-bit systems using a recent version of Microsoft Visual C++.

* `qpdf-<version>-bin-mingw64.exe` - This is a 64-bit version built with mingw. Use this for 64-bit Windows systems if you want development libraries that work with the 64-bit version of mingw. If you are dynamically loading qpdf from the DLL, this version has fewer DLLs than the msvc version and does not require a Visual C++ runtime DLL. Unlike with the MSVC releases, it is possible to link a debugging build with mingw against non-debugging libraries built with mingw.

* `qpdf-<version>-bin-msvc32.exe` - This is a 32-bit version built with MSVC. Use this if you need to run qpdf on a 32-bit system or if you are building 32-bit executables in non-Debug mode with Microsoft Visual C++. The 32-bit executables will work on 64-bit systems as well and are capable of working with files larger than 2 GB.

* `qpdf-<version>-bin-mingw32.exe` - This is a 32-bit version built with mingw. It will work on 32-bit or 64-bit systems and can handle files larger than 2 GB.

Linux Binaries

Virtually all Linux distributions include packages for qpdf. There is also a PPA for Ubuntu at https://launchpad.net/~qpdf/+archive/ubuntu/qpdf that includes the latest version of qpdf for recent versions of Ubuntu. However, there are some downloads available for Linux as well.

* `qpdf-<version>-x86_64.AppImage` - If you'd like to run the latest version of qpdf as an [AppImage](https://appimage.org/), you can download this. This is a self-contained executable that you make symlink `qpdf` to and run on most reasonably recent Linux distributions. See README-appimage.md in the qpdf source distribution for additional details, or run the AppImage with the `--ai-usage` argument to get help specific to the AppImage.

* `qpdf-<version>-bin-linux-x86_64.zip` - This is not intended to be an end-user distribution. It is a (nearly) stand-alone Linux binary, built using an Ubuntu LTS release. It contains the qpdf executables and shared libraries as well as dependent shared libraries that would not typically be present on a minimal system. This can be used to include qpdf in a minimal environment such as a docker container. It is also known to work as a layer in AWS Lambda and was initially created for that purpose. The executables have their runpath set to looks for the qpdf library in `../lib` relative to the location of the executables, which makes this distribution relocatable.

Windows Build Support

If you are building on Windows and want to use pre-built external static libraries, you should obtain current versions from https://github.com/qpdf/external-libs/releases. The `external-libs` directory contains older versions that will not work with qpdf versions >= 10.0.2. Please see README-windows.md in the qpdf source distribution. The libraries from this distribution will not work with a Debug build with MSVC.

Documentation

* `qpdf-<version>-doc.zip` - This is a downloadable version of the QPDF manual. An online version is hosted at https://qpdf.readthedocs.io.
