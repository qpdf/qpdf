/*
 * Include this file to use assert in regular code for
 * debugging/strong sanity checking, knowing that the assert will be
 * disabled in release code. Use qpdf_assert_debug in the code.
 */

/* assert_debug and assert_test intentionally use the same
 * guard. Search for assert in README-MAINTAINER.
 */
#ifdef QPDF_ASSERT_H
# error "At most one qpdf/assert header may be included at most one time"
#else
# define QPDF_ASSERT_H

# include <assert.h>
# define qpdf_assert_debug assert

#endif /* QPDF_ASSERT_H */
