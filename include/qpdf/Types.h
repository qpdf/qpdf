#ifndef __QPDFTYPES_H__
#define __QPDFTYPES_H__

/* Provide an offset type that should be as big as off_t on just about
 * any system.  If your compiler doesn't support C99 (or at least the
 * "long long" type), then you may have to modify this definition.
 */

typedef long long int qpdf_offset_t;

#endif /* __QPDFTYPES_H__ */
