Common Setup
============

You may need to disable antivirus software to run qpdf's test suite. Running Windows Defender on Windows 10 does not interfere with building or running qpdf or its test suite.

Starting with qpdf version 11, qpdf is built with cmake. You can build qpdf with Visual C++ in Release mode with the pre-built external-libraries distribution (described below) without having any additional tools installed. To run the test suite, you need MSYS2.

Here's what I did on my system:

* Download msys2 (64-bit) from msys2.org
* Run the installer.
* Run msys2_shell.cmd by allowing the installer to start it.
* From the prompt:
  * Run `pacman -Syu` and follow the instructions, which may tell you
    to close the window and rerun the command multiple times.
  * Run `pacman -Su` to fully update.
  * `pacman -S make base-devel git zip unzip`
  * `pacman -S mingw-w64-x86_64-toolchain mingw-w64-i686-toolchain`

You need cmake. If you have Visual Studio installed, you can use the cmake that comes with it to build with both MSVC and mingw. You can also a install a native Windows cmake from cmake.org.

To build qpdf with Visual Studio from msys2 so you can run its test suite, start the msys2 mingw32 or mingw64 shell from a command window started from one of the Visual Studio shell windows. You must have it inherit the path. For example:

* Start x64 native tools command prompt from msvc
* `set MSYS2_PATH_TYPE=inherit`
* `C:\msys64\mingw64`

For the test suite to work properly, your build directory must be on the same drive as your source directory. This is because there are parts of the test environment that create relative paths from one to the other. You can use a cross-drive symlink if needed.

Image comparison tests are disabled by default, but it is possible to run them on Windows. To do so, set the `QPDF_TEST_COMPARE_IMAGES` environment variable to `1` and install the additional third-party dependencies described in the manual. These may be provided in an environment such as MSYS or Cygwin or can be downloaded separately for other environments. You may extract or install the following software into separate folders each and add the `bin` folder to your `PATH` environment variable to make executables and DLLs available. If installers are provided, they might do that already by default.

* [LibJpeg](http://gnuwin32.sourceforge.net/packages/jpeg.htm): This archive provides some needed DLLs needed by LibTiff.
* [LibTiff](http://gnuwin32.sourceforge.net/packages/tiff.htm): This archive provides some needed binaries and DLLs if you want to use the image comparison tests. It depends on some DLLs from LibJpeg.
* [GhostScript](http://www.ghostscript.com/download/gsdnld.html): GhostScript is needed for image comparison tests. It's important that the binary is available as `gs`, while its default name is `gswin32[c].exe`. You can either copy one of the original files, use `mklink` to create a hard/softlink, or provide a custom `gs.cmd` wrapper that forwards all arguments to one of the original binaries. Using `mklink` with `gswin32c.exe` is probably the best choice.

# External Libraries

In order to build qpdf, you must have a copy of `zlib` and the `jpeg` library. You can download [prebuilt static external libraries from the qpdf/external-libs github repository](https://github.com/qpdf/external-libs/releases). These include `zlib`, `jpeg`, and `openssl` libraries. For MSVC, you must use a non-debugging build configuration. There are files called `external-libs-bin.zip` and `external-libs-src.zip`. If you are building with a recent MSVC or MINGW with MSYS2, you can just extract the `qpdf-external-libs-bin.zip` zip file into the top-level qpdf source tree. The qpdf build detects the presence of the `external-libs` directory automatically. You don't need to set any cmake options.

You can also obtain `zlib` and `jpeg` directly on your own and install them. Just make sure cmake can find them.

External libraries are built using GitHub Actions from the [qpdf/external-libs](https://github.com/qpdf/external-libs) repository.

# Running tools from the build area

You can run qpdf's tests without modifying your PATH. If you want to manually run executables from the build tree on Windows, you need to add the `libqpdf` build directory to your path so it can find the qpdf DLL. This would typically be either `libqpdf` or `libqpdf/<CONFIG>` (e.g., `libqpdf/Release`) depending on which cmake generator you are using. Alternatively, you can disable `BUILD_SHARED_LIBS` for your Windows build. This will cause the executables to use the static qpdf library and not build a qpdf DLL at all.

# Runtime DLLs

Both build methods create executables and DLLs that are dependent on the compiler's runtime DLLs.  When you run `cmake --install` or `cpack`, the installation process will automatically detect the DLLs and copy them into the installation bin directory. For mingw, a perl script is used. For MSVC, `cmake`'s `InstallRequiredSystemLibraries` module is adequate.
