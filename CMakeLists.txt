# manual/installation.rst mentions the minimum cmake version.
cmake_minimum_required(VERSION 3.16)

# make_dist expects the version line to be on a line by itself after
# the project line. When updating the version, check make_dist for all
# the places it has to be updated. The doc configuration and CI build
# also find the version number here. generate_auto_job also reads the
# version from here.
project(qpdf
  VERSION 11.9.0
  LANGUAGES C CXX)

# Enable correct rpath handling for MacOSX
cmake_policy(SET CMP0042 NEW)
# Honor CMAKE_REQUIRED_LIBRARIES when checking for include files
cmake_policy(SET CMP0075 NEW)

# *** OPTIONS ***

# Keep all options here. It's easier to see the interdependencies this
# way than spreading them throughout the files.
#
# ***** Keep manual/installation.rst (_build-options) up to date. *****

include(CMakeDependentOption)

# CMAKE_DEPENDENT_OPTION(
#   OPTION "Description" default-value-if-visible
#   "when-visible" value-if-not-visible)

# Don't write tests based on MAINTAINER_MODE or CI_MODE. Instead, use
# them as the basis for dependent options.

option(MAINTAINER_MODE "Set options for developing qpdf" OFF)
CMAKE_DEPENDENT_OPTION(
  CI_MODE "Set options for running in CI" OFF
  "NOT MAINTAINER_MODE" OFF)
CMAKE_DEPENDENT_OPTION(
  WERROR "Treat compiler warnings as errors" OFF
  "NOT MAINTAINER_MODE; NOT CI_MODE" ON)
CMAKE_DEPENDENT_OPTION(
  CHECK_SIZES "Compare sizes.cc with classes in public API" OFF
  "NOT MAINTAINER_MODE" ON)
CMAKE_DEPENDENT_OPTION(
  GENERATE_AUTO_JOB "Automatically regenerate job files" OFF
  "NOT MAINTAINER_MODE" ON)
CMAKE_DEPENDENT_OPTION(
  ENABLE_QTC "Enable QTC test coverage" OFF
  "NOT MAINTAINER_MODE" ON)
CMAKE_DEPENDENT_OPTION(
  SHOW_FAILED_TEST_OUTPUT "Show qtest output on failure" OFF
  "NOT CI_MODE" ON)

# To allow building doc to be disabled in maintainer mode, handle the
# condition manually rather than using a dependent option.
if(MAINTAINER_MODE)
  set(default_BUILD_DOC ON)
else()
  set(default_BUILD_DOC OFF)
endif()
option(BUILD_DOC "Build documentation" ${default_BUILD_DOC})
# The values of BUILD_DOC_HTML and BUILD_DOC_PDF are ignored without
# BUILD_DOC, so setting them to ON when not visible forces them to be
# on in MAINTAINER_MODE and is harmless if BUILD_DOC is off.
CMAKE_DEPENDENT_OPTION(
  BUILD_DOC_HTML "Build HTML documentation"
  ON "BUILD_DOC;NOT MAINTAINER_MODE" ON)
CMAKE_DEPENDENT_OPTION(
  BUILD_DOC_PDF "Build PDF documentation"
  ON "BUILD_DOC;NOT MAINTAINER_MODE" ON)
CMAKE_DEPENDENT_OPTION(
  BUILD_DOC_DIST "Create distribution of manual" ON
  "BUILD_DOC_PDF;BUILD_DOC_HTML" OFF)

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(BUILD_SHARED_LIBS "Build qpdf shared libraries" ON)
option(BUILD_STATIC_LIBS "Build qpdf static libraries" ON)
option(QTEST_COLOR "Whether qtest's output should be in color" ON)
option(USE_INSECURE_RANDOM "Use insecure random numbers" OFF)
option(SKIP_OS_SECURE_RANDOM
  "Suppress use of OS-provided secure random numbers" OFF)
CMAKE_DEPENDENT_OPTION(
  AVOID_WINDOWS_HANDLE "Avoid use of HANDLE in Windows" OFF
  "WIN32" OFF)

option(OSS_FUZZ "Specific build configuration for the oss-fuzz project" OFF)

option(USE_IMPLICIT_CRYPTO "Enable any available external crypto provider" ON)
CMAKE_DEPENDENT_OPTION(
  ALLOW_CRYPTO_NATIVE "Allow native crypto as as fallback" ON
  "USE_IMPLICIT_CRYPTO" OFF)
CMAKE_DEPENDENT_OPTION(
  REQUIRE_CRYPTO_NATIVE "Require native crypto provider" OFF
  "NOT MAINTAINER_MODE; NOT CI_MODE" ON)
option(REQUIRE_CRYPTO_OPENSSL "Require openssl crypto" OFF)
option(REQUIRE_CRYPTO_GNUTLS "Require gnutls crypto" OFF)
set(DEFAULT_CRYPTO CACHE STRING "")
option(DEFAULT_CRYPTO
  "Specify default crypto; otherwise chosen automatically" "")

# INSTALL_MANUAL is not dependent on building docs. When creating some
# distributions, we build the doc in one run, copy doc-dist in, and
# install it elsewhere.
option(INSTALL_MANUAL "Install documentation" OFF)

option(INSTALL_PKGCONFIG "Install pkgconfig file" ON)
option(INSTALL_CMAKE_PACKAGE "Install cmake package files" ON)
option(INSTALL_EXAMPLES "Install example files" ON)

option(FUTURE "Include ABI-breaking changes CONSIDERED for the next major release" OFF)
option(CXX_NEXT "Build with next C++ standard version" OFF)

# *** END OPTIONS ***

if(NOT (BUILD_STATIC_LIBS OR BUILD_SHARED_LIBS))
  message(
    FATAL_ERROR "At least one of static or shared libraries must be built")
endif()

set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)
add_compile_definitions($<$<COMPILE_LANGUAGE:CXX>:POINTERHOLDER_TRANSITION=4>)

if(ENABLE_QTC)
  set(ENABLE_QTC_ARG)
else()
  add_compile_definitions(QPDF_DISABLE_QTC=1)
  set(ENABLE_QTC_ARG --disable-tc)
endif()

if(FUTURE)
  add_compile_definitions(QPDF_FUTURE=1)
endif()

enable_testing()
set(RUN_QTEST perl ${qpdf_SOURCE_DIR}/run-qtest ${ENABLE_QTC_ARG})

if(WIN32)
  find_program(COPY_COMMAND NAMES cp copy)
  if(COPY_COMMAND STREQUAL "COPY_COMMAND-NOTFOUND")
    set(COPY_COMMAND "copy")
  endif()
else()
  set(COPY_COMMAND "cp")
endif()

# For a long time, qpdf used libtool's version system. We are no
# longer doing that, but to avoid potential conflict with older
# versions, continue to have the shared library symlink point to a
# file whose version shares minor and patch with the project version
# and major with the SOVERSION. Starting with the transition to cmake,
# increment SOVERSION every time we increment the project major
# version. This works because qpdf uses semantic versioning. qpdf 10.x
# was libqpdf28, so start from there.

if(FUTURE)
  math(EXPR qpdf_SOVERSION 0)
  set(qpdf_LIBVERSION 0)
else()
  math(EXPR qpdf_SOVERSION "${PROJECT_VERSION_MAJOR} + 18")
  set(qpdf_LIBVERSION ${qpdf_SOVERSION}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
endif()

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "
Please build with cmake in a subdirectory, e.g.
 mkdir build
 cmake ..
 cmake --build .
Please remove CMakeCache.txt and the CMakeFiles directories.")
endif()

if(CXX_NEXT)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(WIN32 AND NOT SKIP_OS_SECURE_RANDOM)
  list(APPEND CMAKE_REQUIRED_LIBRARIES Advapi32)
endif()

include(CheckCXXSourceCompiles)
set(ATOMIC_LIBRARY)
function(check_atomic)
  foreach(I 0 1)
    if(I)
      set(CMAKE_REQUIRED_LIBRARIES atomic)
    endif()
    check_cxx_source_compiles(
      "#include <atomic>
int main() {
    static std::atomic<unsigned long long> a{0};
    a = a.fetch_add(1LL);
    return 0;
}"
      ATOMIC_WORKED${I})
    if(ATOMIC_WORKED0)
      return()
    endif()
  endforeach()
  if(ATOMIC_WORKED1)
    set(ATOMIC_LIBRARY atomic PARENT_SCOPE)
  endif()
endfunction()
check_atomic()

set(WINDOWS_WMAIN_COMPILE "")
set(WINDOWS_WMAIN_LINK "")
if(WIN32)
  function(check_wmain)
    foreach(I 0 1)
      if(NOT WINDOWS_WMAIN_COMPILE)
        if(I)
          set(CMAKE_REQUIRED_LINK_OPTIONS -municode)
        endif()
        check_cxx_source_compiles(
          "#include <windows.h>
#include <string.h>
#include <stdio.h>
extern \"C\"
int wmain(int argc, wchar_t* argv[])
{
    size_t x = wcslen(argv[0]);
    return 0;
}
"
          WMAIN_WORKED${I})
      endif()
    endforeach()
    if(WMAIN_WORKED1 OR WMAIN_WORKED1)
      set(WINDOWS_WMAIN_COMPILE -DWINDOWS_WMAIN PARENT_SCOPE)
      if(WMAIN_WORKED1 AND NOT WMAIN_WORKED0)
        set(WINDOWS_WMAIN_LINK -municode PARENT_SCOPE)
      endif()
    endif()
  endfunction()
  check_wmain()
endif()

if(MSVC)
  list(APPEND CMAKE_REQUIRED_LINK_OPTIONS -link setargv.obj)
  list(APPEND WINDOWS_WMAIN_LINK -link wsetargv.obj)
endif()

include(GNUInstallDirs)

if (DEFINED CMAKE_INSTALL_RPATH)
  set(Base_rpath "${CMAKE_INSTALL_RPATH}")
else()
  if(BUILD_SHARED_LIBS)
    set(p "${CMAKE_INSTALL_FULL_LIBDIR}")
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${p}" i)
    if("${i}" STREQUAL "-1")
      set(Base_rpath "${p}")
    endif()
  endif()
endif()

# Append non-standard external dependency directories, if any.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Compiler flags
#
# **NOTE** -- each flag must have its own cache variable.

include(qpdfCheckFlag)
if(WERROR)
  if(MSVC)
    add_compile_options(/WX)
  else()
    qpdf_maybe_add_flag(C -Werror flag_werror)
  endif()
endif()

if(MSVC)
  # /Gy combines identical functions -- useful with C++ templates
  add_compile_options(/Gy)
  add_compile_options(/W3)    # warning level 3
else()
  qpdf_maybe_add_flag(C -Wall flag_wall)
  qpdf_maybe_add_flag(C -Wconversion flag_conversion)
  qpdf_maybe_add_flag(C -Wsign-conversion flag_sign-conversion)
  qpdf_maybe_add_flag(C -Wshadow=local flag_shadow_local)
  qpdf_maybe_add_flag(CXX -Wold-style-cast flag_old-style-cast)
endif()

# We don't include the jpeg library's include path in the PUBLIC
# interface for libqpdf since only Pl_DCT.hh requires it. This is
# documented. Some examples and tests use it though so we have to
# define it. CMakeLists.txt for libqpdf sets the value for
# JPEG_INCLUDE which can be selectively added to include paths other
# tools.
set(JPEG_INCLUDE)

if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
  set(WORDSIZE 64)
else()
  set(WORDSIZE 32)
endif()
if(MSVC)
  set(CPACK_SYSTEM_NAME "msvc${WORDSIZE}")
elseif(MINGW)
  set(CPACK_SYSTEM_NAME "mingw${WORDSIZE}")
endif()
set(CPACK_RESOURCE_FILE_LICENSE "${qpdf_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://qpdf.sourceforge.io/")
set(CPACK_NSIS_MUI_ICON "${qpdf_SOURCE_DIR}/logo/qpdf.ico")

if(ENABLE_COVERAGE)
  add_compile_options(--coverage -O0)
  add_link_options(--coverage)
endif()

include(CPack)

# Install components -- documented in _installation in
# manual/installation.rst.
set(COMPONENT_DEV "dev")
set(COMPONENT_LIB "lib") # runtime library
set(COMPONENT_CLI "cli")
set(COMPONENT_DOC "doc")
set(COMPONENT_EXAMPLES "examples") # example sources

if(WIN32)
  include(InstallRequiredSystemLibraries)
endif()

set(auto_job_inputs
  # Keep in sync with SOURCES in generate_auto_job
  generate_auto_job
  CMakeLists.txt
  manual/_ext/qpdf.py
  job.yml
  manual/cli.rst
  manual/qpdf.1.in
)

set(auto_job_outputs
  # Keep in sync with DESTS in generate_auto_job
  libqpdf/qpdf/auto_job_decl.hh
  libqpdf/qpdf/auto_job_init.hh
  libqpdf/qpdf/auto_job_help.hh
  libqpdf/qpdf/auto_job_schema.hh
  libqpdf/qpdf/auto_job_json_decl.hh
  libqpdf/qpdf/auto_job_json_init.hh
  manual/qpdf.1
)

if(GENERATE_AUTO_JOB)
  add_custom_command(
    OUTPUT ${auto_job_outputs}
    COMMAND ${qpdf_SOURCE_DIR}/generate_auto_job --generate
    WORKING_DIRECTORY ${qpdf_SOURCE_DIR}
    DEPENDS ${auto_job_inputs})
  add_custom_target(auto_job_files ALL DEPENDS ${auto_job_outputs})
endif()

add_test(
  NAME check-assert
  COMMAND perl ${qpdf_SOURCE_DIR}/check_assert)

# add_subdirectory order affects test order
add_subdirectory(include)
add_subdirectory(libqpdf)
add_subdirectory(compare-for-test)
add_subdirectory(qpdf)
add_subdirectory(libtests)
add_subdirectory(examples)
add_subdirectory(zlib-flate)
add_subdirectory(manual)
add_subdirectory(fuzz)

# We don't need to show everything -- just the things that we really
# need to be sure are right or that are turned on or off with complex
# logic.
get_property(MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
message(STATUS "")
message(STATUS "*** Summary ***")
message(STATUS "  qpdf version: ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
if(MULTI_CONFIG)
  message(STATUS "  build type: specify --config at build time")
elseif(CMAKE_BUILD_TYPE)
  message(STATUS "  build type: ${CMAKE_BUILD_TYPE}")
endif()
message(STATUS "  build shared libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "  build static libraries: ${BUILD_STATIC_LIBS}")
message(STATUS "  build manual: ${BUILD_DOC}")
message(STATUS "  compiler warnings are errors: ${WERROR}")
message(STATUS "  QTC test coverage: ${ENABLE_QTC}")
message(STATUS "  include future changes: ${FUTURE}")
message(STATUS "  system: ${CPACK_SYSTEM_NAME}")
message(STATUS "")
message(STATUS "*** Options Summary ***")
foreach(PROP
    COMPILE_OPTIONS INTERFACE_COMPILE_OPTIONS
    COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS
    INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
    LINK_OPTIONS INTERFACE_LINK_OPTIONS
    LINK_LIBRARIES INTERFACE_LINK_LIBRARIES
    LINK_DIRECTORIES INTERFACE_LINK_DIRECTORIES)
  get_target_property(VAL libqpdf ${PROP})
  if(NOT (VAL STREQUAL "VAL-NOTFOUND"))
    message(STATUS "  ${PROP}: ${VAL}")
  endif()
endforeach()
if(APPLE)
  message(STATUS "  CMAKE_OSX_SYSROOT: ${CMAKE_OSX_SYSROOT}")
endif()
message(STATUS "")
message(STATUS "See above for crypto summary.")
message(STATUS "")
if(NOT (MULTI_CONFIG OR CMAKE_BUILD_TYPE))
  message(WARNING "  CMAKE_BUILD_TYPE is not set; using default settings")
endif()
