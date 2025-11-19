/*
 * Include this file to use assert in regular code for
 * debugging/strong sanity checking, knowing that the assert will be
 * disabled in release code. Use qpdf_assert_debug in the code.
 */

#ifndef QPDF_ASSERT_DEBUG_H
#define QPDF_ASSERT_DEBUG_H

#include <cassert>
#define qpdf_assert_debug assert
// Alias for assert. Pre-condition is only enforced in debug builds.
#define qpdf_expect assert
// Alias for assert. Post-condition is only enforced in debug builds.
#define qpdf_ensures assert
// Alias for assert. Invariant is only enforced in debug builds.
#define qpdf_invariant assert
// Alias for static_assert.
#define qpdf_static_expect static_assert

#endif /* QPDF_ASSERT_DEBUG_H */
