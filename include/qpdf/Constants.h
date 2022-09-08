/* Copyright (c) 2005-2022 Jay Berkenbilt
 *
 * This file is part of qpdf.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Versions of qpdf prior to version 7 were released under the terms
 * of version 2.0 of the Artistic License. At your option, you may
 * continue to consider qpdf to be licensed under those terms. Please
 * see the manual for additional information.
 */

#ifndef QPDFCONSTANTS_H
#define QPDFCONSTANTS_H

/*
 * REMEMBER:
 *
 * Keep this file 'C' compatible so it can be used from the C and C++
 * interfaces.
 */

/* ******************************  NOTE  ******************************

Tl;Dr: new values must be added to the end such that no constant's
numerical value changes, even across major releases.

Details:

As new values are added to existing enumerated types in this file,
it is important not to change the actual values of any constants.
This means that, in the absence of explicit assignment of values,
the order of entries can't change even across major releases. Why?
Here are the reasons:

* Many of these constants are used by the C API. The C API is used
  through foreign function call interfaces by users of other languages
  who may not have access to or the ability to parse a C header file.
  As such, users are likely to hard-code numerical values or create
  their own constants whose values match. If we change values here,
  their code would break, and there would be no way to detect it short
  of noticing a bug. Furthermore, it would be difficult to write code
  that properly handled more than one version of the qpdf shared
  object (e.g. DLL) since the information about what version of qpdf
  is involved is only available at runtime.

- It has happened from time to time that a user builds an application
  with an incorrectly installed qpdf, such as having mismatched header
  files and library files. In the event that they are only using qpdf
  interfaces that have been stable across the versions in question,
  this turns out to be harmless. If they happen to use non-compatible
  interfaces, this results usually in a failure to load or an obvious
  runtime error. If we change values of constants, it is possible that
  code that links and runs may have mismatched values for constants.
  This would create a bug that would be extremely difficult to track
  down and impossible for qpdf maintainers to reproduce.

*/

/* Exit Codes from QPDFJob and the qpdf CLI */

enum qpdf_exit_code_e {
    qpdf_exit_success = 0,
    /* Normal exit codes */
    qpdf_exit_error = 2,
    qpdf_exit_warning = 3,
    /* For --is-encrypted and --requires-password */
    qpdf_exit_is_not_encrypted = 2,
    qpdf_exit_correct_password = 3,
};

/* Error Codes */

enum qpdf_error_code_e {
    qpdf_e_success = 0,
    qpdf_e_internal,    /* logic/programming error -- indicates bug */
    qpdf_e_system,      /* I/O error, memory error, etc. */
    qpdf_e_unsupported, /* PDF feature not (yet) supported by qpdf */
    qpdf_e_password,    /* incorrect password for encrypted file */
    qpdf_e_damaged_pdf, /* syntax errors or other damage in PDF */
    qpdf_e_pages,       /* erroneous or unsupported pages structure */
    qpdf_e_object,      /* type/bounds errors accessing objects */
    qpdf_e_json,        /* error in qpdf JSON */
};

/* Object Types */

/* PDF objects represented by QPDFObjectHandle or, in the C API, by
 * qpdf_oh, have a unique type code that has one of the values in the
 * list below. As new object types are added to qpdf, additional items
 * may be added to the list, so code that switches on these values
 * should take that into consideration. (Maintainer note: it would be
 * better to call this qpdf_ot_* rather than ot_* to reduce likelihood
 * of name collision, but changing the names of the values breaks
 * backward compatibility.)
 */
enum qpdf_object_type_e {
    /* Object types internal to qpdf */
    ot_uninitialized,
    ot_reserved,
    /* Object types that can occur in the main document */
    ot_null,
    ot_boolean,
    ot_integer,
    ot_real,
    ot_string,
    ot_name,
    ot_array,
    ot_dictionary,
    ot_stream,
    /* Additional object types that can occur in content streams */
    ot_operator,
    ot_inlineimage,
    /* Object types internal to qpdf */
    ot_unresolved,
    ot_destroyed,
};

/* Write Parameters. See QPDFWriter.hh for details. */

enum qpdf_object_stream_e {
    qpdf_o_disable = 0, /* disable object streams */
    qpdf_o_preserve,    /* preserve object streams */
    qpdf_o_generate     /* generate object streams */
};
enum qpdf_stream_data_e {
    qpdf_s_uncompress = 0, /* uncompress stream data */
    qpdf_s_preserve,       /* preserve stream data compression */
    qpdf_s_compress        /* compress stream data */
};

/* Stream data flags */

/* See pipeStreamData in QPDFObjectHandle.hh for details on these flags. */
enum qpdf_stream_encode_flags_e {
    qpdf_ef_compress = 1 << 0,  /* compress uncompressed streams */
    qpdf_ef_normalize = 1 << 1, /* normalize content stream */
};
enum qpdf_stream_decode_level_e {
    /* These must be in order from less to more decoding. */
    qpdf_dl_none = 0,    /* preserve all stream filters */
    qpdf_dl_generalized, /* decode general-purpose filters */
    qpdf_dl_specialized, /* also decode other non-lossy filters */
    qpdf_dl_all          /* also decode lossy filters */
};
/* For JSON encoding */
enum qpdf_json_stream_data_e {
    qpdf_sj_none = 0,
    qpdf_sj_inline,
    qpdf_sj_file,
};

/* R3 Encryption Parameters */

enum qpdf_r3_print_e {
    qpdf_r3p_full = 0, /* allow all printing */
    qpdf_r3p_low,      /* allow only low-resolution printing */
    qpdf_r3p_none      /* allow no printing */
};

/* qpdf_r3_modify_e doesn't allow the full flexibility of the spec. It
 * corresponds to options in Acrobat 5's menus. The new interface in
 * QPDFWriter offers more granularity and no longer uses this type.
 */
enum qpdf_r3_modify_e /* Allowed changes: */
{
    qpdf_r3m_all = 0,  /* All editing */
    qpdf_r3m_annotate, /* Comments, fill forms, signing, assembly */
    qpdf_r3m_form,     /* Fill forms, signing, assembly */
    qpdf_r3m_assembly, /* Only document assembly */
    qpdf_r3m_none      /* No modifications */
};

/* Form field flags from the PDF spec */

enum pdf_form_field_flag_e {
    /* flags that apply to all form fields */
    ff_all_read_only = 1 << 0,
    ff_all_required = 1 << 1,
    ff_all_no_export = 1 << 2,

    /* flags that apply to fields of type /Btn (button) */
    ff_btn_no_toggle_off = 1 << 14,
    ff_btn_radio = 1 << 15,
    ff_btn_pushbutton = 1 << 16,
    ff_btn_radios_in_unison = 1 << 17,

    /* flags that apply to fields of type /Tx (text) */
    ff_tx_multiline = 1 << 12,
    ff_tx_password = 1 << 13,
    ff_tx_file_select = 1 << 20,
    ff_tx_do_not_spell_check = 1 << 22,
    ff_tx_do_not_scroll = 1 << 23,
    ff_tx_comb = 1 << 24,
    ff_tx_rich_text = 1 << 25,

    /* flags that apply to fields of type /Ch (choice) */
    ff_ch_combo = 1 << 17,
    ff_ch_edit = 1 << 18,
    ff_ch_sort = 1 << 19,
    ff_ch_multi_select = 1 << 21,
    ff_ch_do_not_spell_check = 1 << 22,
    ff_ch_commit_on_sel_change = 1 << 26
};

/* Annotation flags from the PDF spec */

enum pdf_annotation_flag_e {
    an_invisible = 1 << 0,
    an_hidden = 1 << 1,
    an_print = 1 << 2,
    an_no_zoom = 1 << 3,
    an_no_rotate = 1 << 4,
    an_no_view = 1 << 5,
    an_read_only = 1 << 6,
    an_locked = 1 << 7,
    an_toggle_no_view = 1 << 8,
    an_locked_contents = 1 << 9
};

/* Encryption/password status for QPDFJob */
enum qpdf_encryption_status_e {
    qpdf_es_encrypted = 1 << 0,
    qpdf_es_password_incorrect = 1 << 1
};

#endif /* QPDFCONSTANTS_H */
