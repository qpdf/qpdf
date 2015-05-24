/* Copyright (c) 2005-2015 Jay Berkenbilt
 *
 * This file is part of qpdf.  This software may be distributed under
 * the terms of version 2 of the Artistic License which may be found
 * in the source distribution.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __QPDFTYPES_H__
#define __QPDFTYPES_H__

/* Provide an offset type that should be as big as off_t on just about
 * any system.  If your compiler doesn't support C99 (or at least the
 * "long long" type), then you may have to modify this definition.
 */

typedef long long int qpdf_offset_t;

#endif /* __QPDFTYPES_H__ */
