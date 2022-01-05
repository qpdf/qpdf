// Copyright (c) 2005-2021 Jay Berkenbilt
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Versions of qpdf prior to version 7 were released under the terms
// of version 2.0 of the Artistic License. At your option, you may
// continue to consider qpdf to be licensed under those terms. Please
// see the manual for additional information.

#ifndef QPDFJOB_HH
#define QPDFJOB_HH

#include <qpdf/DLL.h>
#include <qpdf/PointerHolder.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/Constants.h>

#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <functional>

class QPDFWriter;

class QPDFJob
{
  public:
    QPDF_DLL
    QPDFJob();

    // Set name that is used to prefix verbose messages, progress
    // messages, and other things that the library writes to output
    // and error streams on the caller's behalf. Defaults to "qpdf".
    QPDF_DLL
    void setMessagePrefix(std::string const&);

    // Override streams that errors and output go to. Defaults are
    // std::cout and std::cerr. Pass nullptr to use the default.
    QPDF_DLL
    void setOutputStreams(std::ostream* out_stream, std::ostream* err_stream);

    // If in verbose mode, call the given function, passing in the
    // output stream and message prefix.
    QPDF_DLL
    void doIfVerbose(
        std::function<void(std::ostream&, std::string const& prefix)> fn);

    QPDF_DLL
    void run();

    QPDF_DLL
    bool hasWarnings();

    QPDF_DLL
    bool createsOutput();

    QPDF_DLL
    bool suppressWarnings();

    QPDF_DLL
    bool checkRequiresPassword();

    QPDF_DLL
    bool checkIsEncrypted();

    // Return value is bitwise OR of values from qpdf_encryption_status_e
    QPDF_DLL
    unsigned long getEncryptionStatus();

    // QXXXQ From here to END-PUBLIC should all be private or
    // different somehow
  public:
    QPDF_DLL
    static JSON json_schema(std::set<std::string>* keys = 0);
    QPDF_DLL
    static void parse_object_id(
        std::string const& objspec, bool& trailer, int& obj, int& gen);

    struct PageSpec
    {
        PageSpec(std::string const& filename,
                 char const* password,
                 char const* range) :
            filename(filename),
            password(password),
            range(range)
        {
        }

        std::string filename;
        char const* password;
        char const* range;
    };

    struct RotationSpec
    {
        RotationSpec(int angle = 0, bool relative = false) :
            angle(angle),
            relative(relative)
        {
        }

        int angle;
        bool relative;
    };

    enum password_mode_e { pm_bytes, pm_hex_bytes, pm_unicode, pm_auto };

    struct UnderOverlay
    {
        UnderOverlay(char const* which) :
            which(which),
            filename(0),
            password(0),
            to_nr("1-z"),
            from_nr("1-z"),
            repeat_nr("")
        {
        }

        std::string which;
        char const* filename;
        char const* password;
        char const* to_nr;
        char const* from_nr;
        char const* repeat_nr;
        PointerHolder<QPDF> pdf;
        std::vector<int> to_pagenos;
        std::vector<int> from_pagenos;
        std::vector<int> repeat_pagenos;
    };

    struct AddAttachment
    {
        AddAttachment() :
            replace(false)
        {
        }

        std::string path;
        std::string key;
        std::string filename;
        std::string creationdate;
        std::string moddate;
        std::string mimetype;
        std::string description;
        bool replace;
    };

    struct CopyAttachmentFrom
    {
        std::string path;
        std::string password;
        std::string prefix;
    };

    PointerHolder<QPDF> processFile(
        char const* filename, char const* password);
    void validateUnderOverlay(QPDF& pdf, QPDFJob::UnderOverlay* uo);
    void handleUnderOverlay(QPDF& pdf);
    void copyAttachments(QPDF& pdf);
    void handleTransformations(QPDF& pdf);
    void addAttachments(QPDF& pdf);
    void setWriterOptions(QPDF& pdf, QPDFWriter& w);
    void doSplitPages(QPDF& pdf, bool& warnings);
    void writeOutfile(QPDF& pdf);
    void doJSON(QPDF& pdf);
    void doInspection(QPDF& pdf);
    void showEncryption(QPDF& pdf);
    void doCheck(QPDF& pdf);
    void doShowObj(QPDF& pdf);
    void doShowPages(QPDF& pdf);
    void doListAttachments(QPDF& pdf);
    void setEncryptionOptions(QPDF&, QPDFWriter&);
    void maybeFixWritePassword(int R, std::string& password);

    enum remove_unref_e { re_auto, re_yes, re_no };

    char const* password;
    std::shared_ptr<char> password_alloc;
    bool linearize;
    bool decrypt;
    int split_pages;
    bool verbose;
    bool progress;
    bool suppress_warnings;
    bool copy_encryption;
    char const* encryption_file;
    char const* encryption_file_password;
    bool encrypt;
    bool password_is_hex_key;
    bool suppress_password_recovery;
    password_mode_e password_mode;
    bool allow_insecure;
    bool allow_weak_crypto;
    std::string user_password;
    std::string owner_password;
    int keylen;
    bool r2_print;
    bool r2_modify;
    bool r2_extract;
    bool r2_annotate;
    bool r3_accessibility;
    bool r3_extract;
    bool r3_assemble;
    bool r3_annotate_and_form;
    bool r3_form_filling;
    bool r3_modify_other;
    qpdf_r3_print_e r3_print;
    bool force_V4;
    bool force_R5;
    bool cleartext_metadata;
    bool use_aes;
    bool stream_data_set;
    qpdf_stream_data_e stream_data_mode;
    bool compress_streams;
    bool compress_streams_set;
    bool recompress_flate;
    bool recompress_flate_set;
    int compression_level;
    qpdf_stream_decode_level_e decode_level;
    bool decode_level_set;
    bool normalize_set;
    bool normalize;
    bool suppress_recovery;
    bool object_stream_set;
    qpdf_object_stream_e object_stream_mode;
    bool ignore_xref_streams;
    bool qdf_mode;
    bool preserve_unreferenced_objects;
    remove_unref_e remove_unreferenced_page_resources;
    bool keep_files_open;
    bool keep_files_open_set;
    size_t keep_files_open_threshold;
    bool newline_before_endstream;
    std::string linearize_pass1;
    bool coalesce_contents;
    bool flatten_annotations;
    int flatten_annotations_required;
    int flatten_annotations_forbidden;
    bool generate_appearances;
    std::string min_version;
    std::string force_version;
    bool show_npages;
    bool deterministic_id;
    bool static_id;
    bool static_aes_iv;
    bool suppress_original_object_id;
    bool show_encryption;
    bool show_encryption_key;
    bool check_linearization;
    bool show_linearization;
    bool show_xref;
    bool show_trailer;
    int show_obj;
    int show_gen;
    bool show_raw_stream_data;
    bool show_filtered_stream_data;
    bool show_pages;
    bool show_page_images;
    size_t collate;
    bool flatten_rotation;
    bool list_attachments;
    std::string attachment_to_show;
    std::list<std::string> attachments_to_remove;
    std::list<AddAttachment> attachments_to_add;
    std::list<CopyAttachmentFrom> attachments_to_copy;
    bool json;
    std::set<std::string> json_keys;
    std::set<std::string> json_objects;
    bool check;
    bool optimize_images;
    bool externalize_inline_images;
    bool keep_inline_images;
    bool remove_page_labels;
    size_t oi_min_width;
    size_t oi_min_height;
    size_t oi_min_area;
    size_t ii_min_bytes;
    UnderOverlay underlay;
    UnderOverlay overlay;
    UnderOverlay* under_overlay;
    std::vector<PageSpec> page_specs;
    std::map<std::string, RotationSpec> rotations;
    bool require_outfile;
    bool replace_input;
    bool check_is_encrypted;
    bool check_requires_password;
    char const* infilename;
    char const* outfilename;
    // QXXXQ END-PUBLIC

  private:
    class Members
    {
        friend class QPDFJob;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members();
        Members(Members const&) = delete;

        std::string message_prefix;
        bool warnings;
        bool creates_output;
        std::ostream* cout;
        std::ostream* cerr;
        unsigned long encryption_status;
    };
    PointerHolder<Members> m;
};

#endif // QPDFOBJECT_HH
