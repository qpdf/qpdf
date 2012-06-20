#ifndef __QPDFTYPES_H__
#define __QPDFTYPES_H__

/* Attempt to provide off_t and size_t on any recent platform.  To
 * make cross compilation easier and to be more portable across
 * platforms, QPDF avoids having any public header files use the
 * results of autoconf testing, so we have to handle this ourselves in
 * a static way.
 */

#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#endif /* __QPDFTYPES_H__ */
