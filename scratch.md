
Windows:
* Install: `DESTDIR=... cmake --install . [--config RelWithDebInfo]`
  * Need `--config` for msvc
* Not necessary to update path for running tests on Windows though still necessary to update path for running executables from the build tree.
* Document unzipping external-libs -- no config option needed; doesn't work with Debug
* cpack -G NSIS -C Release
* cpack -G ZIP -C Release

Incorporate basic knowledge into ~/Q/tasks/reference/

Windows:
```
mkdir build
cd build
../cmake-win {mingw|msvc}
```

Other:
```
mkdir build
cmake -DMAINTAINER_MODE=1 -DBUILD_STATIC_LIBS=0 -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . -j [-v]
ctest -V [-R testname-regex]
```

Windows msvc:
```
mkdir build
cmake -DEXTERNAL_LIBS=1 -DQTEST_COLOR=1 ..
cmake --build . -j --config RelWithDebInfo
ctest -V -C Release
```

Windows mingw:
```
mkdir build
cmake -G "MinGW Makefiles" -DEXTERNAL_LIBS=1 -DQTEST_COLOR=1 ..
cmake --build . -j --config RelWithDebInfo
ctest -V -C Release
```

* Configurations:
  - Debug
  - Release
  - RelWithDebInfo
  - MinSizeRel
* For multi-configuration builds like MSVC, specify --config whatever at runtime.
* For single-configuration builds, specify -DCMAKE_BUILD_TYPE=whatever at configuration time.
* RelWithDebInfo matches what we have by default
* get_property(var GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
* ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}

Use ctest --output-on-failure or --verbose combined with -DSHOW_FAILED_TEST_OUTPUT=1 in CI (--verbose is better)

Remaining work:
* msvc LIBCMT warning...why, and can I use /NODEFAULTLIB:library?
* Go through README*.md and the manual for build and test information
* Document tests that were run manually to verify the build including handling non-standard install locations of third-party libraries
  * Break libqpdf.a, libqpdf.so*, qpdf, fix-qdf, zlib-flate, and DLL.h in the installed qpdf and make sure build and test work
  * build static only, build shared only
  * installation of separate components
* Remove old build files
* Document direct use of QPDF_TEST_COMPARE_IMAGES and QPDF_LARGE_FILE_TEST_PATH rather than configure options. Tests files > 4GB. Need 11 GB free.
* Mention reversing sense of QPDF_TEST_COMPARE_IMAGES (used to be QPDF_SKIP_TEST_COMPARE_IMAGES).
* For packaging documentation:
  * `-DBUILD_DOC_DIST=1` to create manual/doc-dist
  * `-DINSTALL_MANUAL=1` to install manual/doc-dist
  * Build and install are independent
* include **/CMakeLists.txt in spell check
* Build documentation: manual/installation.rst. Mention relevant options and reference CMakeLists.txt's comment.
* README-windows-install.txt is no longer used

Notes:
* `cmake --build . -U pattern` -- remove from cache
* Windows: build directory must be on the same drive as source; cross-drive symlink is fine
* Document that building fuzz requires perl
* Document QTEST_COLOR
* Large file tests fail with linux32 before and after cmake
