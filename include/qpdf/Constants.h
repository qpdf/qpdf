/* Copyright (c) 2005-2015 Jay Berkenbilt
 *
 * This file is part of qpdf.  This software may be distributed under
 * the terms of version 2 of the Artistic License which may be found
 * in the source distribution.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __QPDFCONSTANTS_H__
#define __QPDFCONSTANTS_H__

/* Keep this file 'C' compatible so it can be used from the C and C++
 * interfaces.
 */

/* Error Codes */

enum qpdf_error_code_e
{
    qpdf_e_success = 0,
    qpdf_e_internal,	 	/* logic/programming error -- indicates bug */
    qpdf_e_system,		/* I/O error, memory error, etc. */
    qpdf_e_unsupported,		/* PDF feature not (yet) supported by qpdf */
    qpdf_e_password,		/* incorrect password for encrypted file */
    qpdf_e_damaged_pdf,		/* syntax errors or other damage in PDF */
    qpdf_e_pages,               /* erroneous or unsupported pages structure */
};

/* Write Parameters */

enum qpdf_object_stream_e
{
    qpdf_o_disable = 0,		/* disable object streams */
    qpdf_o_preserve,		/* preserve object streams */
    qpdf_o_generate		/* generate object streams */
};
enum qpdf_stream_data_e
{
    qpdf_s_uncompress = 0,	/* uncompress stream data */
    qpdf_s_preserve,		/* preserve stream data compression */
    qpdf_s_compress		/* compress stream data */
};

/* R3 Encryption Parameters */

enum qpdf_r3_print_e
{
    qpdf_r3p_full = 0,		/* allow all printing */
    qpdf_r3p_low,		/* allow only low-resolution printing */
    qpdf_r3p_none		/* allow no printing */
};
enum qpdf_r3_modify_e		/* Allowed changes: */
{
    qpdf_r3m_all = 0,		/* General editing, comments, forms */
    qpdf_r3m_annotate,	        /* Comments, form field fill-in, and signing */
    qpdf_r3m_form,		/* form field fill-in and signing */
    qpdf_r3m_assembly,		/* only document assembly */
    qpdf_r3m_none		/* no modifications */
};

#endif /* __QPDFCONSTANTS_H__ */
