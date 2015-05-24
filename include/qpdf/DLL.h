/* Copyright (c) 2005-2015 Jay Berkenbilt
 *
 * This file is part of qpdf.  This software may be distributed under
 * the terms of version 2 of the Artistic License which may be found
 * in the source distribution.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __QPDF_DLL_HH__
#define __QPDF_DLL_HH__

#if defined(_WIN32) && defined(DLL_EXPORT)
# define QPDF_DLL __declspec(dllexport)
#else
# define QPDF_DLL
#endif

#endif /* __QPDF_DLL_HH__ */
