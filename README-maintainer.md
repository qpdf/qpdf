# Maintainer Notes

This file contains notes of interest only to maintainers of qpdf.

For information also of interest to contributors and other developers that want to modify qpdf 
please see [README-developer.md](./README-developer.md). This information used to be part 
of this file, but was split off to make it easier for new developers to find relevant information.


## Contents

* [VERSIONS](#versions)
* [GOOGLE OSS-FUZZ](#google-oss-fuzz)
* [RELEASE PREPARATION](#release-preparation)
* [CREATING A RELEASE](#creating-a-release)


## VERSIONS

* The version number on the main branch is whatever the version would
  be if the top of the branch were released. If the most recent
  release is version a.b.c, then

  * If there are any ABI-breaking changes since the last release,
    main's version is a+1.0.0
  * Else if there is any new public API, main's version is a.b+1.0
  * Else if there are any code changes, main's version is a.b.c+1

* Whenever we bump the version number, bump shared library versions as
  well.

* Every released major/minor version has an a.b branch which is used
  primarily for documentation but could potentially be used to create
  a new patch release after main has moved on. We don't do that as a
  rule, but there's no reason we couldn't do it if main had unreleased
  ABI/API changes that were still in flux and an important bug fix was
  needed on the most recent release. In that case, a release can be
  cut from a release branch and then either main can be rebased from
  there or the changes can be merged back, depending on the amount of
  drift.


## GOOGLE OSS-FUZZ

* See ../misc/fuzz (not in repo) for unfixed, downloaded fuzz test cases

* qpdf project: https://github.com/google/oss-fuzz/tree/master/projects/qpdf

* Adding new test cases: download the file from oss-fuzz and drop it
  in fuzz/qpdf_extra/issue-number.fuzz. When ready to include it, add
  to fuzz/CMakeLists.txt. Until ready to use, the file can be stored
  anywhere, and the absolute path can be passed to the reproduction
  code as described below.

* To test locally, see https://github.com/google/oss-fuzz/tree/master/docs/,
  especially new_project_guide.md. Summary:

  Clone the oss-fuzz project. From the root directory of the repository:

  ```
  python3 infra/helper.py build_image --pull qpdf
  python3 infra/helper.py build_fuzzers [ --sanitizer memory|undefined|address ] qpdf [path-to-qpdf-source]
  python3 infra/helper.py check_build qpdf
  python3 infra/helper.py build_fuzzers --sanitizer coverage qpdf
  python3 infra/helper.py coverage qpdf
  ```

  To reproduce a test case, build with the correct sanitizer, then run

  python3 infra/helper.py reproduce qpdf <specific-fuzzer> testcase

  where fuzzer is the fuzzer used in the crash.

  The fuzzer is in build/out/qpdf. It can be run with a directory as
  an argument to run against files in a directory. You can use

  qpdf_fuzzer -merge=1 cur new >& /dev/null&

  to add any files from new into cur if they increase coverage. You
  need to do this with the coverage build (the one with
  --sanitizer coverage)

* General documentation: http://libfuzzer.info

* Build status: https://oss-fuzz-build-logs.storage.googleapis.com/index.html

* Project status: https://oss-fuzz.com/ (private -- log in with Google account)

* Latest corpus:
  gs://qpdf-backup.clusterfuzz-external.appspot.com/corpus/libFuzzer/qpdf_fuzzer/latest.zip


## RELEASE PREPARATION

* Each year, update copyright notices. This will find all relevant
  places (assuming current copyright is from last year):

  ```
  git --no-pager grep -i -n -P "copyright.*$(expr $(date +%Y) - 1).*berkenbilt"
  ```

  Also update the copyright in these places:
  * debian package -- search for copyright.*berkenbilt in debian/copyright
  * qtest-driver, TestDriver.pm in qtest source

  Copyright last updated: 2026.

* Take a look at "External Libraries" in TODO to see if we need to
  make any changes. There is still some automation work left to do, so
  handling external-libs releases is still manual. See also
  README-maintainer in external-libs.

* Check for open fuzz crashes at https://oss-fuzz.com

* Check all open issues and pull requests in github and the
  sourceforge trackers. Don't forget pull
  requests. Note: If the location for reporting issues changes, do a
  careful check of documentation and code to make sure any comments
  that include the issue creation URL are updated.

* Check `TODO` file to make sure all planned items for the release are
  done or retargeted.

* Check work `qpdf` project for private issues

* Make sure the code is formatted.

  ```
  ./format-code
  ```

* Run a spelling checker over the source code to catch errors in
  variable names, strings, and comments.

  ```
  ./spell-check
  ```

  This uses cspell. Install with `npm install -g cspell`. The output
  of cspell is suitable for use with `M-x grep` in emacs. Add
  exceptions to cSpell.json.

* If needed, run large file and image comparison tests by setting
  these environment variables:

  ```
  QPDF_LARGE_FILE_TEST_PATH=/full/path
  QPDF_TEST_COMPARE_IMAGES=1
  ```

  For Windows, use a Windows style path, not an MSYS path for large files.

* If any interfaces were added or changed, check C API to see whether
  changes are appropriate there as well. If necessary, review the
  casting policy in the manual, and ensure that integer types are
  properly handled with QIntC or the appropriate cast. Remember to
  ensure that any exceptions thrown by the library are caught and
  converted. See `trap_errors` in qpdf-c.cc.

* Double check versions and shared library details. They should
  already be up to date in the code.

  * Make sure version numbers are consistent in the following locations:
    * CMakeLists.txt
    * include/qpdf/DLL.h

  `make_dist` verifies this consistency, and CI fails if they are
  inconsistent.

* Review version control history. Update release date in
  `manual/release-notes.rst`. Change "not yet released" to an actual
  date for the release.

* Commit changes with title "Prepare x.y.z release"

* Performance test is included with binary compatibility steps. Even
  if releasing a new major release and not doing binary compatibility
  testing, do performance testing.

* Test for performance and binary compatibility:

  ```
  ./abi-perf-test v<old> @
  ```

  * Prefix with `SKIP_PERF=1` to skip performance test.
  * Prefix with `SKIP_TESTS=1` to skip test suite run.

  See "ABI checks" for details about the process.
  End state:
  * /tmp/check-abi/perf contains the performance comparison
  * /tmp/check-abi/old contains old sizes and library
  * /tmp/check-abi/new contains new sizes and library
  * run check_abi manually to compare

## CREATING A RELEASE

* Releases are dual signed with GPG and also with
  [cosign](https://docs.sigstore.dev/quickstart/quickstart-cosign/)
  using your GitHub identity. If you are creating a release, please
  make sure your correct identity is listed in README.md under
  "Verifying Distributions."

* Push to main. This will create an artifact called distribution
  which will contain all the distribution files. Download these,
  verify the checksums from the job output, rename to remove -ci from
  the names, and extract to an empty directory, which we will call the
  "release directory."

* Set the shell variable `version`, which is used in several steps.

```sh
version=x.y.z
gpg --detach-sign --armor qpdf-$version.tar.gz
```

* From the release directory, sign the releases. You need
  [cosign](https://docs.sigstore.dev/cosign/system_config/installation/).
  When prompted, use your GitHub identity to sign the release.

```sh
\rm -f *.sha256
files=(*)
sha256sum ${files[*]} >| qpdf-$version.sha256
gpg --clearsign --armor qpdf-$version.sha256
mv qpdf-$version.sha256.asc qpdf-$version.sha256
cosign sign-blob qpdf-$version.sha256 --bundle qpdf-$version.sha256.sigstore
chmod 444 *
chmod 555 *.AppImage
```

* When creating releases on github and sourceforge, remember to copy
  `README-what-to-download.md` separately onto the download area if
  needed.

* From the source tree, ensure that the main branch has been pushed to
  github. The rev-parse command below should show the same commit hash
  for all its arguments. Create and push a signed tag. This should be
  run with HEAD pointing to the tip of main.

```sh
git rev-parse qpdf/main @
git tag -s v$version @ -m"qpdf $version"
git push qpdf v$version
```

* Update documentation branches

```sh
git push qpdf @:$(echo $version | sed -E 's/\.[^\.]+$//')
git push qpdf @:stable
```

* If this is an x.y.0 release, visit
  https://readthedocs.org/projects/qpdf/versions/ (log in with
  github), and activate the latest major/minor version

* Create a notes file for the GitHub release. Copy the template below
  to /tmp/notes.md and edit as needed.

```markdown
This is qpdf version x.y.z. (Brief description, summary of highlights)

For a full list of changes from previous releases, please see the [release notes](https://qpdf.readthedocs.io/en/stable/release-notes.html). See also [README-what-to-download](./README-what-to-download.md) for details about the available source and binary distributions.

This release was signed by enter-email@address.here.
```

* Create a github release after pushing the tag. Use `gh` (GitHub
  CLI). This assumes you have `GH_TOKEN` set or are logged in. This
  must be run from the repository directory.

```sh
release=/path/to/release/$version
gh release create -R qpdf/qpdf v$version --title "qpdf version $version" -F /tmp/notes.md $release/*
```

* Upload files to sourceforge. Replace `sourceforge_login` with your
  SourceForge login. **NOTE**: The command below passes `-n` to rsync.
  This is no-op. Run it once to make sure it does the right thing,
  then run it again without `-n` to actually copy the files.

```sh
release=/path/to/release/$version
rsync -n -vrlcO $release/ sourceforge_login,qpdf@frs.sourceforge.net:/home/frs/project/q/qp/qpdf/qpdf/$version/
```

* On sourceforge, make the source package the default for all but
  Windows, and make the 64-bit msvc build the default for Windows.

* Publish a news item manually on sourceforge using the release notes
  text. **Remove the relative link to README-what-to-download.md** (just
  reference the file by name).

* Email the qpdf-announce list. Mention the email address of the release signer.

[//]: # (cSpell:ignore pikepdfs readthedocsorg dgenerate .)
