# Tests for installed packages

The files in this directory are called by autopkgtest in the debian package but can be used by any packager to verify installed packages. Each test-* script should be run from the top of the source tree and takes an empty directory as its single argument. The test passes if the script exits with a zero exit status. Note that these tests write to stderr because they use set -x in the shell.

On a GNU/Linux system, you can run `./pkg-test/run-all` from the top-level directory to run all the tests. Note that you have to specify an altrenative install prefix rather than using DESTDIR since, otherwise, pkg-config won't find the packages. For example:

```
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/tmp/inst
cmake --build build -j$(nproc)
cmake --install build
env PKG_CONFIG_PATH=/tmp/inst/lib/pkgconfig \
    LD_LIBRARY_PATH=/tmp/inst/lib \
    CMAKE_PREFIX_PATH=/tmp/inst \
    PATH=/tmp/inst/bin:$PATH \
   ./pkg-test/run-all
```
