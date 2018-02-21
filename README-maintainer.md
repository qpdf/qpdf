# Release Reminders

* For debugging:
  ```
  ./configure CFLAGS="-g" CXXFLAGS="-g" --enable-werror --disable-shared
  ```
* When making a release, always remember to run large file tests and image comparison tests (`--enable-test-compare-images` `--with-large-file-test-path=/path`). For Windows, use a Windows style path, not an MSYS path for large files. For a major release, consider running a spelling checker over the source code to catch errors in variable names, strings, and comments. Use `ispell -p ispell-words`.
* Run tests with sanitize address enabled:
  ```
  ./configure CFLAGS="-fsanitize=address -g" \
     CXXFLAGS="-fsanitize=address -g" \
     LDFLAGS="-fsanitize=address" \
     --enable-werror --disable-shared
  ```
  The test suite should run clean with this. This seems to be more reliable than valgrind.
* Test with clang. Pass `CC=clang CXX=clang++` to `./configure`.
* Check all open issues in the sourceforge trackers and on github.
* If any interfaces were added or changed, check C API to see whether changes are appropriate there as well.  If necessary, review the casting policy in the manual, and ensure that integer types are properly handled.
* Avoid atoi. Use QUtil::string_to_int instead. It does overflow/underflow checking.
* Remember to avoid using `operator[]` with `std::string` or `std::vector`. Instead, use `at()`. See README-hardening.md for details.
* Increment shared library version information as needed (`LT_*` in `configure.ac`)
* Test for binary compatibility. The easiest way to do this is to check out the last release, run the test suite, check out the new release, run `./autogen.mk; ./configure --enable-werror; make build_libqpdf`, check out the old release, and run `make check NO_REBUILD=1`.
* Update release notes in manual. Look at diffs and ChangeLog.
* Add a release entry to ChangeLog.
* Make sure version numbers are consistent in the following locations:
  * configure.ac
  * libqpdf/QPDF.cc
  * manual/qpdf-manual.xml
  `make_dist` verifies this consistency.
* Generate a signed AppImage using the docker image in appimage. Arguments to the docker container are arguments to git clone. The build should be made off the exact commit that will be officially tagged as the release but built prior to tagging the release.
Example:
  ```
  cd appimage
  docker build -t qpdfbuild .
  rm -rf /tmp/build
  mkdir -p /tmp/build
  cp -rLp ~/.gnupg/. /tmp/build/.gnupg
  docker run --privileged -ti --rm -v /tmp/build:/tmp/build qpdfbuild https://github.com/jberkenbilt/qpdf -b work
  ```
  Rename the AppImage in appimage/build to qpdf-<version>-x86_64.AppImage and include it in the set of files to be released.
* Update release date in `manual/qpdf-manual.xml`.  Remember to ensure that the entities at the top of the document are consistent with the release notes for both version and release date.
* Check `TODO` file to make sure all planned items for the release are done or retargeted.
* Each year, update copyright notices. Just do a case-insensitive search for copyright. Don't forget copyright in manual. Also update debian copyright in debian package. Last updated: 2018.
* To construct a source distribution from a pristine checkout, `make_dist` does the following:
  ```
  ./autogen.sh
  ./configure --enable-doc-maintenance --enable-werror
  make build_manual
  make distclean
  ```
* To create a source release, do an export from the version control system to a directory called qpdf-version.  For example, from this directory:
  ```
  rm -rf /tmp/qpdf-x.y.z
  git archive --prefix=qpdf-x.y.z/ HEAD . | (cd /tmp; tar xf -)
  ```
  From the parent of that directory, run `qpdf-x.y.z/make_dist`.  Remember to have fop in your path.  For internally testing releases, you can run make_dist with the `--no-tests` option.
* To create a source release of external libs, do an export from the version control system into a directory called `qpdf-external-libs` and just make a zip file of the result called `qpdf-external-libs-src.zip`.  See the README.txt file there for information on creating binary external libs releases. Run this from the external-libs repository:
  ```
  git archive --prefix=external-libs/ HEAD . | (cd /tmp; tar xf -)
  cd /tmp
  zip -r qpdf-external-libs-src.zip external-libs
  ```
* To create Windows binary releases, extract the qpdf source distribution in Windows (MSYS2 + MSVC).  From the extracted directory, extract the binary distribution of the external libraries.  Run ./make_windows_releases simultaneously in 32-bit and 64-windows from there.
* Before releasing, rebuild and test debian package.
* Remember to copy `README-what-to-download.md` separately onto the download area.
* Remember to update the web page including putting new documentation in the `files` subdirectory of the website on sourceforge.net.
* Create a tag in the version control system, and make backups of the actual releases.  With git, use git `tag -s` to create a signed tag:
  ```
  git tag -s release-qpdf-$version HEAD -m"qpdf $version"
  ```
* When releasing on sourceforge, `external-libs` distributions go in `external-libs/yyyymmdd`, and qpdf distributions go in `qpdf/vvv`. Make the source package the default for all but Windows, and make the 32-bit mingw build the default for Windows.
* Prepare and archive all release files. Files should be the source package, Windows binaries, AppImage, checksum files, and signature files.
* Create a github release after pushing the tag. `gcurl` is an alias that includes the auth token.
  ```
  url=$(gcurl -s -XPOST https://api.github.com/repos/qpdf/qpdf/releases -d'{"tag_name": "release-qpdf-$version", "name": "qpdf $version", "draft": true, "body": "test *body* text"}' | jq -r '.url')
  ```
* Upload files to sourceforge and github. Publish the github release. Release notes can be added to the github release manually. Publish a news item manually on sourceforge. Email the qpdf-announce list.

# General Build Stuff

QPDF uses autoconf and libtool but does not use automake.  The only files distributed with the qpdf source distribution that are not controlled are `configure`, `libqpdf/qpdf/qpdf-config.h.in`, `aclocal.m4`, and some documentation.  See above for the steps required to prepare a source distribution.

A small handful of additional files have been taken from autotools programs.  These should probably be updated from time to time.
* `config.guess`, `config.sub`, `ltmain.sh`, and the `m4` directory: these were created by running `libtoolize -c`.  To update, run `libtoolize -f -c` or remove the files and rerun `libtoolize`.
* Other files copied as indicated:
  ```
  cp /usr/share/automake-1.11/install-sh .
  cp /usr/share/automake-1.11/mkinstalldirs .
  ```

The entire contents of the `m4` directory came from `libtool.m4`.  If we had some additional local parts, we could also add those to the `m4` directory.  In order for this to work, it is necessary to run `aclocal -I m4` before running `autoheader` and `autoconf`. The `autogen.sh` script handles this.

If building or editing documentation, configure with `--enable-doc-maintenance`.  This will ensure that all tools or files required to validate and build documentation are available.

If you want to run `make maintainer-clean`, `make distclean`, or `make autofiles.zip` and you haven't run `./configure`, you can pass `CLEAN=1` to make on the command line to prevent it from complaining about configure not having been run.

If you want to run checks without rerunning the build, pass `NO_REBUILD=1` to make. This can be useful for special testing scenarios such as validation of memory fixes or binary compatibility.

# Local Windows Testing Procedure

This is what I do for routine testing on Windows.

From Linux, run `./autogen.sh` and `make autofiles.zip CLEAN=1`.

From Windows, git clone from my Linux clone, unzip `external-libs`, and unzip `autofiles.zip`.

Look at `make_windows_releases`. Set up path the same way and run whichever `./config-*` is appropriate for whichever compiler I need to test with. Start one of the Visual Studio native compiler shells, and from there, run one of the msys shells. The Visual Studio step is not necessary if just building with mingw.
