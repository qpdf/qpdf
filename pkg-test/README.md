# Tests for installed packages

The files in this directory are called by autopkgtest in the debian package but can be used by any packager to verify installed packages. Each test-* script should be run from the top of the source tree and takes an empty directory as its single argument. The test passes if the script exits with a zero exit status. Note that these tests write to stderr because they use set -x in the shell.

On a GNU/Linux system, you can run `./pkg-test/run-all` from the top-level directory to run all the tests. For example:

```
cmake -S . -B build
cmake --build build -j$(nproc)
DESTDIR=/tmp/inst cmake --install build
env PKG_CONFIG_PATH=/tmp/inst/usr/local/lib/pkgconfig \
    CMAKE_PREFIX_PATH=/tmp/inst/usr/local \
   ./pkg-test/run-all
```
