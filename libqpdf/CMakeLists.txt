set(libqpdf_crypto_native
  AES_PDF_native.cc
  MD5_native.cc
  QPDFCrypto_native.cc
  RC4_native.cc
  SHA2_native.cc
  rijndael.cc
  sha2.c
  sha2big.c)

set(libqpdf_crypto_openssl
  QPDFCrypto_openssl.cc)

set(libqpdf_crypto_gnutls
  QPDFCrypto_gnutls.cc)

set(libqpdf_SOURCES
  BitStream.cc
  BitWriter.cc
  Buffer.cc
  BufferInputSource.cc
  ClosedFileInputSource.cc
  ContentNormalizer.cc
  CryptoRandomDataProvider.cc
  FileInputSource.cc
  InputSource.cc
  InsecureRandomDataProvider.cc
  JSON.cc
  JSONHandler.cc
  MD5.cc
  NNTree.cc
  OffsetInputSource.cc
  PDFVersion.cc
  Pipeline.cc
  Pl_AES_PDF.cc
  Pl_ASCII85Decoder.cc
  Pl_ASCIIHexDecoder.cc
  Pl_Base64.cc
  Pl_Buffer.cc
  Pl_Concatenate.cc
  Pl_Count.cc
  Pl_DCT.cc
  Pl_Discard.cc
  Pl_Flate.cc
  Pl_Function.cc
  Pl_LZWDecoder.cc
  Pl_MD5.cc
  Pl_OStream.cc
  Pl_PNGFilter.cc
  Pl_QPDFTokenizer.cc
  Pl_RC4.cc
  Pl_RunLength.cc
  Pl_SHA2.cc
  Pl_StdioFile.cc
  Pl_String.cc
  Pl_TIFFPredictor.cc
  QPDF.cc
  QPDFAcroFormDocumentHelper.cc
  QPDFAnnotationObjectHelper.cc
  QPDFArgParser.cc
  QPDFCryptoProvider.cc
  QPDFDocumentHelper.cc
  QPDFEFStreamObjectHelper.cc
  QPDFEmbeddedFileDocumentHelper.cc
  QPDFExc.cc
  QPDFFileSpecObjectHelper.cc
  QPDFFormFieldObjectHelper.cc
  QPDFJob.cc
  QPDFJob_argv.cc
  QPDFJob_config.cc
  QPDFJob_json.cc
  QPDFLogger.cc
  QPDFMatrix.cc
  QPDFNameTreeObjectHelper.cc
  QPDFNumberTreeObjectHelper.cc
  QPDFObject.cc
  QPDFObjectHandle.cc
  QPDFObjectHelper.cc
  QPDFObjGen.cc
  QPDFOutlineDocumentHelper.cc
  QPDFOutlineObjectHelper.cc
  QPDFPageDocumentHelper.cc
  QPDFPageLabelDocumentHelper.cc
  QPDFPageObjectHelper.cc
  QPDFParser.cc
  QPDFStreamFilter.cc
  QPDFSystemError.cc
  QPDFTokenizer.cc
  QPDFUsage.cc
  QPDFValue.cc
  QPDFWriter.cc
  QPDFXRefEntry.cc
  QPDF_Array.cc
  QPDF_Bool.cc
  QPDF_Destroyed.cc
  QPDF_Dictionary.cc
  QPDF_InlineImage.cc
  QPDF_Integer.cc
  QPDF_Name.cc
  QPDF_Null.cc
  QPDF_Operator.cc
  QPDF_Real.cc
  QPDF_Reserved.cc
  QPDF_Stream.cc
  QPDF_String.cc
  QPDF_Unresolved.cc
  QPDF_encryption.cc
  QPDF_json.cc
  QPDF_linearization.cc
  QPDF_optimization.cc
  QPDF_pages.cc
  QTC.cc
  QUtil.cc
  RC4.cc
  ResourceFinder.cc
  SecureRandomDataProvider.cc
  SF_FlateLzwDecode.cc
  qpdf-c.cc
  qpdfjob-c.cc
  qpdflogger-c.cc)

include(FindPkgConfig)
include(CheckTypeSize)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckSymbolExists)

set(dep_include_directories)
set(dep_link_directories)
set(dep_link_libraries)
set(ANYTHING_MISSING 0)

if(WIN32 AND (EXISTS ${qpdf_SOURCE_DIR}/external-libs))
  set(EXTERNAL_LIBS 1)
else()
  set(EXTERNAL_LIBS 0)
endif()

if(EXTERNAL_LIBS)
  set(EXTLIBDIR ${qpdf_SOURCE_DIR}/external-libs)
  list(APPEND dep_include_directories ${EXTLIBDIR}/include)
  set(JPEG_INCLUDE ${EXTLIBDIR}/include)
  list(APPEND dep_link_libraries
    z jpeg ssl crypto msvcrt ws2_32 shell32 advapi32 gdi32 user32 crypt32)
  if (MSVC)
    list(APPEND dep_link_directories ${EXTLIBDIR}/lib-msvc${WORDSIZE})
  else()
    list(APPEND dep_link_directories ${EXTLIBDIR}/lib-mingw${WORDSIZE})
  endif()
endif()

if(NOT EXTERNAL_LIBS)
  pkg_check_modules(pc_zlib zlib)
  if(pc_zlib_FOUND)
    list(APPEND dep_include_directories ${pc_zlib_INCLUDEDIR})
    list(APPEND dep_link_directories ${pc_zlib_LIBDIR})
    list(APPEND dep_link_libraries ${pc_zlib_LIBRARIES})
  else()
    find_path(ZLIB_H_PATH zlib.h)
    find_library(ZLIB_LIB_PATH z zlib)
    if(ZLIB_H_PATH AND ZLIB_LIB_PATH)
      list(APPEND dep_include_directories ${ZLIB_H_PATH})
      list(APPEND dep_link_libraries ${ZLIB_LIB_PATH})
    else()
      message(SEND_ERROR "zlib not found")
      set(ANYTHING_MISSING 1)
    endif()
  endif()
endif()

if(NOT EXTERNAL_LIBS)
  pkg_check_modules(pc_libjpeg libjpeg)
  if(pc_libjpeg_FOUND)
    list(APPEND dep_include_directories ${pc_libjpeg_INCLUDEDIR})
    list(APPEND dep_link_directories ${pc_libjpeg_LIBDIR})
    list(APPEND dep_link_libraries ${pc_libjpeg_LIBRARIES})
    set(JPEG_INCLUDE ${pc_libjpeg_INCLUDEDIR})
  else()
    find_path(LIBJPEG_H_PATH jpeglib.h)
    find_library(LIBJPEG_LIB_PATH jpeg)
    if(LIBJPEG_H_PATH AND LIBJPEG_LIB_PATH)
      list(APPEND dep_include_directories ${LIBJPEG_H_PATH})
      list(APPEND dep_link_libraries ${LIBJPEG_LIB_PATH})
      set(JPEG_INCLUDE ${LIBJPEG_H_PATH})
    else()
      message(SEND_ERROR "libjpeg not found")
      set(ANYTHING_MISSING 1)
    endif()
  endif()
endif()

# Update JPEG_INCLUDE in PARENT_SCOPE after we have finished setting it.
set(JPEG_INCLUDE ${JPEG_INCLUDE} PARENT_SCOPE)

# Crypto provider selection. Prefer external crypto providers. If
# implicit selection is allowed, use native only when no other options
# are available or when explicitly requested. Allowing native as a
# fallback can be disabled using the ALLOW_CRYPTO_NATIVE option.
list(APPEND CRYPTO_PKG)

set(USE_CRYPTO_GNUTLS OFF)
set(USE_CRYPTO_OPENSSL OFF)
set(USE_CRYPTO_NATIVE OFF)
set(FOUND_CRYPTO OFF)

if(USE_IMPLICIT_CRYPTO OR REQUIRE_CRYPTO_OPENSSL)
  if(EXTERNAL_LIBS)
    set(USE_CRYPTO_OPENSSL ON)
  else()
    pkg_check_modules(pc_openssl openssl>=1.1.0)
    if(pc_openssl_FOUND)
      set(USE_CRYPTO_OPENSSL ON)
      set(FOUND_CRYPTO ON)
      set(CRYPTO_PKG "${CRYPTO_PKG}, openssl >= 1.1.0")
    else()
      find_path(OPENSSL_H_PATH openssl/evp.h)
      find_library(OPENSSL_LIB_PATH crypto)
      if(OPENSSL_H_PATH AND OPENSSL_LIB_PATH)
        list(APPEND dep_include_directories ${OPENSSL_H_PATH})
        list(APPEND dep_link_libraries ${OPENSSL_LIB_PATH})
        set(USE_CRYPTO_OPENSSL ON)
        set(FOUND_CRYPTO ON)
      elseif(REQUIRE_CRYPTO_OPENSSL)
        message(SEND_ERROR "openssl not found")
        set(ANYTHING_MISSING 1)
      endif()
    endif()
  endif()
endif()
if(USE_IMPLICIT_CRYPTO OR REQUIRE_CRYPTO_GNUTLS)
  pkg_check_modules(pc_gnutls gnutls)
  if(pc_gnutls_FOUND)
    set(USE_CRYPTO_GNUTLS ON)
    set(FOUND_CRYPTO ON)
    set(CRYPTO_PKG "${CRYPTO_PKG}, gnutls")
  else()
    find_path(GNUTLS_H_PATH gnutls/gnutls.h)
    find_library(GNUTLS_LIB_PATH gnutls)
    if(GNUTLS_H_PATH AND GNUTLS_LIB_PATH)
      list(APPEND dep_include_directories ${GNUTLS_H_PATH})
      list(APPEND dep_link_libraries ${GNUTLS_LIB_PATH})
      set(USE_CRYPTO_GNUTLS ON)
      set(FOUND_CRYPTO ON)
    elseif(REQUIRE_CRYPTO_GNUTLS)
      message(SEND_ERROR "gnutls not found")
      set(ANYTHING_MISSING 1)
    endif()
  endif()
endif()
if(REQUIRE_CRYPTO_NATIVE)
  set(USE_CRYPTO_NATIVE ON)
  set(FOUND_CRYPTO ON)
elseif(USE_IMPLICIT_CRYPTO)
  if(ALLOW_CRYPTO_NATIVE AND (NOT FOUND_CRYPTO))
    set(USE_CRYPTO_NATIVE ON)
    set(FOUND_CRYPTO ON)
  endif()
endif()
if(FOUND_CRYPTO)
  if(NOT DEFAULT_CRYPTO)
    # The preferred order of crypto providers is documented in
    # manual/installation.rst in the crypto.build section.
    if(USE_CRYPTO_GNUTLS)
      set(DEFAULT_CRYPTO "gnutls")
    elseif(USE_CRYPTO_OPENSSL)
      set(DEFAULT_CRYPTO "openssl")
    else()
      set(DEFAULT_CRYPTO "native")
    endif()
  endif()
else()
  message(SEND_ERROR "no crypto provider is available")
  set(ANYTHING_MISSING 1)
endif()
if(ANYTHING_MISSING)
  message(FATAL_ERROR "Missing dependencies; unable to continue")
endif()

message(STATUS "")
message(STATUS "*** Crypto Summary ***")
message(STATUS "  GNU TLS crypto enabled: " ${USE_CRYPTO_GNUTLS})
message(STATUS "  OpenSSL crypto enabled: " ${USE_CRYPTO_OPENSSL})
message(STATUS "  Native crypto enabled: " ${USE_CRYPTO_NATIVE})
message(STATUS "  Default crypto: " ${DEFAULT_CRYPTO})
message(STATUS "")

if(USE_CRYPTO_OPENSSL)
  list(APPEND libqpdf_SOURCES ${libqpdf_crypto_openssl})
  if(NOT EXTERNAL_LIBS)
    list(APPEND dep_include_directories ${pc_openssl_INCLUDEDIR})
    list(APPEND dep_link_directories ${pc_openssl_LIBDIR})
    list(APPEND dep_link_libraries ${pc_openssl_LIBRARIES})
  endif()
endif()
if(USE_CRYPTO_GNUTLS)
  list(APPEND libqpdf_SOURCES ${libqpdf_crypto_gnutls})
  list(APPEND dep_include_directories ${pc_gnutls_INCLUDEDIR})
  list(APPEND dep_link_directories ${pc_gnutls_LIBDIR})
  list(APPEND dep_link_libraries ${pc_gnutls_LIBRARIES})
endif()
if(USE_CRYPTO_NATIVE)
  list(APPEND libqpdf_SOURCES ${libqpdf_crypto_native})
endif()

if(APPLE)
  # 2022: in CI (GitHub actions), pkg-config for zlib was adding a
  # broken directory to the include path. This effectively filters it
  # out.
  list(FILTER dep_include_directories EXCLUDE REGEX "^/Library/")
endif()

list(REMOVE_DUPLICATES dep_include_directories)
list(REMOVE_DUPLICATES dep_link_directories)
list(REMOVE_DUPLICATES dep_link_libraries)

check_type_size(size_t SIZEOF_SIZE_T)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_symbol_exists(fseeko "stdio.h" HAVE_FSEEKO)
check_symbol_exists(fseeko64 "stdio.h" HAVE_FSEEKO64)
check_symbol_exists(localtime_r "time.h" HAVE_LOCALTIME_R)
check_symbol_exists(random "stdlib.h" HAVE_RANDOM)
find_file(RANDOM_DEVICE
  "urandom" "arandom" "arandom" PATHS "/dev" NO_DEFAULT_PATH)

check_c_source_compiles(
"#include <time.h>
#include <stdio.h>
int main(int argc, char* argv[]) {
    tzset();
    printf(\"%ld\", timezone);
    return 0;
}"
    HAVE_EXTERN_LONG_TIMEZONE)

check_c_source_compiles(
"#include <time.h>
int main(int argc, char* argv[]) {
    struct tm tm;
    tm.tm_gmtoff = 1;
    return 0;
}"
    HAVE_EXTERN_TM_GMTOFF)

check_c_source_compiles(
"#include <stdio.h>
#include <sys/types.h>
int main(int argc, char* argv[]) {
    int a[sizeof(off_t) >= 8 ? 1 : -1];
}"
    LFS_WITHOUT_MACROS)

check_c_source_compiles(
"#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <sys/types.h>
int main(int argc, char* argv[]) {
    int a[sizeof(off_t) >= 8 ? 1 : -1];
}"
    LFS_WITH_MACROS)
if(LFS_WITH_MACROS AND NOT LFS_WITHOUT_MACROS)
  set(_FILE_OFFSET_BITS 64)
endif()

check_c_source_compiles(
"#include <malloc.h>
#include <stdio.h>
int main(int argc, char* argv[]) {
    malloc_info(0, stdout);
    return 0;
}"
    HAVE_MALLOC_INFO)

check_c_source_compiles(
"#include <stdio.h>
#include <stdlib.h>
int main(int argc, char* argv[]) {
    char* buf;
    size_t size;
    FILE* f;
    f = open_memstream(&buf, &size);
    fclose(f);
    free(buf);
    return 0;
}"
    HAVE_OPEN_MEMSTREAM)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/qpdf/qpdf-config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/qpdf/qpdf-config.h"
  NEWLINE_STYLE UNIX)

if(NOT BUILD_STATIC_LIBS)
  set(OBJECT_LIB_IS_PIC ON)
else()
  set(OBJECT_LIB_IS_PIC OFF)
endif()

# Build an "object library" for use in libtests so we don't have to
# export symbols that are not officially part of the public API. If we
# are building static libraries, the object library won't use
# position-independent code and will provided objects for the static
# library. If we are only building the shared library, go ahead and
# use PIC for the object library so we don't have to compile twice.
set(OBJECT_LIB libqpdf_object)
add_library(${OBJECT_LIB} OBJECT ${libqpdf_SOURCES})
if(OBJECT_LIB_IS_PIC)
  target_compile_definitions(${OBJECT_LIB} PRIVATE libqpdf_EXPORTS)
endif()
set_target_properties(${OBJECT_LIB} PROPERTIES
  POSITION_INDEPENDENT_CODE ${OBJECT_LIB_IS_PIC})
target_include_directories(${OBJECT_LIB}
  SYSTEM PRIVATE ${dep_include_directories})
target_include_directories(${OBJECT_LIB}
  PUBLIC
  ${JPEG_INCLUDE}
  ${qpdf_INCLUDE}
  ${qpdf_SOURCE_DIR}/libqpdf
  ${CMAKE_CURRENT_BINARY_DIR})
target_link_directories(${OBJECT_LIB} INTERFACE ${dep_link_directories})
target_link_libraries(${OBJECT_LIB} INTERFACE ${dep_link_libraries})
if(ATOMIC_LIBRARY)
  target_link_libraries(${OBJECT_LIB} INTERFACE ${ATOMIC_LIBRARY})
endif()

set(LD_VERSION_FLAGS "")
function(ld_version_script)
  # Check if the linker supports linker scripts, and use if it does.
  # This functionality is currently constrained to compilers using GNU
  # ld on ELF systems or systems that emulation this behavior.
  set(ld_script
    "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/qpdf-tmp/conftest.map")
  file(WRITE ${ld_script}
"VERS_1 {
        global: sym;
};

VERS_2 {
        global: sym;
} VERS_1;
")
  set(CMAKE_REQUIRED_LINK_OPTIONS -Wl,--version-script=${ld_script})
  check_c_source_compiles("int main() { return 0; }" HAVE_LD_SCRIPT)
  if(HAVE_LD_SCRIPT)
    set(LD_VERSION_FLAGS
      -Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/libqpdf.map PARENT_SCOPE)
    configure_file(
      "${qpdf_SOURCE_DIR}/libqpdf.map.in"
      "${CMAKE_CURRENT_BINARY_DIR}/libqpdf.map"
      NEWLINE_STYLE UNIX)
  endif()
endfunction()
if(NOT WIN32)
  ld_version_script()
endif()

if(BUILD_SHARED_LIBS)
  set(SHARED_LIB libqpdf)
  if(OBJECT_LIB_IS_PIC)
    add_library(${SHARED_LIB} SHARED $<TARGET_OBJECTS:libqpdf_object>)
  else()
    add_library(${SHARED_LIB} SHARED ${libqpdf_SOURCES})
  endif()
  if(WIN32)
    # Goal: the DLL import library should be libqpdf.a or qpdf.lib so
    # that linking with -lqpdf gets you a shared library link on all
    # platforms. The DLL should be qpdf${SONAME}.dll rather than just
    # qpdf.dll. qpdf has always done this, and it gives us some
    # protection against binary incompatible DLLs being installed.
    set(SHARED_OUT qpdf${qpdf_SOVERSION})  # Put API version number in DLL
    if(MINGW)
      # Reference: Platform/Windows-GNU.cmake in the cmake installation
      set(CMAKE_SHARED_LIBRARY_PREFIX "")    # libqpdf$v.dll -> qpdf$v.dll
      set(CMAKE_IMPORT_LIBRARY_SUFFIX ".a")  # libqpdf.dll.a -> libqpdf.a
      # Ensure the DLLs are striped in Release mode.
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
    endif()
    if(MSVC)
      # Avoid linker warning from mixing libraries built with /MT and /MD.
      set_target_properties(${SHARED_LIB}
        PROPERTIES LINK_FLAGS "/NODEFAULTLIB:LIBCMT /NODEFAULTLIB:LIBCMTD")
    endif()
  else()
    set(SHARED_OUT qpdf)
  endif()
  # Setting OUTPUT_NAME and ARCHIVE_OUTPUT_NAME separate enables us to
  # have a versioned DLL and an unversioned import library, which
  # gives us semantics similar to ELF shared libraries and makes
  # linking against qpdf the same across all platforms.
  set_target_properties(${SHARED_LIB} PROPERTIES
    OUTPUT_NAME ${SHARED_OUT}
    ARCHIVE_OUTPUT_NAME qpdf
    VERSION ${qpdf_LIBVERSION}
    SOVERSION ${qpdf_SOVERSION}
    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})

  target_compile_definitions(${SHARED_LIB} PRIVATE libqpdf_EXPORTS)
  target_include_directories(${SHARED_LIB}
    SYSTEM PRIVATE ${dep_include_directories})
  target_include_directories(${SHARED_LIB}
    PUBLIC
    $<BUILD_INTERFACE:${qpdf_INCLUDE}>
    $<INSTALL_INTERFACE:include>)
  target_link_directories(${SHARED_LIB} PRIVATE ${dep_link_directories})
  target_link_libraries(${SHARED_LIB} PRIVATE ${dep_link_libraries})
  if(ATOMIC_LIBRARY)
    target_link_libraries(${SHARED_LIB} PRIVATE ${ATOMIC_LIBRARY})
  endif()
  if(LD_VERSION_FLAGS)
    target_link_options(${SHARED_LIB} PRIVATE ${LD_VERSION_FLAGS})
  endif()

  target_include_directories(${SHARED_LIB}
    PRIVATE ${qpdf_SOURCE_DIR}/libqpdf ${CMAKE_CURRENT_BINARY_DIR})

  install(TARGETS ${SHARED_LIB}
    EXPORT libqpdfTargets
    TYPE LIBRARY
    COMPONENT ${COMPONENT_LIB}
    NAMELINK_COMPONENT ${COMPONENT_DEV}
    INCLUDES ${qpdf_INCLUDE})
endif()

if(BUILD_STATIC_LIBS)
  if(BUILD_SHARED_LIBS)
    set(STATIC_LIB libqpdf_static)
  else()
    set(STATIC_LIB libqpdf)
  endif()
  if(OBJECT_LIB_IS_PIC)
    add_library(${STATIC_LIB} STATIC ${libqpdf_SOURCES})
  else()
    add_library(${STATIC_LIB} STATIC $<TARGET_OBJECTS:libqpdf_object>)
  endif()

  target_include_directories(${STATIC_LIB}
    SYSTEM PRIVATE ${dep_include_directories})
  target_include_directories(${STATIC_LIB}
    PUBLIC
    $<BUILD_INTERFACE:${qpdf_INCLUDE}>
    $<INSTALL_INTERFACE:include>)
  target_link_directories(${STATIC_LIB}
    INTERFACE $<BUILD_INTERFACE:${dep_link_directories}>
    PRIVATE $<INSTALL_INTERFACE:${dep_link_directories}>)
  target_link_libraries(${STATIC_LIB} INTERFACE ${dep_link_libraries})
  if(ATOMIC_LIBRARY)
    target_link_libraries(${STATIC_LIB} INTERFACE ${ATOMIC_LIBRARY})
  endif()

  # Avoid name clashes on Windows with the the DLL import library.
  if(NOT DEFINED STATIC_SUFFIX AND BUILD_SHARED_LIBS)
    if (WIN32)
      set(STATIC_SUFFIX "_static")
    else()
      set(STATIC_SUFFIX "")
    endif()
  endif()

  set_target_properties(${STATIC_LIB} PROPERTIES
    OUTPUT_NAME qpdf${STATIC_SUFFIX}
    VERSION ${PROJECT_VERSION})
  target_include_directories(${STATIC_LIB}
    PRIVATE ${qpdf_SOURCE_DIR}/libqpdf ${CMAKE_CURRENT_BINARY_DIR})

  install(TARGETS ${STATIC_LIB}
    EXPORT libqpdfTargets
    TYPE ARCHIVE
    COMPONENT ${COMPONENT_DEV}
    INCLUDES ${qpdf_INCLUDE})
endif()

configure_file(
  "${qpdf_SOURCE_DIR}/libqpdf.pc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/libqpdf.pc"
  @ONLY NEWLINE_STYLE UNIX)
if(INSTALL_PKGCONFIG)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libqpdf.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT ${COMPONENT_DEV})
endif()

if(INSTALL_CMAKE_PACKAGE)
  include(CMakePackageConfigHelpers)
  configure_package_config_file(
    ${qpdf_SOURCE_DIR}/qpdfConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/qpdfConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qpdf)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/qpdfConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
  install(EXPORT libqpdfTargets
    NAMESPACE qpdf::
    FILE libqpdfTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qpdf)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/qpdfConfigVersion.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/qpdfConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qpdf)
endif()

if(CHECK_SIZES AND BUILD_SHARED_LIBS AND (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  # We can only do this check on a system with ELF shared libraries.
  # Since this is a maintainer-only option, testing for Linux is a
  # close enough approximation.
  add_test(
    NAME check-sizes
    COMMAND ${qpdf_SOURCE_DIR}/check_abi check-sizes
    --lib $<TARGET_FILE:libqpdf>)
endif()
