/*
 * Include this file to use assert in CI code for. This will allow
 * the use of assert and ensure that NDEBUG is undefined (which
 * would cause spurious CI passes).
 */

#ifndef QPDF_ASSERT_TEST_H
#define QPDF_ASSERT_TEST_H

#ifdef NDEBUG
# undef NDEBUG
#endif
#include <assert.h>

#endif /* QPDF_ASSERT_H */
