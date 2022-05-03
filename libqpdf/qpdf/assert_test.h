/*
 * Include this file to use assert in regular code for
 * debugging/strong sanity checking, knowing that the assert will be
 * disabled in release code. Use qpdf_debug_assert in the code.
 */

/* assert_debug and assert_test intentionally use the same
 * guard. Search for assert in README-MAINTAINER.
 */
#ifdef QPDF_ASSERT_H
# error "At most one qpdf/assert header may be included at most one time"
#else
# define QPDF_ASSERT_H

# ifdef NDEBUG
#  undef NDEBUG
# endif
# include <assert.h>

#endif /* QPDF_ASSERT_H */
