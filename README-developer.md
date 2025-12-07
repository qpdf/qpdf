# Notes for Contributors and Other qpdf Developers

This file contains notes of interest to developers that want to modify qpdf itself rather than using
qpdf as a library.

## Contents

* [ROUTINE DEVELOPMENT](#routine-development)
* [CHECKING DOCS ON readthedocs](#checking-docs-on-readthedocs)
* [CODING RULES](#coding-rules)
* [ZLIB COMPATIBILITY](#zlib-compatibility)
* [HOW TO ADD A COMMAND-LINE ARGUMENT](#how-to-add-a-command-line-argument)
* [RUNNING pikepdf's TEST SUITE](#running-pikepdfs-test-suite)
* [OTHER NOTES](#other-notes)
* [DEPRECATION](#deprecation)
* [LOCAL WINDOWS TESTING PROCEDURE](#local-windows-testing-procedure)
* [DOCS ON readthedocs.org](#docs-on-readthedocsorg)
* [CMAKE notes](#cmake-notes)
* [ABI checks](#abi-checks)
* [CODE FORMATTING](#code-formatting)


## ROUTINE DEVELOPMENT

**When making changes that users need to know about, update the release notes
(manual/release-notes.rst) as you go.** Major changes to the internal API can also be mentioned in
the release notes in a section called "Internal Changes" or similar. This removes ChangeLog as a
separate mechanism for tracking changes.

**Remember to check pull requests as well as issues in github.**

Run `cmake --list-presets` to see available cmake presets. Routine maintainer development can be

```
cmake --preset maintainer
cmake --build --preset maintainer
ctest --preset maintainer
```

See [CMakePresets.json](CMakePresets.json) for additional presets. Reminders about presets:
* You can override/enhance configure presets, e.g., `cmake --preset maintainer -DCMAKE_BUILD_TYPE=Release`
* You can pass flags to ctest, e.g., `ctest --preset maintainer -R zlib-flate`.
* You can't override the build directory for build and test presets, but you _can_ override the
  directory for configure presets and then run `cmake --build build-dir` and `ctest` manually, as
  shown below.
* The base configuration includes `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, which is useful for LSP mode
  in C++. This is harmless in environments where it's not needed. You may need to make a symlink
  from compile_commands.json to the one in whichever build directory you are using.
* If you have common configurations you'd like to see, pull requests are welcome, but
  `CMakeUserPresets.json` is your friend. You can copy or inherit from CMakeUserPresets.json for
  your own use. Note that CMakeUserPresets.json is not part of the stable API. We reserve the right
  to modify these presets in a non-compatible fashion at any time without regard to qpdf version
  numbers, but we should mention changes in the release notes.
* Study the CMakePresets.json file for details on how these are implemented.

See also ./build-scripts for other ways to run the build for different configurations.

### Useful build examples

To run a maintainer build in release mode and run only the unicode-filenames test, you could run

```
cmake --preset maintainer -DCMAKE_BUILD_TYPE=Release
cmake --build --preset maintainer
TESTS=unicode-filenames ctest --preset maintainer -R qpdf
```

To run a maintainer build in release mode in a _different directory_ and run only the
unicode-filenames test, you could run the following. Trying to override the directory on the command
line of `cmake --build` or `ctest` in conjunction with `--preset` may silently ignore the directory
override, and you may not get what you think you are getting.

```
cmake --preset maintainer -DCMAKE_BUILD_TYPE=Release -B cmake-build-release
cmake --build cmake-build-release
TESTS=unicode-filenames ctest --verbose --test-dir cmake-build-release -R qpdf
```

### Profiling

When running with the `maintainer-profile` preset (or any time you run profiling), run `gprof
gmon.out`. Note that gmon.out is not cumulative.

### Coverage

When running with the `maintainer-coverage` preset, after running tests:
```
gcovr -r .. --html --html-details -o coverage-report.html
```

Note that, in early 2024, branch coverage information is not very accurate with C++.

### Sanitizers/Memory Checks

If `clang++` fails to create output during configuration, it may be necessary to install a specific
version of libstdc++-dev. For example, with clang++ version 20 on Ubuntu 24.04, `clang++ -v`
indicates the selected GCC installation is 14, so it is necessary to install `libstdc++-14-dev`.

### Windows

You can use this for command-line builds, which does a bit more than the presets. The msvc presets
are known to work in CLion if the environment is set up as described in
[README-windows.md](./README-windows.md), but for regular command-line builds (and CI), continue to
use `cmake-win` from inside a build directory. Look at `build-scripts/build-windows` to see how this
is used.

```
../cmake-win {mingw|msvc} maint
```

## CHECKING DOCS ON readthedocs

To check docs on readthedocs.io without running all of CI, push to the
doc-check branch. Then visit https://qpdf.readthedocs.io/en/doc-check/
Building docs from pull requests is also enabled.

## CODING RULES

* Code is formatted with clang-format. See .clang-format and the
  "Code Formatting" section in manual/contributing.rst for details.
  See also "CODE FORMATTING" below.

* Use std::to_string instead of QUtil::int_to_string et al

* Use of assert:

  * Test code: #include <qpdf/assert_test.h> first.
  * Debug code: #include <qpdf/assert_debug.h> first and use
    qpdf_assert_debug instead of assert. Note that <qpdf/Util.hh>
    includes assert_debug.h. Include this instead if 'At most one
    qpdf/assert header ...' errors are encountered, especially when
    using assert in private header files.
  * Use 'qpdf_expect', 'qpdf_static_expect', 'qpdf_ensures' and
    'qpdf_invariant' to document pre/post-conditions and invariants.
    This requires inclusion of 'assert_debug.h' or 'Util.hh'. Remember
    that these (except for 'qpdf_static_expect') are only checked in
    debug builds.
  * Use 'util::assertion' when checks should also be carried out in
    release code in preference to throwing logic_errors directly
    unless it is practical and desirable to test violations during
    CI testing. This avoids obscuring genuine gaps in coverage with
    noise generated by unreachable sanity checks.

  These rules are enforced by the check-assert test. This practices
  serves to

  * remind us that assert in release code disappears and so should only
    be used to document pre/post conditions and invariants, and for
    sanity checks during development and testing; when doing so use
    a Debug build configuration.

  * protect us from using assert in test code without explicitly
    removing the NDEBUG definition, since that would cause the assert
    not to actually be testing anything in non-Debug build
    configurations.
  
  Prior to 12.3 assert_test.h and assert_debug.h shared the same header
  guard, which prevented the simultaneous inclusion of both headers.
  This was changed to permit the CI testing of private-API methods
  without loosing the use of assertions in private header files.

* In a source file, include the header file that declares the source
  class first followed by a blank line. If a config file is needed
  first, put a blank line between that and the header followed by
  another blank line. This assures that each header file is included
  first at least once, thereby ensuring that it explicitly includes
  all the headers it needs, which in turn alleviates lots of header
  ordering problems. The blank line ensures that formatters don't
  mess this up by resorting the headers.

* Avoid atoi. Use QUtil::string_to_int instead. It does
  overflow/underflow checking.

* Avoid certain functions that tend to be macros or create compilation
  errors on some platforms. Known cases: strcasecmp, abs. Avoid min
  and max. If needed, std::min and std::max are okay to use in C++
  code with <algorithm> included.

* Remember to avoid using `operator[]` with `std::string` or
  `std::vector`. Instead, use `at()`. See README-hardening.md for
  details.

* Use QIntC for type conversions -- see casting policy in docs.

* Remember to imbue ostringstreams with std::locale::classic() before
  outputting numbers. This protects against the user's global locale
  altering otherwise deterministic values. (See github issue #459.)
  One could argue that error messages containing numbers should
  respect the user's locale, but I think it's more important for
  output to be consistent, since the messages in question are not
  really targeted at the end user.

* Use QPDF_DLL on all methods that are to be exported in the shared
  library/DLL. Use QPDF_DLL_CLASS for all classes whose type
  information is needed. This is important for classes that are used
  as exceptions, subclassed, or tested with dynamic_cast across the
  the shared object boundary (or "shared library boundary" -- we may
  use either term in comments and documentation). In particular,
  anything new derived from Pipeline or InputSource should be marked
  with QPDF_DLL_CLASS. We shouldn't need to do it for QPDFObjectHelper
  or QPDFDocumentHelper subclasses since there's no reason to use
  dynamic_cast with those, but doing it anyway may help with some
  strange cases for mingw or with some code generators that may
  systematically do this for other reasons.

  IMPORTANT NOTE ABOUT QPDF_DLL_CLASS: On mingw, the vtable for a
  class with some virtual methods and no pure virtual methods seems
  often (always?) not to be generated if the destructor is inline or
  declared with `= default`. Therefore, for any class that is intended
  to be used as a base class and doesn't contain any pure virtual
  methods, you must declare the destructor in the header without
  `= default` and provide a non-inline implementation in the source
  file. Add this comment to the implementation:

    ```cpp
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
    ```

* Put private member variables in std::unique_ptr<Members> for all
  public classes. Forward declare Members in the header file and define
  Members in the implementation file. One of the major benefits of
  defining Members in the implementation file is that it makes it easier
  to use private classes as data members and simplifies the include order.
  Remember that Members must be fully defined before the destructor of the
  main class. For an example of this pattern see class JSONHandler.

  Exception: indirection through std::unique_ptr<Members> incurs an overhead,
  so don't do it for:
  * (especially private) classes that are copied a lot, like QPDFObjectHandle
    and QPDFObject.
  * classes that are a shared pointer to another class, such as QPDFObjectHandle
    or JSON.

  For exported classes that do not use the member pattern for performance
  reasons it is worth considering adding a std::unique_ptr to an empty Members
  class initialized to nullptr to give the flexibility to add data members
  without breaking the ABI.

  Note that, as of qpdf 11, many public classes use `std::shared_ptr`
  instead. Changing this to `std::unique_ptr` is ABI-breaking. If the
  class doesn't allow copying, we can switch it to std::unique_ptr and
  let that be the thing that prevents copying. If the intention is to
  allow the object to be copied by value and treated as if it were
  copied by reference, then `std::shared_ptr<Members>` should be used.
  The `JSON` class is an example of this. As a rule, we should avoid
  this design pattern. It's better to make things non-copiable and to
  require explicit use of shared pointers, so going forward,
  `std::unique_ptr` should be preferred.

* Traversal of objects is expensive. It's worth adding some complexity
  to avoid needless traversals of objects.

* Avoid attaching too much metadata to objects and object handles
  since those have to get copied around a lot.

* Prefer std::string_view to std::string const& and char const*.

  * Where functions rely on strings being null-terminated, std::string_view may not be appropriate.

  * For return values, consider whether returning a string_view is safe or whether it is more appropriate
    to return a std::string or std::string const&, especially in the public API.

  * NEVER replace a std::string const& return value with std::string_view in the public API.


## ZLIB COMPATIBILITY

The qpdf test suite is designed to be independent of the output of any
particular version of zlib. (See also `ZOPFLI` in README.md.) There
are several strategies to make this work:

* `build-scripts/test-alt-zlib` runs in CI and runs the test suite
  with a non-default zlib. Please refer to that code for an example of
  how to do this in case you want to test locally.

* The test suite is full of cases that compare output PDF files with
  expected PDF files in the test suite. If the file contains data that
  was compressed by QPDFWriter, then the output file will depend on
  the behavior of zlib. As such, using a simple comparison won't work.
  There are several strategies used by the test suite.

  * A new program called `qpdf-test-compare`, in most cases, is a drop
    in replacement for a simple file comparison. This code make sure
    the two files have exactly the same number of objects with the
    same object and generation numbers, and that corresponding objects
    are identical with the following allowances (consult its source
    code for all the details details):
    * The `/Length` key is not compared in stream dictionaries.
    * The second element of `/ID` is not compared.
    * If the first and second element of `/ID` are the same, then the
      first element if `/ID` is also not compared.
    * If a stream is compressed with `/FlateDecode`, the
      _uncompressed_ stream data is compared. Otherwise, the raw
      stream data is compared.
    * Generated fields in the `/Encrypt` dictionary are not compared,
      though password-protected files must have the same password.
    * Differences in the contents of `/XRef` streams are ignored.

    To use this, run `qpdf-test-compare actual.pdf expected.pdf`, and
    expect the output to match `expected.pdf`. For example, if a test
    used to be written like this;
    ```perl
    $td->runtest("check output",
                 {$td->FILE => "a.pdf"},
                 {$td->FILE => "out.pdf"});
    ```
    then write it like this instead:
    ```perl
    $td->runtest("check output",
                 {$td->COMMAND => "qpdf-test-compare a.pdf out.pdf"},
                 {$td->FILE => "out.pdf", $td->EXIT_STATUS => 0});
    ```
    You can look at `compare-for-test/qtest/compare.test` for
    additional examples.

    Here's what's going on:
    * If the files "match" according to the rules of
      `qpdf-test-compare`, the output of the program is the expected
      file.
    * If the files do not match, the output is the actual file. The
      reason is that, if a change is made that results in an expected
      change to the expected file, the output of the comparison can be
      used to replace the expected file (as long as it is definitely
      known to be correct—no shortcuts here!). That way, it doesn't
      matter which zlib you use to generate test files.
    * As a special debugging tool, you can set the `QPDF_COMPARE_WHY`
      environment variable to any value. In this case, if the files
      don't match, the output is a description of the first thing in
      the file that doesn't match. This is mostly useful for debugging
      `qpdf-test-compare` itself, but it can also be helpful as a
      sanity check that the differences are expected. If you are
      trying to find out the _real_ differences, a suggestion is to
      convert both files to qdf and compare them lexically.

  * There are some cases where `qpdf-test-compare` can't be used. For
    example, if you need to actually test one of the things that
    `qpdf-test-compare` ignores, you'll need some other mechanism.
    There are tests for deterministic ID creation and xref streams
    that have to implement other mechanisms. Also, linearization hint
    streams and the linearization dictionary in a linearized file
    contain file offsets. Rather than ignoring those, it can be
    helpful to create linearized files using `--compress-streams=n`.
    In that case, `QPDFWriter` won't compress any data, so the PDF
    will be independent of the output of any particular zlib
    implementation.

You can find many examples of how tests were rewritten by looking at
the commits preceding the one that added this section of this README
file.

Note about `/ID`: many test cases use `--static-id` to have a
predictable `/ID` for testing. Many other test cases use
`--deterministic-id`. While `--static-id` is unaffected by file
contents, `--deterministic-id` is based on file contents and so is
dependent on zlib output if there is any newly compressed data. By
using `qpdf-test-compare`, it's actually not necessary to use either
`--static-id` or `--deterministic-id`. It may still be necessary to
use `--static-aes-iv` if comparing encrypted files, but since
`qpdf-test-compare` ignores `/Perms`, a wider range of encrypted files
can be compared using `qpdf-test-compare`.


## HOW TO ADD A COMMAND-LINE ARGUMENT

Quick reminder:

* Add an entry to the top half of job.yml for the command-line
  argument
* Add an entry to the bottom half of job.yml for the job JSON field
* Add documentation for the new option to cli.rst
* Implement the QPDFJob::Config method in QPDFJob_config.cc
* Pass the build option `-DGENERATE_AUTO_JOB=1` to `cmake`
  (see [here](https://qpdf.readthedocs.io/en/stable/installation.html#options-for-working-on-qpdf))
  or run `generate_auto_job` manually.
* Adding new options tables is harder -- see below

QPDFJob is documented in three places:

* This section provides a quick reminder for how to add a command-line
  argument

* generate_auto_job has a detailed explanation about how QPDFJob and
  generate_auto_job work together

* The manual ("QPDFJob Design" in qpdf-job.rst) discusses the design
  approach, rationale, and evolution of QPDFJob.

Command-line arguments are closely coupled with QPDFJob. To add a new
command-line argument, add the option to the appropriate table in
job.yml. After `generate_auto_job` is run (either manually or as part
of the build process, when `GENERATE_AUTO_JOB` is set), this will
automatically declare a method in the private ArgParser class in
QPDFJob_argv.cc which you have to implement. The implementation should
make calls to methods in QPDFJob via its Config classes. Then, add the
same option to either the no-json section of job.yml if it is to be
excluded from the job json structure, or add it under the json
structure to the place where it should appear in the json structure.

In most cases, adding a new option will automatically declare and call
the appropriate Config method, which you then have to implement. If
you need a manual handler, you have to declare the option as manual in
job.yml and implement the handler yourself, though the automatically
generated code will declare it for you.

Adding a new option table is a bit harder and is not well-documented.
For a simple example, look at the code that added the
--set-page-labels table. That change was divided into two commits (one
for the manual changes, and one for the generated changes) to make it
easier to use as an example.

The build will fail until the new option is documented in
manual/cli.rst. To do that, create documentation for the option by
adding a ".. qpdf:option::" directive followed by a magic help comment
as described at the top of manual/cli.rst. Put this in the correct
help topic. Help topics roughly correspond with sections in that
chapter and are created using a special ".. help-topic" comment.
Follow the example of other options for style.

When done, the following should happen:

* qpdf --new-option should work as expected
* qpdf --help=--new-option should show the help from the comment in cli.rst
* qpdf --help=topic should list --new-option for the correct topic
* --new-option should appear in the manual
* --new-option should be in the command-line option index in the manual
* A Config method (in Config or one of the other Config classes in
  QPDFJob) should exist that corresponds to the command-line flag
* The job JSON file should have a new key in the schema corresponding
  to the new option


## RUNNING pikepdf's TEST SUITE

We run pikepdf's test suite from CI. These instructions show how to do
it manually.

Do this in a separate shell.

```
cd ...qpdf-source-tree...
export QPDF_SOURCE_TREE=$PWD
export QPDF_BUILD_LIBDIR=$QPDF_SOURCE_TREE/build/libqpdf
export LD_LIBRARY_PATH=$QPDF_BUILD_LIBDIR
rm -rf /tmp/z
mkdir /tmp/z
cd /tmp/z
git clone git@github.com:pikepdf/pikepdf
python3 -m venv v
source v/bin/activate
cd pikepdf
python3 -m pip install --upgrade pip
python3 -m pip install '.[test]'
rehash
python3 -m pip install .
pytest -n auto
```

If there are failures, use git bisect to figure out where the failure
was introduced. For example, set up a work area like this:

```
rm -rf /tmp/z
mkdir /tmp/z
cd /tmp/z
git clone file://$HOME/source/qpdf/qpdf/.git qpdf
git clone git@github.com:pikepdf/pikepdf
export QPDF_SOURCE_TREE=/tmp/z/qpdf
export QPDF_BUILD_LIBDIR=$QPDF_SOURCE_TREE/build/libqpdf
export LD_LIBRARY_PATH=$QPDF_BUILD_LIBDIR
cd qpdf
mkdir build
cmake -B build -DMAINTAINER_MODE=ON -DBUILD_STATIC_LIBS=OFF \
   -DCMAKE_BUILD_TYPE=RelWithDebInfo
cat <<'EOF'
#!/bin/bash
cd /tmp/z/pikepdf
cmake --build /tmp/z/qpdf/build -j16 --target libqpdf -- -k
git clean -dfx
rm -rf ../v
python3 -m venv ../v
source ../v/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install '.[test]'
python3 -m pip install .
pytest -n auto
EOF
chmod +x /tmp/check
```

Then in /tmp/z/qpdf, run git bisect. Use /tmp/check at each stage to
test whether it's a good or bad commit.


## OTHER NOTES

For local iteration on the AppImage generation, it works to just
./build-scripts/build-appimage and get the resulting AppImage from the
distribution directory. You can pass additional arguments to
build-appimage, which passes them along to to docker.

Use -e SKIP_TESTS=1 to skip the test suite.
Use -ti -e RUN_SHELL=1 to run a shell instead of the build script.

To iterate on the scripts directly in the source tree, you can run

```
docker build -t qpdfbuild appimage
docker run --privileged --rm -ti -e SKIP_TESTS=1 -e RUN_SHELL=1 \
       -v $PWD/..:/tmp/build ${1+"$@"} qpdfbuild
```

This will put you at a shell prompt inside the container with your
current directory set to the top of the source tree and your uid equal
to the owner of the parent directory source tree.

Note: this will leave some extra files (like .bash_history) in the
parent directory of the source tree. You will want to clean those up.


## DEPRECATION

This is a reminder of how to use and test deprecation.

To temporarily disable deprecation warnings for testing:

```cpp
#ifdef _MSC_VER
# pragma warning(disable : 4996)
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    // Do deprecated thing here
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif
```

To declare something as deprecated:

```cpp
[[deprecated("explanation")]]
```

## LOCAL WINDOWS TESTING PROCEDURE

This is what I do for routine testing on Windows.

* From Windows, git clone from my Linux clone, and unzip
  `external-libs`.

* Start a command-line shell for x86_64 and x86 tools from Visual
  studio. From there, start C:\msys64\mingw64 twice and
  C:\msys64\mingw32 twice.

* Create a build directory for each of the four permutations. Then, in
  each build directory, run `../cmake-win <tool> maint`.

* Run `cmake --build . -j4`. For MSVC, add `--config Release`

* Test with with msvc: `ctest --verbose -C Release`

* Test with mingw:  `ctest --verbose -C RelWithDebInfo`

## DOCS ON readthedocs.org

* Registered for an account at readthedocs.org with my github account
* Project page: https://readthedocs.org/projects/qpdf/
* Docs: https://qpdf.readthedocs.io/
* Admin -> Settings
  * Set project home page
  * Advanced
    * Show version warning
    * Default version: stable
  * Email Notifications: set email address for build failures

At this time, there is nothing in .github/workflows to support this.
It's all set up as an integration directly between github and
readthedocs.

The way readthedocs.org does stable and versions doesn't exactly work
for qpdf. My tagging convention is different from what they expect,
and I don't need versions for every point release. I have the
following branching strategy to support docs:

* x.y -- points to the latest x.y.z release
* stable -- points to the latest release

The release process includes updating the approach branches and
activating versions.


## CMAKE notes

To verify the various cmake options and their interactions, several
manual tests were done:

* Break installed qpdf executables (qpdf, fix-qdf, zlib-flate), the
  shared library, and DLL.h to ensure that other qpdf installations do
  not interfere with building from source

* Build static only and shared only

* Install separate components separately

* Build only HTML docs and only PDF docs

* Try MAINTAINER_MODE without BUILD_DOC

We are using RelWithDebInfo for mingw and other non-Windows builds but
Release for MSVC. There are linker warnings if MSVC is built with
RelWithDebInfo when using external-libs.


## ABI checks

Note: the check_abi program requires [castxml](https://github.com/CastXML/CastXML) to be installed.

Until the conversion of the build to cmake, we relied on running the
test suite with old executables and a new library. When QPDFJob was
introduced, this method got much less reliable since a lot of public
API doesn't cross the shared library boundary. Also, when switching to
cmake, we wanted a stronger check that the library had the expected
ABI.

Our ABI check now consists of three parts:

* The same check as before: run the test suite with old executables
  and a new library

* Do a literal comparison of the symbols in the old and new shared
  libraries -- this is a strong test of ABI change

* Do a check to ensure that object sizes didn't change -- even with no
  changes to the API of exported functions, size changes break API

The combination of these checks is pretty strong, though there are
still things that could potentially break ABI, such as

* Changes to the types of public or protected data members without
  changing the size

* Changes to the meanings of parameters without changing the signature

Not breaking ABI/API still requires care.

The check_abi script is responsible for performing many of these
steps. See comments in check_abi for additional notes.


## CODE FORMATTING

* Emacs doesn't indent breaking strings concatenated with + over
  lines but clang-format does. It's clearer with clang-format. To
  get emacs and clang-format to agree, parenthesize the expression
  that builds the concatenated string.

* With

  ```cpp
  long_function(long_function(
      args)

  ```

  clang-format anchors relative to the first function, and emacs
  anchors relative to the second function. Use

  ```cpp
  long_function(
      // line-break
      long_function(
	  args)
  ```
  to resolve.

In the revision control history, there is a commit around April 3,
2022 with the title "Update some code manually to get better
formatting results" that shows several examples of changing code so
that clang-format produces several results. (In git this is commit
77e889495f7c513ba8677df5fe662f08053709eb.)

The commits that have the bulk of automatic or mechanical reformatting are
listed in .git-blame-ignore-revs. Any new bulk updates should be added there.

[//]: # (cSpell:ignore pikepdfs readthedocsorg dgenerate .)
