Quick Start with JetBrains CLion
================================

The following *should* work but has not been tested on a completely clean system with CLion. It may
work with other "batteries-included" IDEs as well.

* Download the most recent vcpkg zip file from [the qpdf vcpkg
  cache](https://github.com/qpdf/qpdf/releases/tag/vcpkg-cache-v1), and unzip it into an otherwise
  clean source tree.
* Using the default toolchain, you can create a cmake build of type *other than Debug*. A `Debug`
  build will not work with the external libraries since debug versions are not redistributable. If
  you want a Debug build, you'll have to build the external libraries yourself, which you can do
  with vcpkg. See `vcpkg-setup-win` as a hint. It's possible that you can just use vcpkg in its
  usual way, though this is not regularly tested and may not work correctly when building from msys.
* If you have MSVC, you can enable one of the `msvc` presets that you should see when you edit CMake
  configurations.

In any of these, it should work to build and run the executables from the IDE. Note that, if you
start a terminal from CLion and mingw is not in your path, the executables built my mingw won't run.
If mingw is in your path, it should work. You can also start a mingw64 shell. The executables should
work from there. This works because the cmake configuration copies the qpdf DLL into the bin
directory. If you want the other executables to work, you should add the `libqpdf` directory of your
build directory to your path or disable shared libraries. For more details, consult the qpdf manual
and the rest of this file.

Additional dependencies are required for running tests. For the foreseeable future, that requires
msys2, though this may eventually not be the case.

Common Setup
============

You may need to disable antivirus software to run qpdf's test suite. Running Windows Defender on Windows 10 does not interfere with building or running qpdf or its test suite.

Starting with qpdf version 11, qpdf is built with cmake. You can build qpdf with Visual C++ in Release mode with the pre-built external-libraries distribution (described below) without having any additional tools installed. You can also build with Visual C++ using JetBrains CLion with the external libraries distribution as long as you pass `-DBUILD_SHARED_LIBS=OFF`. It also works to use the build type `RelWithDebInfo`, in which case you can run qpdf in the debugger. To run the test suite, you need MSYS2.

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

You need cmake. If you have Visual Studio or JetBrains CLion installed, you can use the cmake that comes with those tools to build with both MSVC and mingw. You can also a install a native Windows cmake from cmake.org.

To build qpdf with Visual Studio from msys2 so you can run its test suite, start the msys2 mingw32 or mingw64 shell from a command window started from one of the Visual Studio shell windows. You must have it inherit the path. For example:

* Start x64 native tools command prompt from msvc
* `set MSYS2_PATH_TYPE=inherit`
* `C:\msys64\mingw64`

For the test suite to work properly, your build directory must be on the same drive as your source directory. This is because there are parts of the test environment that create relative paths from one to the other. You can use a cross-drive symlink if needed.

Image comparison tests are disabled by default, but it is possible to run them on Windows. To do so, set the `QPDF_TEST_COMPARE_IMAGES` environment variable to `1` and install the additional third-party dependencies described in the manual. These may be provided in an environment such as MSYS or Cygwin or can be downloaded separately for other environments. You may extract or install the following software into separate folders each and add the `bin` folder to your `PATH` environment variable to make executables and DLLs available. If installers are provided, they might do that already by default.

* [LibJpeg](http://gnuwin32.sourceforge.net/packages/jpeg.htm): This archive provides some needed DLLs needed by LibTiff.
* [LibTiff](http://gnuwin32.sourceforge.net/packages/tiff.htm): This archive provides some needed binaries and DLLs if you want to use the image comparison tests. It depends on some DLLs from LibJpeg.
* [GhostScript](http://www.ghostscript.com/download/gsdnld.html): GhostScript is needed for image comparison tests. It's important that the binary is available as `gs`, while its default name is `gswin32[c].exe`. You can either copy one of the original files, use `mklink` to create a hard/softlink, or provide a custom `gs.cmd` wrapper that forwards all arguments to one of the original binaries. Using `mklink` with `gswin32c.exe` is probably the best choice.

# External Libraries/vcpkg

In order to build qpdf, you must have a copy of `zlib` and the `jpeg` library. You can download [prebuilt static external libraries from the qpdf vcpkg cache](https://github.com/qpdf/qpdf/releases/tag/vcpkg-cache-v1). Download the most recent available artifact and unzip it. This includes `zlib`, `jpeg`, and `openssl` libraries. For MSVC, you must use a non-debugging build configuration. If you are building with a recent MSVC or MINGW with MSYS2, you can just unzip the zip file into the top-level qpdf source tree. The qpdf build detects the presence of the `vcpkg/installed` directory automatically. You don't need to set any cmake options.

If you prefer to use qpdf's automatic integration with vcpkg but download and build the libraries yourself, you can use the `./vcpkg-setup-win` script, which takes `msvc` or `mingw` as a parameter just like `cmake-win` does.

The triplet is selected from the compiler in your current environment, so for an ARM64 build, run `./vcpkg-setup-win msvc` from an ARM64 Visual Studio shell (`vcvarsarm64.bat`, or `vcvarsamd64_arm64.bat` to cross compile from x64).

You can also obtain `zlib` and `jpeg` directly on your own and install them. Just make sure cmake can find them. It's possible that a system version of `vcpkg` may work, though this is not regularly tested and was known to break msys-based builds.

External libraries are built using GitHub Actions using the vcpkg.yml workflow. In addition to the special `vcpkg-cache-v1` release, you can download the specific `vcpkg.zip` that was used for any given build as the `distribution-vcpkg` artifact from the action's status page.

# Running tools from the build area

You can run qpdf's tests without modifying your PATH. If you want to manually run executables from the build tree on Windows, you need to add the `libqpdf` build directory to your path so it can find the qpdf DLL. This would typically be either `libqpdf` or `libqpdf/<CONFIG>` (e.g., `libqpdf/Release`) depending on which cmake generator you are using. Alternatively, you can disable `BUILD_SHARED_LIBS` for your Windows build. This will cause the executables to use the static qpdf library and not build a qpdf DLL at all.

Note that if you cross-compile for ARM64 on an x64 host, you will not be able to run tests as x64 hosts cannot run ARM64 executables.

# Runtime DLLs

Both build methods create executables and DLLs that are dependent on the compiler's runtime DLLs.  When you run `cmake --install` or `cpack`, the installation process will automatically detect the DLLs and copy them into the installation bin directory. For mingw, a perl script is used. For MSVC, `cmake`'s `InstallRequiredSystemLibraries` module is adequate.
