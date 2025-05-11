#include <qpdf/assert_debug.h>

#include <qpdf/qpdf-config.h> // include early for large file support

#include <qpdf/QPDFWriter_private.hh>

#include <qpdf/MD5.hh>
#include <qpdf/Pl_AES_PDF.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_MD5.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_RC4.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/RC4.hh>
#include <qpdf/Util.hh>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

using namespace std::literals;
using namespace qpdf;

QPDFWriter::ProgressReporter::~ProgressReporter() // NOLINT (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
}

QPDFWriter::FunctionProgressReporter::FunctionProgressReporter(std::function<void(int)> handler) :
    handler(handler)
{
}

QPDFWriter::FunctionProgressReporter::~FunctionProgressReporter() // NOLINT
                                                                  // (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
}

void
QPDFWriter::FunctionProgressReporter::reportProgress(int progress)
{
    this->handler(progress);
}

QPDFWriter::Members::Members(QPDF& pdf) :
    pdf(pdf),
    root_og(pdf.getRoot().getObjGen().isIndirect() ? pdf.getRoot().getObjGen() : QPDFObjGen(-1, 0))
{
}

QPDFWriter::Members::~Members()
{
    if (file && close_file) {
        fclose(file);
    }
    delete output_buffer;
}

QPDFWriter::QPDFWriter(QPDF& pdf) :
    m(new Members(pdf))
{
}

QPDFWriter::QPDFWriter(QPDF& pdf, char const* filename) :
    m(new Members(pdf))
{
    setOutputFilename(filename);
}

QPDFWriter::QPDFWriter(QPDF& pdf, char const* description, FILE* file, bool close_file) :
    m(new Members(pdf))
{
    setOutputFile(description, file, close_file);
}

void
QPDFWriter::setOutputFilename(char const* filename)
{
    char const* description = filename;
    FILE* f = nullptr;
    bool close_file = false;
    if (filename == nullptr) {
        description = "standard output";
        QTC::TC("qpdf", "QPDFWriter write to stdout");
        f = stdout;
        QUtil::binary_stdout();
    } else {
        QTC::TC("qpdf", "QPDFWriter write to file");
        f = QUtil::safe_fopen(filename, "wb+");
        close_file = true;
    }
    setOutputFile(description, f, close_file);
}

void
QPDFWriter::setOutputFile(char const* description, FILE* file, bool close_file)
{
    m->filename = description;
    m->file = file;
    m->close_file = close_file;
    std::shared_ptr<Pipeline> p = std::make_shared<Pl_StdioFile>("qpdf output", file);
    m->to_delete.push_back(p);
    initializePipelineStack(p.get());
}

void
QPDFWriter::setOutputMemory()
{
    m->filename = "memory buffer";
    m->buffer_pipeline = new Pl_Buffer("qpdf output");
    m->to_delete.push_back(std::shared_ptr<Pipeline>(m->buffer_pipeline));
    initializePipelineStack(m->buffer_pipeline);
}

Buffer*
QPDFWriter::getBuffer()
{
    Buffer* result = m->output_buffer;
    m->output_buffer = nullptr;
    return result;
}

std::shared_ptr<Buffer>
QPDFWriter::getBufferSharedPointer()
{
    return std::shared_ptr<Buffer>(getBuffer());
}

void
QPDFWriter::setOutputPipeline(Pipeline* p)
{
    m->filename = "custom pipeline";
    initializePipelineStack(p);
}

void
QPDFWriter::setObjectStreamMode(qpdf_object_stream_e mode)
{
    m->object_stream_mode = mode;
}

void
QPDFWriter::setStreamDataMode(qpdf_stream_data_e mode)
{
    switch (mode) {
    case qpdf_s_uncompress:
        m->stream_decode_level = std::max(qpdf_dl_generalized, m->stream_decode_level);
        m->compress_streams = false;
        break;

    case qpdf_s_preserve:
        m->stream_decode_level = qpdf_dl_none;
        m->compress_streams = false;
        break;

    case qpdf_s_compress:
        m->stream_decode_level = std::max(qpdf_dl_generalized, m->stream_decode_level);
        m->compress_streams = true;
        break;
    }
    m->stream_decode_level_set = true;
    m->compress_streams_set = true;
}

void
QPDFWriter::setCompressStreams(bool val)
{
    m->compress_streams = val;
    m->compress_streams_set = true;
}

void
QPDFWriter::setDecodeLevel(qpdf_stream_decode_level_e val)
{
    m->stream_decode_level = val;
    m->stream_decode_level_set = true;
}

void
QPDFWriter::setRecompressFlate(bool val)
{
    m->recompress_flate = val;
}

void
QPDFWriter::setContentNormalization(bool val)
{
    m->normalize_content_set = true;
    m->normalize_content = val;
}

void
QPDFWriter::setQDFMode(bool val)
{
    m->qdf_mode = val;
}

void
QPDFWriter::setPreserveUnreferencedObjects(bool val)
{
    m->preserve_unreferenced_objects = val;
}

void
QPDFWriter::setNewlineBeforeEndstream(bool val)
{
    m->newline_before_endstream = val;
}

void
QPDFWriter::setMinimumPDFVersion(std::string const& version, int extension_level)
{
    bool set_version = false;
    bool set_extension_level = false;
    if (m->min_pdf_version.empty()) {
        set_version = true;
        set_extension_level = true;
    } else {
        int old_major = 0;
        int old_minor = 0;
        int min_major = 0;
        int min_minor = 0;
        parseVersion(version, old_major, old_minor);
        parseVersion(m->min_pdf_version, min_major, min_minor);
        int compare = compareVersions(old_major, old_minor, min_major, min_minor);
        if (compare > 0) {
            QTC::TC("qpdf", "QPDFWriter increasing minimum version", extension_level == 0 ? 0 : 1);
            set_version = true;
            set_extension_level = true;
        } else if (compare == 0) {
            if (extension_level > m->min_extension_level) {
                QTC::TC("qpdf", "QPDFWriter increasing extension level");
                set_extension_level = true;
            }
        }
    }

    if (set_version) {
        m->min_pdf_version = version;
    }
    if (set_extension_level) {
        m->min_extension_level = extension_level;
    }
}

void
QPDFWriter::setMinimumPDFVersion(PDFVersion const& v)
{
    std::string version;
    int extension_level;
    v.getVersion(version, extension_level);
    setMinimumPDFVersion(version, extension_level);
}

void
QPDFWriter::forcePDFVersion(std::string const& version, int extension_level)
{
    m->forced_pdf_version = version;
    m->forced_extension_level = extension_level;
}

void
QPDFWriter::setExtraHeaderText(std::string const& text)
{
    m->extra_header_text = text;
    if ((m->extra_header_text.length() > 0) && (*(m->extra_header_text.rbegin()) != '\n')) {
        QTC::TC("qpdf", "QPDFWriter extra header text add newline");
        m->extra_header_text += "\n";
    } else {
        QTC::TC("qpdf", "QPDFWriter extra header text no newline");
    }
}

void
QPDFWriter::setStaticID(bool val)
{
    m->static_id = val;
}

void
QPDFWriter::setDeterministicID(bool val)
{
    m->deterministic_id = val;
}

void
QPDFWriter::setStaticAesIV(bool val)
{
    if (val) {
        Pl_AES_PDF::useStaticIV();
    }
}

void
QPDFWriter::setSuppressOriginalObjectIDs(bool val)
{
    m->suppress_original_object_ids = val;
}

void
QPDFWriter::setPreserveEncryption(bool val)
{
    m->preserve_encryption = val;
}

void
QPDFWriter::setLinearization(bool val)
{
    m->linearized = val;
    if (val) {
        m->pclm = false;
    }
}

void
QPDFWriter::setLinearizationPass1Filename(std::string const& filename)
{
    m->lin_pass1_filename = filename;
}

void
QPDFWriter::setPCLm(bool val)
{
    m->pclm = val;
    if (val) {
        m->linearized = false;
    }
}

void
QPDFWriter::setR2EncryptionParametersInsecure(
    char const* user_password,
    char const* owner_password,
    bool allow_print,
    bool allow_modify,
    bool allow_extract,
    bool allow_annotate)
{
    std::set<int> clear;
    if (!allow_print) {
        clear.insert(3);
    }
    if (!allow_modify) {
        clear.insert(4);
    }
    if (!allow_extract) {
        clear.insert(5);
    }
    if (!allow_annotate) {
        clear.insert(6);
    }

    setEncryptionParameters(user_password, owner_password, 1, 2, 5, clear);
}

void
QPDFWriter::setR3EncryptionParametersInsecure(
    char const* user_password,
    char const* owner_password,
    bool allow_accessibility,
    bool allow_extract,
    bool allow_assemble,
    bool allow_annotate_and_form,
    bool allow_form_filling,
    bool allow_modify_other,
    qpdf_r3_print_e print)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
        clear,
        user_password,
        owner_password,
        allow_accessibility,
        allow_extract,
        allow_assemble,
        allow_annotate_and_form,
        allow_form_filling,
        allow_modify_other,
        print,
        qpdf_r3m_all);
    setEncryptionParameters(user_password, owner_password, 2, 3, 16, clear);
}

void
QPDFWriter::setR4EncryptionParametersInsecure(
    char const* user_password,
    char const* owner_password,
    bool allow_accessibility,
    bool allow_extract,
    bool allow_assemble,
    bool allow_annotate_and_form,
    bool allow_form_filling,
    bool allow_modify_other,
    qpdf_r3_print_e print,
    bool encrypt_metadata,
    bool use_aes)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
        clear,
        user_password,
        owner_password,
        allow_accessibility,
        allow_extract,
        allow_assemble,
        allow_annotate_and_form,
        allow_form_filling,
        allow_modify_other,
        print,
        qpdf_r3m_all);
    m->encrypt_use_aes = use_aes;
    m->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 4, 4, 16, clear);
}

void
QPDFWriter::setR5EncryptionParameters(
    char const* user_password,
    char const* owner_password,
    bool allow_accessibility,
    bool allow_extract,
    bool allow_assemble,
    bool allow_annotate_and_form,
    bool allow_form_filling,
    bool allow_modify_other,
    qpdf_r3_print_e print,
    bool encrypt_metadata)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
        clear,
        user_password,
        owner_password,
        allow_accessibility,
        allow_extract,
        allow_assemble,
        allow_annotate_and_form,
        allow_form_filling,
        allow_modify_other,
        print,
        qpdf_r3m_all);
    m->encrypt_use_aes = true;
    m->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 5, 5, 32, clear);
}

void
QPDFWriter::setR6EncryptionParameters(
    char const* user_password,
    char const* owner_password,
    bool allow_accessibility,
    bool allow_extract,
    bool allow_assemble,
    bool allow_annotate_and_form,
    bool allow_form_filling,
    bool allow_modify_other,
    qpdf_r3_print_e print,
    bool encrypt_metadata)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
        clear,
        user_password,
        owner_password,
        allow_accessibility,
        allow_extract,
        allow_assemble,
        allow_annotate_and_form,
        allow_form_filling,
        allow_modify_other,
        print,
        qpdf_r3m_all);
    m->encrypt_use_aes = true;
    m->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 5, 6, 32, clear);
}

void
QPDFWriter::interpretR3EncryptionParameters(
    std::set<int>& clear,
    char const* user_password,
    char const* owner_password,
    bool allow_accessibility,
    bool allow_extract,
    bool allow_assemble,
    bool allow_annotate_and_form,
    bool allow_form_filling,
    bool allow_modify_other,
    qpdf_r3_print_e print,
    qpdf_r3_modify_e modify)
{
    // Acrobat 5 security options:

    // Checkboxes:
    //   Enable Content Access for the Visually Impaired
    //   Allow Content Copying and Extraction

    // Allowed changes menu:
    //   None
    //   Only Document Assembly
    //   Only Form Field Fill-in or Signing
    //   Comment Authoring, Form Field Fill-in or Signing
    //   General Editing, Comment and Form Field Authoring

    // Allowed printing menu:
    //   None
    //   Low Resolution
    //   Full printing

    // Meanings of bits in P when R >= 3
    //
    //  3: low-resolution printing
    //  4: document modification except as controlled by 6, 9, and 11
    //  5: extraction
    //  6: add/modify annotations (comment), fill in forms
    //     if 4+6 are set, also allows modification of form fields
    //  9: fill in forms even if 6 is clear
    // 10: accessibility; ignored by readers, should always be set
    // 11: document assembly even if 4 is clear
    // 12: high-resolution printing

    if (!allow_accessibility) {
        // setEncryptionParameters sets this if R > 3
        clear.insert(10);
    }
    if (!allow_extract) {
        clear.insert(5);
    }

    // Note: these switch statements all "fall through" (no break statements).  Each option clears
    // successively more access bits.
    switch (print) {
    case qpdf_r3p_none:
        clear.insert(3); // any printing

    case qpdf_r3p_low:
        clear.insert(12); // high resolution printing

    case qpdf_r3p_full:
        break;

        // no default so gcc warns for missing cases
    }

    // Modify options. The qpdf_r3_modify_e options control groups of bits and lack the full
    // flexibility of the spec. This is unfortunate, but it's been in the API for ages, and we're
    // stuck with it. See also allow checks below to control the bits individually.

    // NOT EXERCISED IN TEST SUITE
    switch (modify) {
    case qpdf_r3m_none:
        clear.insert(11); // document assembly

    case qpdf_r3m_assembly:
        clear.insert(9); // filling in form fields

    case qpdf_r3m_form:
        clear.insert(6); // modify annotations, fill in form fields

    case qpdf_r3m_annotate:
        clear.insert(4); // other modifications

    case qpdf_r3m_all:
        break;

        // no default so gcc warns for missing cases
    }
    // END NOT EXERCISED IN TEST SUITE

    if (!allow_assemble) {
        clear.insert(11);
    }
    if (!allow_annotate_and_form) {
        clear.insert(6);
    }
    if (!allow_form_filling) {
        clear.insert(9);
    }
    if (!allow_modify_other) {
        clear.insert(4);
    }
}

void
QPDFWriter::setEncryptionParameters(
    char const* user_password,
    char const* owner_password,
    int V,
    int R,
    int key_len,
    std::set<int>& bits_to_clear)
{
    // PDF specification refers to bits with the low bit numbered 1.
    // We have to convert this into a bit field.

    // Specification always requires bits 1 and 2 to be cleared.
    bits_to_clear.insert(1);
    bits_to_clear.insert(2);

    if (R > 3) {
        // Bit 10 is deprecated and should always be set.  This used to mean accessibility.  There
        // is no way to disable accessibility with R > 3.
        bits_to_clear.erase(10);
    }

    int P = 0;
    // Create the complement of P, then invert.
    for (int b: bits_to_clear) {
        P |= (1 << (b - 1));
    }
    P = ~P;

    generateID();
    std::string O;
    std::string U;
    std::string OE;
    std::string UE;
    std::string Perms;
    std::string encryption_key;
    if (V < 5) {
        QPDF::compute_encryption_O_U(
            user_password, owner_password, V, R, key_len, P, m->encrypt_metadata, m->id1, O, U);
    } else {
        QPDF::compute_encryption_parameters_V5(
            user_password,
            owner_password,
            V,
            R,
            key_len,
            P,
            m->encrypt_metadata,
            m->id1,
            encryption_key,
            O,
            U,
            OE,
            UE,
            Perms);
    }
    setEncryptionParametersInternal(
        V, R, key_len, P, O, U, OE, UE, Perms, m->id1, user_password, encryption_key);
}

void
QPDFWriter::copyEncryptionParameters(QPDF& qpdf)
{
    m->preserve_encryption = false;
    QPDFObjectHandle trailer = qpdf.getTrailer();
    if (trailer.hasKey("/Encrypt")) {
        generateID();
        m->id1 = trailer.getKey("/ID").getArrayItem(0).getStringValue();
        QPDFObjectHandle encrypt = trailer.getKey("/Encrypt");
        int V = encrypt.getKey("/V").getIntValueAsInt();
        int key_len = 5;
        if (V > 1) {
            key_len = encrypt.getKey("/Length").getIntValueAsInt() / 8;
        }
        if (encrypt.hasKey("/EncryptMetadata") && encrypt.getKey("/EncryptMetadata").isBool()) {
            m->encrypt_metadata = encrypt.getKey("/EncryptMetadata").getBoolValue();
        }
        if (V >= 4) {
            // When copying encryption parameters, use AES even if the original file did not.
            // Acrobat doesn't create files with V >= 4 that don't use AES, and the logic of
            // figuring out whether AES is used or not is complicated with /StmF, /StrF, and /EFF
            // all potentially having different values.
            m->encrypt_use_aes = true;
        }
        QTC::TC("qpdf", "QPDFWriter copy encrypt metadata", m->encrypt_metadata ? 0 : 1);
        QTC::TC("qpdf", "QPDFWriter copy use_aes", m->encrypt_use_aes ? 0 : 1);
        std::string OE;
        std::string UE;
        std::string Perms;
        std::string encryption_key;
        if (V >= 5) {
            QTC::TC("qpdf", "QPDFWriter copy V5");
            OE = encrypt.getKey("/OE").getStringValue();
            UE = encrypt.getKey("/UE").getStringValue();
            Perms = encrypt.getKey("/Perms").getStringValue();
            encryption_key = qpdf.getEncryptionKey();
        }

        setEncryptionParametersInternal(
            V,
            encrypt.getKey("/R").getIntValueAsInt(),
            key_len,
            static_cast<int>(encrypt.getKey("/P").getIntValue()),
            encrypt.getKey("/O").getStringValue(),
            encrypt.getKey("/U").getStringValue(),
            OE,
            UE,
            Perms,
            m->id1, // m->id1 == the other file's id1
            qpdf.getPaddedUserPassword(),
            encryption_key);
    }
}

void
QPDFWriter::disableIncompatibleEncryption(int major, int minor, int extension_level)
{
    if (!m->encrypted) {
        return;
    }

    bool disable = false;
    if (compareVersions(major, minor, 1, 3) < 0) {
        disable = true;
    } else {
        int V = QUtil::string_to_int(m->encryption_dictionary["/V"].c_str());
        int R = QUtil::string_to_int(m->encryption_dictionary["/R"].c_str());
        if (compareVersions(major, minor, 1, 4) < 0) {
            if ((V > 1) || (R > 2)) {
                disable = true;
            }
        } else if (compareVersions(major, minor, 1, 5) < 0) {
            if ((V > 2) || (R > 3)) {
                disable = true;
            }
        } else if (compareVersions(major, minor, 1, 6) < 0) {
            if (m->encrypt_use_aes) {
                disable = true;
            }
        } else if (
            (compareVersions(major, minor, 1, 7) < 0) ||
            ((compareVersions(major, minor, 1, 7) == 0) && extension_level < 3)) {
            if ((V >= 5) || (R >= 5)) {
                disable = true;
            }
        }
    }
    if (disable) {
        QTC::TC("qpdf", "QPDFWriter forced version disabled encryption");
        m->encrypted = false;
    }
}

void
QPDFWriter::parseVersion(std::string const& version, int& major, int& minor) const
{
    major = QUtil::string_to_int(version.c_str());
    minor = 0;
    size_t p = version.find('.');
    if ((p != std::string::npos) && (version.length() > p)) {
        minor = QUtil::string_to_int(version.substr(p + 1).c_str());
    }
    std::string tmp = std::to_string(major) + "." + std::to_string(minor);
    if (tmp != version) {
        // The version number in the input is probably invalid. This happens with some files that
        // are designed to exercise bugs, such as files in the fuzzer corpus. Unfortunately
        // QPDFWriter doesn't have a way to give a warning, so we just ignore this case.
    }
}

int
QPDFWriter::compareVersions(int major1, int minor1, int major2, int minor2) const
{
    if (major1 < major2) {
        return -1;
    } else if (major1 > major2) {
        return 1;
    } else if (minor1 < minor2) {
        return -1;
    } else if (minor1 > minor2) {
        return 1;
    } else {
        return 0;
    }
}

void
QPDFWriter::setEncryptionParametersInternal(
    int V,
    int R,
    int key_len,
    int P,
    std::string const& O,
    std::string const& U,
    std::string const& OE,
    std::string const& UE,
    std::string const& Perms,
    std::string const& id1,
    std::string const& user_password,
    std::string const& encryption_key)
{
    m->encryption_V = V;
    m->encryption_R = R;
    m->encryption_dictionary["/Filter"] = "/Standard";
    m->encryption_dictionary["/V"] = std::to_string(V);
    m->encryption_dictionary["/Length"] = std::to_string(key_len * 8);
    m->encryption_dictionary["/R"] = std::to_string(R);
    m->encryption_dictionary["/P"] = std::to_string(P);
    m->encryption_dictionary["/O"] = QPDF_String(O).unparse(true);
    m->encryption_dictionary["/U"] = QPDF_String(U).unparse(true);
    if (V >= 5) {
        m->encryption_dictionary["/OE"] = QPDF_String(OE).unparse(true);
        m->encryption_dictionary["/UE"] = QPDF_String(UE).unparse(true);
        m->encryption_dictionary["/Perms"] = QPDF_String(Perms).unparse(true);
    }
    if (R >= 6) {
        setMinimumPDFVersion("1.7", 8);
    } else if (R == 5) {
        setMinimumPDFVersion("1.7", 3);
    } else if (R == 4) {
        setMinimumPDFVersion(m->encrypt_use_aes ? "1.6" : "1.5");
    } else if (R == 3) {
        setMinimumPDFVersion("1.4");
    } else {
        setMinimumPDFVersion("1.3");
    }

    if ((R >= 4) && (!m->encrypt_metadata)) {
        m->encryption_dictionary["/EncryptMetadata"] = "false";
    }
    if ((V == 4) || (V == 5)) {
        // The spec says the value for the crypt filter key can be anything, and xpdf seems to
        // agree.  However, Adobe Reader won't open our files unless we use /StdCF.
        m->encryption_dictionary["/StmF"] = "/StdCF";
        m->encryption_dictionary["/StrF"] = "/StdCF";
        std::string method = (m->encrypt_use_aes ? ((V < 5) ? "/AESV2" : "/AESV3") : "/V2");
        // The PDF spec says the /Length key is optional, but the PDF previewer on some versions of
        // MacOS won't open encrypted files without it.
        m->encryption_dictionary["/CF"] = "<< /StdCF << /AuthEvent /DocOpen /CFM " + method +
            " /Length " + std::string((V < 5) ? "16" : "32") + " >> >>";
    }

    m->encrypted = true;
    QPDF::EncryptionData encryption_data(
        V, R, key_len, P, O, U, OE, UE, Perms, id1, m->encrypt_metadata);
    if (V < 5) {
        m->encryption_key = QPDF::compute_encryption_key(user_password, encryption_data);
    } else {
        m->encryption_key = encryption_key;
    }
}

void
QPDFWriter::setDataKey(int objid)
{
    m->cur_data_key = QPDF::compute_data_key(
        m->encryption_key, objid, 0, m->encrypt_use_aes, m->encryption_V, m->encryption_R);
}

unsigned int
QPDFWriter::bytesNeeded(long long n)
{
    unsigned int bytes = 0;
    while (n) {
        ++bytes;
        n >>= 8;
    }
    return bytes;
}

void
QPDFWriter::writeBinary(unsigned long long val, unsigned int bytes)
{
    if (bytes > sizeof(unsigned long long)) {
        throw std::logic_error("QPDFWriter::writeBinary called with too many bytes");
    }
    unsigned char data[sizeof(unsigned long long)];
    for (unsigned int i = 0; i < bytes; ++i) {
        data[bytes - i - 1] = static_cast<unsigned char>(val & 0xff);
        val >>= 8;
    }
    m->pipeline->write(data, bytes);
}

void
QPDFWriter::writeString(std::string_view str)
{
    m->pipeline->write(reinterpret_cast<unsigned char const*>(str.data()), str.size());
}

void
QPDFWriter::writeStringQDF(std::string_view str)
{
    if (m->qdf_mode) {
        m->pipeline->write(reinterpret_cast<unsigned char const*>(str.data()), str.size());
    }
}

void
QPDFWriter::writeStringNoQDF(std::string_view str)
{
    if (!m->qdf_mode) {
        m->pipeline->write(reinterpret_cast<unsigned char const*>(str.data()), str.size());
    }
}

void
QPDFWriter::writePad(size_t nspaces)
{
    writeString(std::string(nspaces, ' '));
}

Pipeline*
QPDFWriter::pushPipeline(Pipeline* p)
{
    qpdf_assert_debug(!dynamic_cast<pl::Count*>(p));
    m->pipeline_stack.emplace_back(p);
    return p;
}

void
QPDFWriter::initializePipelineStack(Pipeline* p)
{
    m->pipeline = new pl::Count(1, p);
    m->to_delete.emplace_back(std::shared_ptr<Pipeline>(m->pipeline));
    m->pipeline_stack.emplace_back(m->pipeline);
}

void
QPDFWriter::activatePipelineStack(PipelinePopper& pp, std::string& str)
{
    activatePipelineStack(pp, false, &str, nullptr);
}

void
QPDFWriter::activatePipelineStack(PipelinePopper& pp, std::unique_ptr<pl::Link> link)
{
    m->count_buffer.clear();
    activatePipelineStack(pp, false, &m->count_buffer, std::move(link));
}

void
QPDFWriter::activatePipelineStack(
    PipelinePopper& pp, bool discard, std::string* str, std::unique_ptr<pl::Link> link)
{
    pl::Count* c;
    if (link) {
        c = new pl::Count(m->next_stack_id, m->count_buffer, std::move(link));
    } else if (discard) {
        c = new pl::Count(m->next_stack_id, nullptr);
    } else if (!str) {
        c = new pl::Count(m->next_stack_id, m->pipeline_stack.back());
    } else {
        c = new pl::Count(m->next_stack_id, *str);
    }
    pp.stack_id = m->next_stack_id;
    m->pipeline_stack.emplace_back(c);
    m->pipeline = c;
    ++m->next_stack_id;
}

QPDFWriter::PipelinePopper::~PipelinePopper()
{
    if (!stack_id) {
        return;
    }
    qpdf_assert_debug(qw->m->pipeline_stack.size() >= 2);
    qw->m->pipeline->finish();
    qpdf_assert_debug(dynamic_cast<pl::Count*>(qw->m->pipeline_stack.back()) == qw->m->pipeline);
    // It might be possible for this assertion to fail if writeLinearized exits by exception when
    // deterministic ID, but I don't think so. As of this writing, this is the only case in which
    // two dynamically allocated PipelinePopper objects ever exist at the same time, so the
    // assertion will fail if they get popped out of order from automatic destruction.
    qpdf_assert_debug(qw->m->pipeline->id() == stack_id);
    delete qw->m->pipeline_stack.back();
    qw->m->pipeline_stack.pop_back();
    while (!dynamic_cast<pl::Count*>(qw->m->pipeline_stack.back())) {
        Pipeline* p = qw->m->pipeline_stack.back();
        if (dynamic_cast<Pl_MD5*>(p) == qw->m->md5_pipeline) {
            qw->m->md5_pipeline = nullptr;
        }
        qw->m->pipeline_stack.pop_back();
        delete p;
    }
    qw->m->pipeline = dynamic_cast<pl::Count*>(qw->m->pipeline_stack.back());
}

void
QPDFWriter::adjustAESStreamLength(size_t& length)
{
    if (m->encrypted && (!m->cur_data_key.empty()) && m->encrypt_use_aes) {
        // Stream length will be padded with 1 to 16 bytes to end up as a multiple of 16.  It will
        // also be prepended by 16 bits of random data.
        length += 32 - (length & 0xf);
    }
}

void
QPDFWriter::pushEncryptionFilter(PipelinePopper& pp)
{
    if (m->encrypted && (!m->cur_data_key.empty())) {
        Pipeline* p = nullptr;
        if (m->encrypt_use_aes) {
            p = new Pl_AES_PDF(
                "aes stream encryption",
                m->pipeline,
                true,
                QUtil::unsigned_char_pointer(m->cur_data_key),
                m->cur_data_key.length());
        } else {
            p = new Pl_RC4(
                "rc4 stream encryption",
                m->pipeline,
                QUtil::unsigned_char_pointer(m->cur_data_key),
                QIntC::to_int(m->cur_data_key.length()));
        }
        pushPipeline(p);
    }
    // Must call this unconditionally so we can call popPipelineStack to balance
    // pushEncryptionFilter().
    activatePipelineStack(pp);
}

void
QPDFWriter::pushMD5Pipeline(PipelinePopper& pp)
{
    if (!m->id2.empty()) {
        // Can't happen in the code
        throw std::logic_error(
            "Deterministic ID computation enabled after ID generation has already occurred.");
    }
    qpdf_assert_debug(m->deterministic_id);
    qpdf_assert_debug(m->md5_pipeline == nullptr);
    qpdf_assert_debug(m->pipeline->getCount() == 0);
    m->md5_pipeline = new Pl_MD5("qpdf md5", m->pipeline);
    m->md5_pipeline->persistAcrossFinish(true);
    // Special case code in popPipelineStack clears m->md5_pipeline upon deletion.
    pushPipeline(m->md5_pipeline);
    activatePipelineStack(pp);
}

void
QPDFWriter::computeDeterministicIDData()
{
    qpdf_assert_debug(m->md5_pipeline != nullptr);
    qpdf_assert_debug(m->deterministic_id_data.empty());
    m->deterministic_id_data = m->md5_pipeline->getHexDigest();
    m->md5_pipeline->enable(false);
}

int
QPDFWriter::openObject(int objid)
{
    if (objid == 0) {
        objid = m->next_objid++;
    }
    m->new_obj[objid].xref = QPDFXRefEntry(m->pipeline->getCount());
    writeString(std::to_string(objid));
    writeString(" 0 obj\n");
    return objid;
}

void
QPDFWriter::closeObject(int objid)
{
    // Write a newline before endobj as it makes the file easier to repair.
    writeString("\nendobj\n");
    writeStringQDF("\n");
    auto& new_obj = m->new_obj[objid];
    new_obj.length = m->pipeline->getCount() - new_obj.xref.getOffset();
}

void
QPDFWriter::assignCompressedObjectNumbers(QPDFObjGen og)
{
    int objid = og.getObj();
    if ((og.getGen() != 0) || (m->object_stream_to_objects.count(objid) == 0)) {
        // This is not an object stream.
        return;
    }

    // Reserve numbers for the objects that belong to this object stream.
    for (auto const& iter: m->object_stream_to_objects[objid]) {
        m->obj[iter].renumber = m->next_objid++;
    }
}

void
QPDFWriter::enqueueObject(QPDFObjectHandle object)
{
    if (object.isIndirect()) {
        // This owner check can only be done for indirect objects. It is possible for a direct
        // object to have an owning QPDF that is from another file if a direct QPDFObjectHandle from
        // one file was insert into another file without copying. Doing that is safe even if the
        // original QPDF gets destroyed, which just disconnects the QPDFObjectHandle from its owner.
        if (object.getOwningQPDF() != &(m->pdf)) {
            QTC::TC("qpdf", "QPDFWriter foreign object");
            throw std::logic_error(
                "QPDFObjectHandle from different QPDF found while writing.  Use "
                "QPDF::copyForeignObject to add objects from another file.");
        }

        if (m->qdf_mode && object.isStreamOfType("/XRef")) {
            // As a special case, do not output any extraneous XRef streams in QDF mode. Doing so
            // will confuse fix-qdf, which expects to see only one XRef stream at the end of the
            // file. This case can occur when creating a QDF from a file with object streams when
            // preserving unreferenced objects since the old cross reference streams are not
            // actually referenced by object number.
            QTC::TC("qpdf", "QPDFWriter ignore XRef in qdf mode");
            return;
        }

        QPDFObjGen og = object.getObjGen();
        auto& obj = m->obj[og];

        if (obj.renumber == 0) {
            if (obj.object_stream > 0) {
                // This is in an object stream.  Don't process it here.  Instead, enqueue the object
                // stream.  Object streams always have generation 0.
                // Detect loops by storing invalid object ID -1, which will get overwritten later.
                obj.renumber = -1;
                enqueueObject(m->pdf.getObject(obj.object_stream, 0));
            } else {
                m->object_queue.push_back(object);
                obj.renumber = m->next_objid++;

                if ((og.getGen() == 0) && m->object_stream_to_objects.count(og.getObj())) {
                    // For linearized files, uncompressed objects go at end, and we take care of
                    // assigning numbers to them elsewhere.
                    if (!m->linearized) {
                        assignCompressedObjectNumbers(og);
                    }
                } else if ((!m->direct_stream_lengths) && object.isStream()) {
                    // reserve next object ID for length
                    ++m->next_objid;
                }
            }
        } else if (obj.renumber == -1) {
            // This can happen if a specially constructed file indicates that an object stream is
            // inside itself.
        }
        return;
    } else if (!m->linearized) {
        if (object.isArray()) {
            for (auto& item: object.as_array()) {
                enqueueObject(item);
            }
        } else if (auto d = object.as_dictionary()) {
            for (auto const& item: d) {
                if (!item.second.null()) {
                    enqueueObject(item.second);
                }
            }
        }
    } else {
        // ignore
    }
}

void
QPDFWriter::unparseChild(QPDFObjectHandle child, int level, int flags)
{
    if (!m->linearized) {
        enqueueObject(child);
    }
    if (child.isIndirect()) {
        writeString(std::to_string(m->obj[child].renumber));
        writeString(" 0 R");
    } else {
        unparseObject(child, level, flags);
    }
}

void
QPDFWriter::writeTrailer(
    trailer_e which, int size, bool xref_stream, qpdf_offset_t prev, int linearization_pass)
{
    QPDFObjectHandle trailer = getTrimmedTrailer();
    if (xref_stream) {
        m->cur_data_key.clear();
    } else {
        writeString("trailer <<");
    }
    writeStringQDF("\n");
    if (which == t_lin_second) {
        writeString(" /Size ");
        writeString(std::to_string(size));
    } else {
        for (auto const& [key, value]: trailer.as_dictionary()) {
            if (value.null()) {
                continue;
            }
            writeStringQDF("  ");
            writeStringNoQDF(" ");
            writeString(Name::normalize(key));
            writeString(" ");
            if (key == "/Size") {
                writeString(std::to_string(size));
                if (which == t_lin_first) {
                    writeString(" /Prev ");
                    qpdf_offset_t pos = m->pipeline->getCount();
                    writeString(std::to_string(prev));
                    writePad(QIntC::to_size(pos - m->pipeline->getCount() + 21));
                }
            } else {
                unparseChild(value, 1, 0);
            }
            writeStringQDF("\n");
        }
    }

    // Write ID
    writeStringQDF(" ");
    writeString(" /ID [");
    if (linearization_pass == 1) {
        std::string original_id1 = getOriginalID1();
        if (original_id1.empty()) {
            writeString("<00000000000000000000000000000000>");
        } else {
            // Write a string of zeroes equal in length to the representation of the original ID.
            // While writing the original ID would have the same number of bytes, it would cause a
            // change to the deterministic ID generated by older versions of the software that
            // hard-coded the length of the ID to 16 bytes.
            writeString("<");
            size_t len = QPDF_String(original_id1).unparse(true).length() - 2;
            for (size_t i = 0; i < len; ++i) {
                writeString("0");
            }
            writeString(">");
        }
        writeString("<00000000000000000000000000000000>");
    } else {
        if ((linearization_pass == 0) && (m->deterministic_id)) {
            computeDeterministicIDData();
        }
        generateID();
        writeString(QPDF_String(m->id1).unparse(true));
        writeString(QPDF_String(m->id2).unparse(true));
    }
    writeString("]");

    if (which != t_lin_second) {
        // Write reference to encryption dictionary
        if (m->encrypted) {
            writeString(" /Encrypt ");
            writeString(std::to_string(m->encryption_dict_objid));
            writeString(" 0 R");
        }
    }

    writeStringQDF("\n");
    writeStringNoQDF(" ");
    writeString(">>");
}

bool
QPDFWriter::willFilterStream(
    QPDFObjectHandle stream,
    bool& compress_stream,  // out only
    bool& is_root_metadata, // out only
    std::string* stream_data)
{
    compress_stream = false;
    is_root_metadata = false;

    QPDFObjGen old_og = stream.getObjGen();
    QPDFObjectHandle stream_dict = stream.getDict();

    if (stream.isRootMetadata()) {
        is_root_metadata = true;
    }
    bool filter = stream.isDataModified() || m->compress_streams || m->stream_decode_level;
    bool filter_on_write = stream.getFilterOnWrite();
    if (!filter_on_write) {
        QTC::TC("qpdf", "QPDFWriter getFilterOnWrite false");
        filter = false;
    }
    if (filter_on_write && m->compress_streams) {
        // Don't filter if the stream is already compressed with FlateDecode. This way we don't make
        // it worse if the original file used a better Flate algorithm, and we don't spend time and
        // CPU cycles uncompressing and recompressing stuff. This can be overridden with
        // setRecompressFlate(true).
        QPDFObjectHandle filter_obj = stream_dict.getKey("/Filter");
        if (!m->recompress_flate && !stream.isDataModified() && filter_obj.isName() &&
            (filter_obj.getName() == "/FlateDecode" || filter_obj.getName() == "/Fl")) {
            QTC::TC("qpdf", "QPDFWriter not recompressing /FlateDecode");
            filter = false;
        }
    }
    bool normalize = false;
    bool uncompress = false;
    if (filter_on_write && is_root_metadata && (!m->encrypted || !m->encrypt_metadata)) {
        QTC::TC("qpdf", "QPDFWriter not compressing metadata");
        filter = true;
        compress_stream = false;
        uncompress = true;
    } else if (filter_on_write && m->normalize_content && m->normalized_streams.count(old_og)) {
        normalize = true;
        filter = true;
    } else if (filter_on_write && filter && m->compress_streams) {
        compress_stream = true;
        QTC::TC("qpdf", "QPDFWriter compressing uncompressed stream");
    }

    // Disable compression for empty streams to improve compatibility
    if (stream_dict.getKey("/Length").isInteger() &&
        stream_dict.getKey("/Length").getIntValue() == 0) {
        filter = true;
        compress_stream = false;
    }

    bool filtered = false;
    for (bool first_attempt: {true, false}) {
        PipelinePopper pp_stream_data(this);
        if (stream_data != nullptr) {
            activatePipelineStack(pp_stream_data, *stream_data);
        } else {
            activatePipelineStack(pp_stream_data, true);
        }
        try {
            filtered = stream.pipeStreamData(
                m->pipeline,
                !filter ? 0
                        : ((normalize ? qpdf_ef_normalize : 0) |
                           (compress_stream ? qpdf_ef_compress : 0)),
                !filter ? qpdf_dl_none : (uncompress ? qpdf_dl_all : m->stream_decode_level),
                false,
                first_attempt);
            if (filter && !filtered) {
                // Try again
                filter = false;
                stream.setFilterOnWrite(false);
            } else {
                break;
            }
        } catch (std::runtime_error& e) {
            if (filter && first_attempt) {
                stream.warnIfPossible("error while getting stream data: "s + e.what());
                stream.warnIfPossible("qpdf will attempt to write the damaged stream unchanged");
                filter = false;
                stream.setFilterOnWrite(false);
                continue;
            }
            throw std::runtime_error(
                "error while getting stream data for " + stream.unparse() + ": " + e.what());
        }
        if (stream_data) {
            stream_data->clear();
        }
    }
    if (!filtered) {
        compress_stream = false;
    }
    return filtered;
}

void
QPDFWriter::unparseObject(
    QPDFObjectHandle object, int level, int flags, size_t stream_length, bool compress)
{
    QPDFObjGen old_og = object.getObjGen();
    int child_flags = flags & ~f_stream;
    if (level < 0) {
        throw std::logic_error("invalid level in QPDFWriter::unparseObject");
    }
    // For non-qdf, "indent" is a single space between tokens. For qdf, indent includes the
    // preceding newline.
    std::string indent = " ";
    if (m->qdf_mode) {
        indent.append(static_cast<size_t>(2 * level), ' ');
        indent[0] = '\n';
    }

    if (auto const tc = object.getTypeCode(); tc == ::ot_array) {
        // Note: PDF spec 1.4 implementation note 121 states that Acrobat requires a space after the
        // [ in the /H key of the linearization parameter dictionary.  We'll do this unconditionally
        // for all arrays because it looks nicer and doesn't make the files that much bigger.
        writeString("[");
        for (auto const& item: object.as_array()) {
            writeString(indent);
            writeStringQDF("  ");
            unparseChild(item, level + 1, child_flags);
        }
        writeString(indent);
        writeString("]");
    } else if (tc == ::ot_dictionary) {
        // Make a shallow copy of this object so we can modify it safely without affecting the
        // original. This code has logic to skip certain keys in agreement with prepareFileForWrite
        // and with skip_stream_parameters so that replacing them doesn't leave unreferenced objects
        // in the output. We can use unsafeShallowCopy here because all we are doing is removing or
        // replacing top-level keys.
        object = object.unsafeShallowCopy();

        // Handle special cases for specific dictionaries.

        // Extensions dictionaries.

        // We have one of several cases:
        //
        // * We need ADBE
        //    - We already have Extensions
        //       - If it has the right ADBE, preserve it
        //       - Otherwise, replace ADBE
        //    - We don't have Extensions: create one from scratch
        // * We don't want ADBE
        //    - We already have Extensions
        //       - If it only has ADBE, remove it
        //       - If it has other things, keep those and remove ADBE
        //    - We have no extensions: no action required
        //
        // Before writing, we guarantee that /Extensions, if present, is direct through the ADBE
        // dictionary, so we can modify in place.

        const bool is_root = (old_og == m->root_og);
        bool have_extensions_other = false;
        bool have_extensions_adbe = false;

        QPDFObjectHandle extensions;
        if (is_root) {
            if (object.hasKey("/Extensions") && object.getKey("/Extensions").isDictionary()) {
                extensions = object.getKey("/Extensions");
            }
        }

        if (extensions) {
            std::set<std::string> keys = extensions.getKeys();
            if (keys.count("/ADBE") > 0) {
                have_extensions_adbe = true;
                keys.erase("/ADBE");
            }
            if (keys.size() > 0) {
                have_extensions_other = true;
            }
        }

        bool need_extensions_adbe = (m->final_extension_level > 0);

        if (is_root) {
            if (need_extensions_adbe) {
                if (!(have_extensions_other || have_extensions_adbe)) {
                    // We need Extensions and don't have it.  Create it here.
                    QTC::TC("qpdf", "QPDFWriter create Extensions", m->qdf_mode ? 0 : 1);
                    extensions = object.replaceKeyAndGetNew(
                        "/Extensions", QPDFObjectHandle::newDictionary());
                }
            } else if (!have_extensions_other) {
                // We have Extensions dictionary and don't want one.
                if (have_extensions_adbe) {
                    QTC::TC("qpdf", "QPDFWriter remove existing Extensions");
                    object.removeKey("/Extensions");
                    extensions = QPDFObjectHandle(); // uninitialized
                }
            }
        }

        if (extensions) {
            QTC::TC("qpdf", "QPDFWriter preserve Extensions");
            QPDFObjectHandle adbe = extensions.getKey("/ADBE");
            if (adbe.isDictionary() &&
                adbe.getKey("/BaseVersion").isNameAndEquals("/" + m->final_pdf_version) &&
                adbe.getKey("/ExtensionLevel").isInteger() &&
                (adbe.getKey("/ExtensionLevel").getIntValue() == m->final_extension_level)) {
                QTC::TC("qpdf", "QPDFWriter preserve ADBE");
            } else {
                if (need_extensions_adbe) {
                    extensions.replaceKey(
                        "/ADBE",
                        QPDFObjectHandle::parse(
                            "<< /BaseVersion /" + m->final_pdf_version + " /ExtensionLevel " +
                            std::to_string(m->final_extension_level) + " >>"));
                } else {
                    QTC::TC("qpdf", "QPDFWriter remove ADBE");
                    extensions.removeKey("/ADBE");
                }
            }
        }

        // Stream dictionaries.

        if (flags & f_stream) {
            // Suppress /Length since we will write it manually
            object.removeKey("/Length");

            // If /DecodeParms is an empty list, remove it.
            if (object.getKey("/DecodeParms").isArray() &&
                (0 == object.getKey("/DecodeParms").getArrayNItems())) {
                QTC::TC("qpdf", "QPDFWriter remove empty DecodeParms");
                object.removeKey("/DecodeParms");
            }

            if (flags & f_filtered) {
                // We will supply our own filter and decode
                // parameters.
                object.removeKey("/Filter");
                object.removeKey("/DecodeParms");
            } else {
                // Make sure, no matter what else we have, that we don't have /Crypt in the output
                // filters.
                QPDFObjectHandle filter = object.getKey("/Filter");
                QPDFObjectHandle decode_parms = object.getKey("/DecodeParms");
                if (filter.isOrHasName("/Crypt")) {
                    if (filter.isName()) {
                        object.removeKey("/Filter");
                        object.removeKey("/DecodeParms");
                    } else {
                        int idx = -1;
                        for (int i = 0; i < filter.getArrayNItems(); ++i) {
                            QPDFObjectHandle item = filter.getArrayItem(i);
                            if (item.isNameAndEquals("/Crypt")) {
                                idx = i;
                                break;
                            }
                        }
                        if (idx >= 0) {
                            // If filter is an array, then the code in QPDF_Stream has already
                            // verified that DecodeParms and Filters are arrays of the same length,
                            // but if they weren't for some reason, eraseItem does type and bounds
                            // checking.
                            QTC::TC("qpdf", "QPDFWriter remove Crypt");
                            filter.eraseItem(idx);
                            decode_parms.eraseItem(idx);
                        }
                    }
                }
            }
        }

        writeString("<<");

        for (auto const& [key, value]: object.as_dictionary()) {
            if (!value.null()) {
                writeString(indent);
                writeStringQDF("  ");
                writeString(Name::normalize(key));
                writeString(" ");
                if (key == "/Contents" && object.isDictionaryOfType("/Sig") &&
                    object.hasKey("/ByteRange")) {
                    QTC::TC("qpdf", "QPDFWriter no encryption sig contents");
                    unparseChild(value, level + 1, child_flags | f_hex_string | f_no_encryption);
                } else {
                    unparseChild(value, level + 1, child_flags);
                }
            }
        }

        if (flags & f_stream) {
            writeString(indent);
            writeStringQDF("  ");
            writeString("/Length ");

            if (m->direct_stream_lengths) {
                writeString(std::to_string(stream_length));
            } else {
                writeString(std::to_string(m->cur_stream_length_id));
                writeString(" 0 R");
            }
            if (compress && (flags & f_filtered)) {
                writeString(indent);
                writeStringQDF("  ");
                writeString("/Filter /FlateDecode");
            }
        }

        writeString(indent);
        writeString(">>");
    } else if (tc == ::ot_stream) {
        // Write stream data to a buffer.
        if (!m->direct_stream_lengths) {
            m->cur_stream_length_id = m->obj[old_og].renumber + 1;
        }

        flags |= f_stream;
        bool compress_stream = false;
        bool is_metadata = false;
        std::string stream_data;
        if (willFilterStream(object, compress_stream, is_metadata, &stream_data)) {
            flags |= f_filtered;
        }
        QPDFObjectHandle stream_dict = object.getDict();

        m->cur_stream_length = stream_data.size();
        if (is_metadata && m->encrypted && (!m->encrypt_metadata)) {
            // Don't encrypt stream data for the metadata stream
            m->cur_data_key.clear();
        }
        adjustAESStreamLength(m->cur_stream_length);
        unparseObject(stream_dict, 0, flags, m->cur_stream_length, compress_stream);
        char last_char = stream_data.empty() ? '\0' : stream_data.back();
        writeString("\nstream\n");
        {
            PipelinePopper pp_enc(this);
            pushEncryptionFilter(pp_enc);
            writeString(stream_data);
        }

        if (m->newline_before_endstream || (m->qdf_mode && last_char != '\n')) {
            writeString("\n");
            m->added_newline = true;
        } else {
            m->added_newline = false;
        }
        writeString("endstream");
    } else if (tc == ::ot_string) {
        std::string val;
        if (m->encrypted && (!(flags & f_in_ostream)) && (!(flags & f_no_encryption)) &&
            (!m->cur_data_key.empty())) {
            val = object.getStringValue();
            if (m->encrypt_use_aes) {
                Pl_Buffer bufpl("encrypted string");
                Pl_AES_PDF pl(
                    "aes encrypt string",
                    &bufpl,
                    true,
                    QUtil::unsigned_char_pointer(m->cur_data_key),
                    m->cur_data_key.length());
                pl.writeString(val);
                pl.finish();
                val = QPDF_String(bufpl.getString()).unparse(true);
            } else {
                auto tmp_ph = QUtil::make_unique_cstr(val);
                char* tmp = tmp_ph.get();
                size_t vlen = val.length();
                RC4 rc4(
                    QUtil::unsigned_char_pointer(m->cur_data_key),
                    QIntC::to_int(m->cur_data_key.length()));
                auto data = QUtil::unsigned_char_pointer(tmp);
                rc4.process(data, vlen, data);
                val = QPDF_String(std::string(tmp, vlen)).unparse();
            }
        } else if (flags & f_hex_string) {
            val = QPDF_String(object.getStringValue()).unparse(true);
        } else {
            val = object.unparseResolved();
        }
        writeString(val);
    } else {
        writeString(object.unparseResolved());
    }
}

void
QPDFWriter::writeObjectStreamOffsets(std::vector<qpdf_offset_t>& offsets, int first_obj)
{
    qpdf_assert_debug(first_obj > 0);
    bool is_first = true;
    auto id = std::to_string(first_obj) + ' ';
    for (auto& offset: offsets) {
        if (is_first) {
            is_first = false;
        } else {
            writeStringQDF("\n");
            writeStringNoQDF(" ");
        }
        writeString(id);
        util::increment(id, 1);
        writeString(std::to_string(offset));
    }
    writeString("\n");
}

void
QPDFWriter::writeObjectStream(QPDFObjectHandle object)
{
    // Note: object might be null if this is a place-holder for an object stream that we are
    // generating from scratch.

    QPDFObjGen old_og = object.getObjGen();
    qpdf_assert_debug(old_og.getGen() == 0);
    int old_id = old_og.getObj();
    int new_stream_id = m->obj[old_og].renumber;

    std::vector<qpdf_offset_t> offsets;
    qpdf_offset_t first = 0;

    // Generate stream itself.  We have to do this in two passes so we can calculate offsets in the
    // first pass.
    std::string stream_buffer_pass1;
    std::string stream_buffer_pass2;
    int first_obj = -1;
    const bool compressed = m->compress_streams && !m->qdf_mode;
    {
        // Pass 1
        PipelinePopper pp_ostream_pass1(this);
        activatePipelineStack(pp_ostream_pass1, stream_buffer_pass1);

        int count = -1;
        for (auto const& obj: m->object_stream_to_objects[old_id]) {
            ++count;
            int new_obj = m->obj[obj].renumber;
            if (first_obj == -1) {
                first_obj = new_obj;
            }
            if (m->qdf_mode) {
                writeString(
                    "%% Object stream: object " + std::to_string(new_obj) + ", index " +
                    std::to_string(count));
                if (!m->suppress_original_object_ids) {
                    writeString("; original object ID: " + std::to_string(obj.getObj()));
                    // For compatibility, only write the generation if non-zero.  While object
                    // streams only allow objects with generation 0, if we are generating object
                    // streams, the old object could have a non-zero generation.
                    if (obj.getGen() != 0) {
                        QTC::TC("qpdf", "QPDFWriter original obj non-zero gen");
                        writeString(" " + std::to_string(obj.getGen()));
                    }
                }
                writeString("\n");
            }

            offsets.push_back(m->pipeline->getCount());
            // To avoid double-counting objects being written in object streams for progress
            // reporting, decrement in pass 1.
            indicateProgress(true, false);

            QPDFObjectHandle obj_to_write = m->pdf.getObject(obj);
            if (obj_to_write.isStream()) {
                // This condition occurred in a fuzz input. Ideally we should block it at parse
                // time, but it's not clear to me how to construct a case for this.
                obj_to_write.warnIfPossible("stream found inside object stream; treating as null");
                obj_to_write = QPDFObjectHandle::newNull();
            }
            writeObject(obj_to_write, count);

            m->new_obj[new_obj].xref = QPDFXRefEntry(new_stream_id, count);
        }
    }
    {
        PipelinePopper pp_ostream(this);
        // Adjust offsets to skip over comment before first object
        first = offsets.at(0);
        for (auto& iter: offsets) {
            iter -= first;
        }

        // Take one pass at writing pairs of numbers so we can get their size information
        {
            PipelinePopper pp_discard(this);
            activatePipelineStack(pp_discard, true);
            writeObjectStreamOffsets(offsets, first_obj);
            first += m->pipeline->getCount();
        }

        // Set up a stream to write the stream data into a buffer.
        if (compressed) {
            activatePipelineStack(
                pp_ostream,
                pl::create<Pl_Flate>(
                    pl::create<pl::String>(stream_buffer_pass2), Pl_Flate::a_deflate));
        } else {
            activatePipelineStack(pp_ostream, stream_buffer_pass2);
        }
        writeObjectStreamOffsets(offsets, first_obj);
        writeString(stream_buffer_pass1);
        stream_buffer_pass1.clear();
        stream_buffer_pass1.shrink_to_fit();
    }

    // Write the object
    openObject(new_stream_id);
    setDataKey(new_stream_id);
    writeString("<<");
    writeStringQDF("\n ");
    writeString(" /Type /ObjStm");
    writeStringQDF("\n ");
    size_t length = stream_buffer_pass2.size();
    adjustAESStreamLength(length);
    writeString(" /Length " + std::to_string(length));
    writeStringQDF("\n ");
    if (compressed) {
        writeString(" /Filter /FlateDecode");
    }
    writeString(" /N " + std::to_string(offsets.size()));
    writeStringQDF("\n ");
    writeString(" /First " + std::to_string(first));
    if (!object.isNull()) {
        // If the original object has an /Extends key, preserve it.
        QPDFObjectHandle dict = object.getDict();
        QPDFObjectHandle extends = dict.getKey("/Extends");
        if (extends.isIndirect()) {
            QTC::TC("qpdf", "QPDFWriter copy Extends");
            writeStringQDF("\n ");
            writeString(" /Extends ");
            unparseChild(extends, 1, f_in_ostream);
        }
    }
    writeStringQDF("\n");
    writeStringNoQDF(" ");
    writeString(">>\nstream\n");
    if (m->encrypted) {
        QTC::TC("qpdf", "QPDFWriter encrypt object stream");
    }
    {
        PipelinePopper pp_enc(this);
        pushEncryptionFilter(pp_enc);
        writeString(stream_buffer_pass2);
    }
    if (m->newline_before_endstream) {
        writeString("\n");
    }
    writeString("endstream");
    m->cur_data_key.clear();
    closeObject(new_stream_id);
}

void
QPDFWriter::writeObject(QPDFObjectHandle object, int object_stream_index)
{
    QPDFObjGen old_og = object.getObjGen();

    if ((object_stream_index == -1) && (old_og.getGen() == 0) &&
        (m->object_stream_to_objects.count(old_og.getObj()))) {
        writeObjectStream(object);
        return;
    }

    indicateProgress(false, false);
    auto new_id = m->obj[old_og].renumber;
    if (m->qdf_mode) {
        if (m->page_object_to_seq.count(old_og)) {
            writeString("%% Page ");
            writeString(std::to_string(m->page_object_to_seq[old_og]));
            writeString("\n");
        }
        if (m->contents_to_page_seq.count(old_og)) {
            writeString("%% Contents for page ");
            writeString(std::to_string(m->contents_to_page_seq[old_og]));
            writeString("\n");
        }
    }
    if (object_stream_index == -1) {
        if (m->qdf_mode && (!m->suppress_original_object_ids)) {
            writeString("%% Original object ID: " + object.getObjGen().unparse(' ') + "\n");
        }
        openObject(new_id);
        setDataKey(new_id);
        unparseObject(object, 0, 0);
        m->cur_data_key.clear();
        closeObject(new_id);
    } else {
        unparseObject(object, 0, f_in_ostream);
        writeString("\n");
    }

    if ((!m->direct_stream_lengths) && object.isStream()) {
        if (m->qdf_mode) {
            if (m->added_newline) {
                writeString("%QDF: ignore_newline\n");
            }
        }
        openObject(new_id + 1);
        writeString(std::to_string(m->cur_stream_length));
        closeObject(new_id + 1);
    }
}

std::string
QPDFWriter::getOriginalID1()
{
    QPDFObjectHandle trailer = m->pdf.getTrailer();
    if (trailer.hasKey("/ID")) {
        return trailer.getKey("/ID").getArrayItem(0).getStringValue();
    } else {
        return "";
    }
}

void
QPDFWriter::generateID()
{
    // Generate the ID lazily so that we can handle the user's preference to use static or
    // deterministic ID generation.

    if (!m->id2.empty()) {
        return;
    }

    QPDFObjectHandle trailer = m->pdf.getTrailer();

    std::string result;

    if (m->static_id) {
        // For test suite use only...
        static unsigned char tmp[] = {
            0x31,
            0x41,
            0x59,
            0x26,
            0x53,
            0x58,
            0x97,
            0x93,
            0x23,
            0x84,
            0x62,
            0x64,
            0x33,
            0x83,
            0x27,
            0x95,
            0x00};
        result = reinterpret_cast<char*>(tmp);
    } else {
        // The PDF specification has guidelines for creating IDs, but it states clearly that the
        // only thing that's really important is that it is very likely to be unique.  We can't
        // really follow the guidelines in the spec exactly because we haven't written the file yet.
        // This scheme should be fine though.  The deterministic ID case uses a digest of a
        // sufficient portion of the file's contents such no two non-matching files would match in
        // the subsets used for this computation.  Note that we explicitly omit the filename from
        // the digest calculation for deterministic ID so that the same file converted with qpdf, in
        // that case, would have the same ID regardless of the output file's name.

        std::string seed;
        if (m->deterministic_id) {
            if (m->deterministic_id_data.empty()) {
                QTC::TC("qpdf", "QPDFWriter deterministic with no data");
                throw std::runtime_error(
                    "INTERNAL ERROR: QPDFWriter::generateID has no data for "
                    "deterministic ID.  This may happen if deterministic ID "
                    "and file encryption are requested together.");
            }
            seed += m->deterministic_id_data;
        } else {
            seed += std::to_string(QUtil::get_current_time());
            seed += m->filename;
            seed += " ";
        }
        seed += " QPDF ";
        if (trailer.hasKey("/Info")) {
            for (auto const& item: trailer.getKey("/Info").as_dictionary()) {
                if (item.second.isString()) {
                    seed += " ";
                    seed += item.second.getStringValue();
                }
            }
        }

        MD5 m;
        m.encodeString(seed.c_str());
        MD5::Digest digest;
        m.digest(digest);
        result = std::string(reinterpret_cast<char*>(digest), sizeof(MD5::Digest));
    }

    // If /ID already exists, follow the spec: use the original first word and generate a new second
    // word.  Otherwise, we'll use the generated ID for both.

    m->id2 = result;
    // Note: keep /ID from old file even if --static-id was given.
    m->id1 = getOriginalID1();
    if (m->id1.empty()) {
        m->id1 = m->id2;
    }
}

void
QPDFWriter::initializeSpecialStreams()
{
    // Mark all page content streams in case we are filtering or normalizing.
    std::vector<QPDFObjectHandle> pages = m->pdf.getAllPages();
    int num = 0;
    for (auto& page: pages) {
        m->page_object_to_seq[page.getObjGen()] = ++num;
        QPDFObjectHandle contents = page.getKey("/Contents");
        std::vector<QPDFObjGen> contents_objects;
        if (contents.isArray()) {
            int n = contents.getArrayNItems();
            for (int i = 0; i < n; ++i) {
                contents_objects.push_back(contents.getArrayItem(i).getObjGen());
            }
        } else if (contents.isStream()) {
            contents_objects.push_back(contents.getObjGen());
        }

        for (auto const& c: contents_objects) {
            m->contents_to_page_seq[c] = num;
            m->normalized_streams.insert(c);
        }
    }
}

void
QPDFWriter::preserveObjectStreams()
{
    auto const& xref = QPDF::Writer::getXRefTable(m->pdf);
    // Our object_to_object_stream map has to map ObjGen -> ObjGen since we may be generating object
    // streams out of old objects that have generation numbers greater than zero. However in an
    // existing PDF, all object stream objects and all objects in them must have generation 0
    // because the PDF spec does not provide any way to do otherwise. This code filters out objects
    // that are not allowed to be in object streams. In addition to removing objects that were
    // erroneously included in object streams in the source PDF, it also prevents unreferenced
    // objects from being included.
    auto end = xref.cend();
    m->obj.streams_empty = true;
    if (m->preserve_unreferenced_objects) {
        for (auto iter = xref.cbegin(); iter != end; ++iter) {
            if (iter->second.getType() == 2) {
                // Pdf contains object streams.
                QTC::TC("qpdf", "QPDFWriter preserve object streams preserve unreferenced");
                m->obj.streams_empty = false;
                m->obj[iter->first].object_stream = iter->second.getObjStreamNumber();
            }
        }
    } else {
        // Start by scanning for first compressed object in case we don't have any object streams to
        // process.
        for (auto iter = xref.cbegin(); iter != end; ++iter) {
            if (iter->second.getType() == 2) {
                // Pdf contains object streams.
                QTC::TC("qpdf", "QPDFWriter preserve object streams");
                m->obj.streams_empty = false;
                auto eligible = QPDF::Writer::getCompressibleObjSet(m->pdf);
                // The object pointed to by iter may be a previous generation, in which case it is
                // removed by getCompressibleObjSet. We need to restart the loop (while the object
                // table may contain multiple generations of an object).
                for (iter = xref.cbegin(); iter != end; ++iter) {
                    if (iter->second.getType() == 2) {
                        auto id = static_cast<size_t>(iter->first.getObj());
                        if (id < eligible.size() && eligible[id]) {
                            m->obj[iter->first].object_stream = iter->second.getObjStreamNumber();
                        } else {
                            QTC::TC("qpdf", "QPDFWriter exclude from object stream");
                        }
                    }
                }
                return;
            }
        }
    }
}

void
QPDFWriter::generateObjectStreams()
{
    // Basic strategy: make a list of objects that can go into an object stream.  Then figure out
    // how many object streams are needed so that we can distribute objects approximately evenly
    // without having any object stream exceed 100 members.  We don't have to worry about linearized
    // files here -- if the file is linearized, we take care of excluding things that aren't allowed
    // here later.

    // This code doesn't do anything with /Extends.

    std::vector<QPDFObjGen> eligible = QPDF::Writer::getCompressibleObjGens(m->pdf);
    size_t n_object_streams = (eligible.size() + 99U) / 100U;

    initializeTables(2U * n_object_streams);
    if (n_object_streams == 0) {
        m->obj.streams_empty = true;
        return;
    }
    size_t n_per = eligible.size() / n_object_streams;
    if (n_per * n_object_streams < eligible.size()) {
        ++n_per;
    }
    unsigned int n = 0;
    int cur_ostream = m->pdf.newIndirectNull().getObjectID();
    for (auto const& item: eligible) {
        if (n == n_per) {
            QTC::TC("qpdf", "QPDFWriter generate >1 ostream");
            n = 0;
            // Construct a new null object as the "original" object stream.  The rest of the code
            // knows that this means we're creating the object stream from scratch.
            cur_ostream = m->pdf.newIndirectNull().getObjectID();
        }
        auto& obj = m->obj[item];
        obj.object_stream = cur_ostream;
        obj.gen = item.getGen();
        ++n;
    }
}

QPDFObjectHandle
QPDFWriter::getTrimmedTrailer()
{
    // Remove keys from the trailer that necessarily have to be replaced when writing the file.

    QPDFObjectHandle trailer = m->pdf.getTrailer().unsafeShallowCopy();

    // Remove encryption keys
    trailer.removeKey("/ID");
    trailer.removeKey("/Encrypt");

    // Remove modification information
    trailer.removeKey("/Prev");

    // Remove all trailer keys that potentially come from a cross-reference stream
    trailer.removeKey("/Index");
    trailer.removeKey("/W");
    trailer.removeKey("/Length");
    trailer.removeKey("/Filter");
    trailer.removeKey("/DecodeParms");
    trailer.removeKey("/Type");
    trailer.removeKey("/XRefStm");

    return trailer;
}

// Make document extension level information direct as required by the spec.
void
QPDFWriter::prepareFileForWrite()
{
    m->pdf.fixDanglingReferences();
    auto root = m->pdf.getRoot();
    auto oh = root.getKey("/Extensions");
    if (oh.isDictionary()) {
        const bool extensions_indirect = oh.isIndirect();
        if (extensions_indirect) {
            QTC::TC("qpdf", "QPDFWriter make Extensions direct");
            oh = root.replaceKeyAndGetNew("/Extensions", oh.shallowCopy());
        }
        if (oh.hasKey("/ADBE")) {
            auto adbe = oh.getKey("/ADBE");
            if (adbe.isIndirect()) {
                QTC::TC("qpdf", "QPDFWriter make ADBE direct", extensions_indirect ? 0 : 1);
                adbe.makeDirect();
                oh.replaceKey("/ADBE", adbe);
            }
        }
    }
}

void
QPDFWriter::initializeTables(size_t extra)
{
    auto size = QIntC::to_size(QPDF::Writer::tableSize(m->pdf) + 100) + extra;
    m->obj.resize(size);
    m->new_obj.resize(size);
}

void
QPDFWriter::doWriteSetup()
{
    if (m->did_write_setup) {
        return;
    }
    m->did_write_setup = true;

    // Do preliminary setup

    if (m->linearized) {
        m->qdf_mode = false;
    }

    if (m->pclm) {
        m->stream_decode_level = qpdf_dl_none;
        m->compress_streams = false;
        m->encrypted = false;
    }

    if (m->qdf_mode) {
        if (!m->normalize_content_set) {
            m->normalize_content = true;
        }
        if (!m->compress_streams_set) {
            m->compress_streams = false;
        }
        if (!m->stream_decode_level_set) {
            m->stream_decode_level = qpdf_dl_generalized;
        }
    }

    if (m->encrypted) {
        // Encryption has been explicitly set
        m->preserve_encryption = false;
    } else if (m->normalize_content || !m->compress_streams || m->pclm || m->qdf_mode) {
        // Encryption makes looking at contents pretty useless.  If the user explicitly encrypted
        // though, we still obey that.
        m->preserve_encryption = false;
    }

    if (m->preserve_encryption) {
        copyEncryptionParameters(m->pdf);
    }

    if (!m->forced_pdf_version.empty()) {
        int major = 0;
        int minor = 0;
        parseVersion(m->forced_pdf_version, major, minor);
        disableIncompatibleEncryption(major, minor, m->forced_extension_level);
        if (compareVersions(major, minor, 1, 5) < 0) {
            QTC::TC("qpdf", "QPDFWriter forcing object stream disable");
            m->object_stream_mode = qpdf_o_disable;
        }
    }

    if (m->qdf_mode || m->normalize_content || m->stream_decode_level) {
        initializeSpecialStreams();
    }

    if (m->qdf_mode) {
        // Generate indirect stream lengths for qdf mode since fix-qdf uses them for storing
        // recomputed stream length data. Certain streams such as object streams, xref streams, and
        // hint streams always get direct stream lengths.
        m->direct_stream_lengths = false;
    }

    switch (m->object_stream_mode) {
    case qpdf_o_disable:
        initializeTables();
        m->obj.streams_empty = true;
        break;

    case qpdf_o_preserve:
        initializeTables();
        preserveObjectStreams();
        break;

    case qpdf_o_generate:
        generateObjectStreams();
        break;

        // no default so gcc will warn for missing case tag
    }

    if (!m->obj.streams_empty) {
        if (m->linearized) {
            // Page dictionaries are not allowed to be compressed objects.
            for (auto& page: m->pdf.getAllPages()) {
                if (m->obj[page].object_stream > 0) {
                    QTC::TC("qpdf", "QPDFWriter uncompressing page dictionary");
                    m->obj[page].object_stream = 0;
                }
            }
        }

        if (m->linearized || m->encrypted) {
            // The document catalog is not allowed to be compressed in linearized files either.  It
            // also appears that Adobe Reader 8.0.0 has a bug that prevents it from being able to
            // handle encrypted files with compressed document catalogs, so we disable them in that
            // case as well.
            if (m->obj[m->root_og].object_stream > 0) {
                QTC::TC("qpdf", "QPDFWriter uncompressing root");
                m->obj[m->root_og].object_stream = 0;
            }
        }

        // Generate reverse mapping from object stream to objects
        m->obj.forEach([this](auto id, auto const& item) -> void {
            if (item.object_stream > 0) {
                auto& vec = m->object_stream_to_objects[item.object_stream];
                vec.emplace_back(id, item.gen);
                if (m->max_ostream_index < vec.size()) {
                    ++m->max_ostream_index;
                }
            }
        });
        --m->max_ostream_index;

        if (m->object_stream_to_objects.empty()) {
            m->obj.streams_empty = true;
        } else {
            setMinimumPDFVersion("1.5");
        }
    }

    setMinimumPDFVersion(m->pdf.getPDFVersion(), m->pdf.getExtensionLevel());
    m->final_pdf_version = m->min_pdf_version;
    m->final_extension_level = m->min_extension_level;
    if (!m->forced_pdf_version.empty()) {
        QTC::TC("qpdf", "QPDFWriter using forced PDF version");
        m->final_pdf_version = m->forced_pdf_version;
        m->final_extension_level = m->forced_extension_level;
    }
}

void
QPDFWriter::write()
{
    doWriteSetup();

    // Set up progress reporting. For linearized files, we write two passes. events_expected is an
    // approximation, but it's good enough for progress reporting, which is mostly a guess anyway.
    m->events_expected = QIntC::to_int(m->pdf.getObjectCount() * (m->linearized ? 2 : 1));

    prepareFileForWrite();

    if (m->linearized) {
        writeLinearized();
    } else {
        writeStandard();
    }

    m->pipeline->finish();
    if (m->close_file) {
        fclose(m->file);
    }
    m->file = nullptr;
    if (m->buffer_pipeline) {
        m->output_buffer = m->buffer_pipeline->getBuffer();
        m->buffer_pipeline = nullptr;
    }
    indicateProgress(false, true);
}

QPDFObjGen
QPDFWriter::getRenumberedObjGen(QPDFObjGen og)
{
    return QPDFObjGen(m->obj[og].renumber, 0);
}

std::map<QPDFObjGen, QPDFXRefEntry>
QPDFWriter::getWrittenXRefTable()
{
    std::map<QPDFObjGen, QPDFXRefEntry> result;

    auto it = result.begin();
    m->new_obj.forEach([&it, &result](auto id, auto const& item) -> void {
        if (item.xref.getType() != 0) {
            it = result.emplace_hint(it, QPDFObjGen(id, 0), item.xref);
        }
    });
    return result;
}

void
QPDFWriter::enqueuePart(std::vector<QPDFObjectHandle>& part)
{
    for (auto const& oh: part) {
        enqueueObject(oh);
    }
}

void
QPDFWriter::writeEncryptionDictionary()
{
    m->encryption_dict_objid = openObject(m->encryption_dict_objid);
    writeString("<<");
    for (auto const& iter: m->encryption_dictionary) {
        writeString(" ");
        writeString(iter.first);
        writeString(" ");
        writeString(iter.second);
    }
    writeString(" >>");
    closeObject(m->encryption_dict_objid);
}

std::string
QPDFWriter::getFinalVersion()
{
    doWriteSetup();
    return m->final_pdf_version;
}

void
QPDFWriter::writeHeader()
{
    writeString("%PDF-");
    writeString(m->final_pdf_version);
    if (m->pclm) {
        // PCLm version
        writeString("\n%PCLm 1.0\n");
    } else {
        // This string of binary characters would not be valid UTF-8, so it really should be treated
        // as binary.
        writeString("\n%\xbf\xf7\xa2\xfe\n");
    }
    writeStringQDF("%QDF-1.0\n\n");

    // Note: do not write extra header text here.  Linearized PDFs must include the entire
    // linearization parameter dictionary within the first 1024 characters of the PDF file, so for
    // linearized files, we have to write extra header text after the linearization parameter
    // dictionary.
}

void
QPDFWriter::writeHintStream(int hint_id)
{
    std::string hint_buffer;
    int S = 0;
    int O = 0;
    bool compressed = (m->compress_streams && !m->qdf_mode);
    QPDF::Writer::generateHintStream(m->pdf, m->new_obj, m->obj, hint_buffer, S, O, compressed);

    openObject(hint_id);
    setDataKey(hint_id);

    size_t hlen = hint_buffer.size();

    writeString("<< ");
    if (compressed) {
        writeString("/Filter /FlateDecode ");
    }
    writeString("/S ");
    writeString(std::to_string(S));
    if (O) {
        writeString(" /O ");
        writeString(std::to_string(O));
    }
    writeString(" /Length ");
    adjustAESStreamLength(hlen);
    writeString(std::to_string(hlen));
    writeString(" >>\nstream\n");

    if (m->encrypted) {
        QTC::TC("qpdf", "QPDFWriter encrypted hint stream");
    }
    char last_char = hint_buffer.empty() ? '\0' : hint_buffer.back();
    {
        PipelinePopper pp_enc(this);
        pushEncryptionFilter(pp_enc);
        writeString(hint_buffer);
    }

    if (last_char != '\n') {
        writeString("\n");
    }
    writeString("endstream");
    closeObject(hint_id);
}

qpdf_offset_t
QPDFWriter::writeXRefTable(trailer_e which, int first, int last, int size)
{
    // There are too many extra arguments to replace overloaded function with defaults in the header
    // file...too much risk of leaving something off.
    return writeXRefTable(which, first, last, size, 0, false, 0, 0, 0, 0);
}

qpdf_offset_t
QPDFWriter::writeXRefTable(
    trailer_e which,
    int first,
    int last,
    int size,
    qpdf_offset_t prev,
    bool suppress_offsets,
    int hint_id,
    qpdf_offset_t hint_offset,
    qpdf_offset_t hint_length,
    int linearization_pass)
{
    writeString("xref\n");
    writeString(std::to_string(first));
    writeString(" ");
    writeString(std::to_string(last - first + 1));
    qpdf_offset_t space_before_zero = m->pipeline->getCount();
    writeString("\n");
    for (int i = first; i <= last; ++i) {
        if (i == 0) {
            writeString("0000000000 65535 f \n");
        } else {
            qpdf_offset_t offset = 0;
            if (!suppress_offsets) {
                offset = m->new_obj[i].xref.getOffset();
                if ((hint_id != 0) && (i != hint_id) && (offset >= hint_offset)) {
                    offset += hint_length;
                }
            }
            writeString(QUtil::int_to_string(offset, 10));
            writeString(" 00000 n \n");
        }
    }
    writeTrailer(which, size, false, prev, linearization_pass);
    writeString("\n");
    return space_before_zero;
}

qpdf_offset_t
QPDFWriter::writeXRefStream(
    int objid, int max_id, qpdf_offset_t max_offset, trailer_e which, int first, int last, int size)
{
    // There are too many extra arguments to replace overloaded function with defaults in the header
    // file...too much risk of leaving something off.
    return writeXRefStream(
        objid, max_id, max_offset, which, first, last, size, 0, 0, 0, 0, false, 0);
}

qpdf_offset_t
QPDFWriter::writeXRefStream(
    int xref_id,
    int max_id,
    qpdf_offset_t max_offset,
    trailer_e which,
    int first,
    int last,
    int size,
    qpdf_offset_t prev,
    int hint_id,
    qpdf_offset_t hint_offset,
    qpdf_offset_t hint_length,
    bool skip_compression,
    int linearization_pass)
{
    qpdf_offset_t xref_offset = m->pipeline->getCount();
    qpdf_offset_t space_before_zero = xref_offset - 1;

    // field 1 contains offsets and object stream identifiers
    unsigned int f1_size = std::max(bytesNeeded(max_offset + hint_length), bytesNeeded(max_id));

    // field 2 contains object stream indices
    unsigned int f2_size = bytesNeeded(QIntC::to_longlong(m->max_ostream_index));

    unsigned int esize = 1 + f1_size + f2_size;

    // Must store in xref table in advance of writing the actual data rather than waiting for
    // openObject to do it.
    m->new_obj[xref_id].xref = QPDFXRefEntry(m->pipeline->getCount());

    std::string xref_data;
    const bool compressed = m->compress_streams && !m->qdf_mode;
    {
        PipelinePopper pp_xref(this);
        if (compressed) {
            m->count_buffer.clear();
            auto link = pl::create<pl::String>(xref_data);
            if (!skip_compression) {
                // Write the stream dictionary for compression but don't actually compress.  This
                // helps us with computation of padding for pass 1 of linearization.
                link = pl::create<Pl_Flate>(std::move(link), Pl_Flate::a_deflate);
            }
            activatePipelineStack(
                pp_xref, pl::create<Pl_PNGFilter>(std::move(link), Pl_PNGFilter::a_encode, esize));
        } else {
            activatePipelineStack(pp_xref, xref_data);
        }

        for (int i = first; i <= last; ++i) {
            QPDFXRefEntry& e = m->new_obj[i].xref;
            switch (e.getType()) {
            case 0:
                writeBinary(0, 1);
                writeBinary(0, f1_size);
                writeBinary(0, f2_size);
                break;

            case 1:
                {
                    qpdf_offset_t offset = e.getOffset();
                    if ((hint_id != 0) && (i != hint_id) && (offset >= hint_offset)) {
                        offset += hint_length;
                    }
                    writeBinary(1, 1);
                    writeBinary(QIntC::to_ulonglong(offset), f1_size);
                    writeBinary(0, f2_size);
                }
                break;

            case 2:
                writeBinary(2, 1);
                writeBinary(QIntC::to_ulonglong(e.getObjStreamNumber()), f1_size);
                writeBinary(QIntC::to_ulonglong(e.getObjStreamIndex()), f2_size);
                break;

            default:
                throw std::logic_error("invalid type writing xref stream");
                break;
            }
        }
    }

    openObject(xref_id);
    writeString("<<");
    writeStringQDF("\n ");
    writeString(" /Type /XRef");
    writeStringQDF("\n ");
    writeString(" /Length " + std::to_string(xref_data.size()));
    if (compressed) {
        writeStringQDF("\n ");
        writeString(" /Filter /FlateDecode");
        writeStringQDF("\n ");
        writeString(" /DecodeParms << /Columns " + std::to_string(esize) + " /Predictor 12 >>");
    }
    writeStringQDF("\n ");
    writeString(" /W [ 1 " + std::to_string(f1_size) + " " + std::to_string(f2_size) + " ]");
    if (!((first == 0) && (last == size - 1))) {
        writeString(
            " /Index [ " + std::to_string(first) + " " + std::to_string(last - first + 1) + " ]");
    }
    writeTrailer(which, size, true, prev, linearization_pass);
    writeString("\nstream\n");
    writeString(xref_data);
    writeString("\nendstream");
    closeObject(xref_id);
    return space_before_zero;
}

size_t
QPDFWriter::calculateXrefStreamPadding(qpdf_offset_t xref_bytes)
{
    // This routine is called right after a linearization first pass xref stream has been written
    // without compression.  Calculate the amount of padding that would be required in the worst
    // case, assuming the number of uncompressed bytes remains the same. The worst case for zlib is
    // that the output is larger than the input by 6 bytes plus 5 bytes per 16K, and then we'll add
    // 10 extra bytes for number length increases.

    return QIntC::to_size(16 + (5 * ((xref_bytes + 16383) / 16384)));
}

void
QPDFWriter::writeLinearized()
{
    // Optimize file and enqueue objects in order

    std::map<int, int> stream_cache;

    auto skip_stream_parameters = [this, &stream_cache](QPDFObjectHandle& stream) {
        auto& result = stream_cache[stream.getObjectID()];
        if (result == 0) {
            bool compress_stream;
            bool is_metadata;
            if (willFilterStream(stream, compress_stream, is_metadata, nullptr)) {
                result = 2;
            } else {
                result = 1;
            }
        }
        return result;
    };

    QPDF::Writer::optimize(m->pdf, m->obj, skip_stream_parameters);

    std::vector<QPDFObjectHandle> part4;
    std::vector<QPDFObjectHandle> part6;
    std::vector<QPDFObjectHandle> part7;
    std::vector<QPDFObjectHandle> part8;
    std::vector<QPDFObjectHandle> part9;
    QPDF::Writer::getLinearizedParts(m->pdf, m->obj, part4, part6, part7, part8, part9);

    // Object number sequence:
    //
    //  second half
    //    second half uncompressed objects
    //    second half xref stream, if any
    //    second half compressed objects
    //  first half
    //    linearization dictionary
    //    first half xref stream, if any
    //    part 4 uncompresesd objects
    //    encryption dictionary, if any
    //    hint stream
    //    part 6 uncompressed objects
    //    first half compressed objects
    //

    // Second half objects
    int second_half_uncompressed = QIntC::to_int(part7.size() + part8.size() + part9.size());
    int second_half_first_obj = 1;
    int after_second_half = 1 + second_half_uncompressed;
    m->next_objid = after_second_half;
    int second_half_xref = 0;
    bool need_xref_stream = !m->obj.streams_empty;
    if (need_xref_stream) {
        second_half_xref = m->next_objid++;
    }
    // Assign numbers to all compressed objects in the second half.
    std::vector<QPDFObjectHandle>* vecs2[] = {&part7, &part8, &part9};
    for (int i = 0; i < 3; ++i) {
        for (auto const& oh: *vecs2[i]) {
            assignCompressedObjectNumbers(oh.getObjGen());
        }
    }
    int second_half_end = m->next_objid - 1;
    int second_trailer_size = m->next_objid;

    // First half objects
    int first_half_start = m->next_objid;
    int lindict_id = m->next_objid++;
    int first_half_xref = 0;
    if (need_xref_stream) {
        first_half_xref = m->next_objid++;
    }
    int part4_first_obj = m->next_objid;
    m->next_objid += QIntC::to_int(part4.size());
    int after_part4 = m->next_objid;
    if (m->encrypted) {
        m->encryption_dict_objid = m->next_objid++;
    }
    int hint_id = m->next_objid++;
    int part6_first_obj = m->next_objid;
    m->next_objid += QIntC::to_int(part6.size());
    int after_part6 = m->next_objid;
    // Assign numbers to all compressed objects in the first half
    std::vector<QPDFObjectHandle>* vecs1[] = {&part4, &part6};
    for (int i = 0; i < 2; ++i) {
        for (auto const& oh: *vecs1[i]) {
            assignCompressedObjectNumbers(oh.getObjGen());
        }
    }
    int first_half_end = m->next_objid - 1;
    int first_trailer_size = m->next_objid;

    int part4_end_marker = part4.back().getObjectID();
    int part6_end_marker = part6.back().getObjectID();
    qpdf_offset_t space_before_zero = 0;
    qpdf_offset_t file_size = 0;
    qpdf_offset_t part6_end_offset = 0;
    qpdf_offset_t first_half_max_obj_offset = 0;
    qpdf_offset_t second_xref_offset = 0;
    qpdf_offset_t first_xref_end = 0;
    qpdf_offset_t second_xref_end = 0;

    m->next_objid = part4_first_obj;
    enqueuePart(part4);
    if (m->next_objid != after_part4) {
        // This can happen with very botched files as in the fuzzer test. There are likely some
        // faulty assumptions in calculateLinearizationData
        throw std::runtime_error("error encountered after writing part 4 of linearized data");
    }
    m->next_objid = part6_first_obj;
    enqueuePart(part6);
    if (m->next_objid != after_part6) {
        throw std::runtime_error("error encountered after writing part 6 of linearized data");
    }
    m->next_objid = second_half_first_obj;
    enqueuePart(part7);
    enqueuePart(part8);
    enqueuePart(part9);
    if (m->next_objid != after_second_half) {
        throw std::runtime_error("error encountered after writing part 9 of linearized data");
    }

    qpdf_offset_t hint_length = 0;
    std::string hint_buffer;

    // Write file in two passes.  Part numbers refer to PDF spec 1.4.

    FILE* lin_pass1_file = nullptr;
    auto pp_pass1 = std::make_unique<PipelinePopper>(this);
    auto pp_md5 = std::make_unique<PipelinePopper>(this);
    for (int pass: {1, 2}) {
        if (pass == 1) {
            if (!m->lin_pass1_filename.empty()) {
                lin_pass1_file = QUtil::safe_fopen(m->lin_pass1_filename.c_str(), "wb");
                pushPipeline(new Pl_StdioFile("linearization pass1", lin_pass1_file));
                activatePipelineStack(*pp_pass1);
            } else {
                activatePipelineStack(*pp_pass1, true);
            }
            if (m->deterministic_id) {
                pushMD5Pipeline(*pp_md5);
            }
        }

        // Part 1: header

        writeHeader();

        // Part 2: linearization parameter dictionary.  Save enough space to write real dictionary.
        // 200 characters is enough space if all numerical values in the parameter dictionary that
        // contain offsets are 20 digits long plus a few extra characters for safety.  The entire
        // linearization parameter dictionary must appear within the first 1024 characters of the
        // file.

        qpdf_offset_t pos = m->pipeline->getCount();
        openObject(lindict_id);
        writeString("<<");
        if (pass == 2) {
            std::vector<QPDFObjectHandle> const& pages = m->pdf.getAllPages();
            int first_page_object = m->obj[pages.at(0)].renumber;
            int npages = QIntC::to_int(pages.size());

            writeString(" /Linearized 1 /L ");
            writeString(std::to_string(file_size + hint_length));
            // Implementation note 121 states that a space is mandatory after this open bracket.
            writeString(" /H [ ");
            writeString(std::to_string(m->new_obj[hint_id].xref.getOffset()));
            writeString(" ");
            writeString(std::to_string(hint_length));
            writeString(" ] /O ");
            writeString(std::to_string(first_page_object));
            writeString(" /E ");
            writeString(std::to_string(part6_end_offset + hint_length));
            writeString(" /N ");
            writeString(std::to_string(npages));
            writeString(" /T ");
            writeString(std::to_string(space_before_zero + hint_length));
        }
        writeString(" >>");
        closeObject(lindict_id);
        static int const pad = 200;
        writePad(QIntC::to_size(pos - m->pipeline->getCount() + pad));
        writeString("\n");

        // If the user supplied any additional header text, write it here after the linearization
        // parameter dictionary.
        writeString(m->extra_header_text);

        // Part 3: first page cross reference table and trailer.

        qpdf_offset_t first_xref_offset = m->pipeline->getCount();
        qpdf_offset_t hint_offset = 0;
        if (pass == 2) {
            hint_offset = m->new_obj[hint_id].xref.getOffset();
        }
        if (need_xref_stream) {
            // Must pad here too.
            if (pass == 1) {
                // Set first_half_max_obj_offset to a value large enough to force four bytes to be
                // reserved for each file offset.  This would provide adequate space for the xref
                // stream as long as the last object in page 1 starts with in the first 4 GB of the
                // file, which is extremely likely.  In the second pass, we will know the actual
                // value for this, but it's okay if it's smaller.
                first_half_max_obj_offset = 1 << 25;
            }
            pos = m->pipeline->getCount();
            writeXRefStream(
                first_half_xref,
                first_half_end,
                first_half_max_obj_offset,
                t_lin_first,
                first_half_start,
                first_half_end,
                first_trailer_size,
                hint_length + second_xref_offset,
                hint_id,
                hint_offset,
                hint_length,
                (pass == 1),
                pass);
            qpdf_offset_t endpos = m->pipeline->getCount();
            if (pass == 1) {
                // Pad so we have enough room for the real xref stream.
                writePad(calculateXrefStreamPadding(endpos - pos));
                first_xref_end = m->pipeline->getCount();
            } else {
                // Pad so that the next object starts at the same place as in pass 1.
                writePad(QIntC::to_size(first_xref_end - endpos));

                if (m->pipeline->getCount() != first_xref_end) {
                    throw std::logic_error(
                        "insufficient padding for first pass xref stream; "
                        "first_xref_end=" +
                        std::to_string(first_xref_end) + "; endpos=" + std::to_string(endpos));
                }
            }
            writeString("\n");
        } else {
            writeXRefTable(
                t_lin_first,
                first_half_start,
                first_half_end,
                first_trailer_size,
                hint_length + second_xref_offset,
                (pass == 1),
                hint_id,
                hint_offset,
                hint_length,
                pass);
            writeString("startxref\n0\n%%EOF\n");
        }

        // Parts 4 through 9

        for (auto const& cur_object: m->object_queue) {
            if (cur_object.getObjectID() == part6_end_marker) {
                first_half_max_obj_offset = m->pipeline->getCount();
            }
            writeObject(cur_object);
            if (cur_object.getObjectID() == part4_end_marker) {
                if (m->encrypted) {
                    writeEncryptionDictionary();
                }
                if (pass == 1) {
                    m->new_obj[hint_id].xref = QPDFXRefEntry(m->pipeline->getCount());
                } else {
                    // Part 5: hint stream
                    writeString(hint_buffer);
                }
            }
            if (cur_object.getObjectID() == part6_end_marker) {
                part6_end_offset = m->pipeline->getCount();
            }
        }

        // Part 10: overflow hint stream -- not used

        // Part 11: main cross reference table and trailer

        second_xref_offset = m->pipeline->getCount();
        if (need_xref_stream) {
            pos = m->pipeline->getCount();
            space_before_zero = writeXRefStream(
                second_half_xref,
                second_half_end,
                second_xref_offset,
                t_lin_second,
                0,
                second_half_end,
                second_trailer_size,
                0,
                0,
                0,
                0,
                (pass == 1),
                pass);
            qpdf_offset_t endpos = m->pipeline->getCount();

            if (pass == 1) {
                // Pad so we have enough room for the real xref stream.  See comments for previous
                // xref stream on how we calculate the padding.
                writePad(calculateXrefStreamPadding(endpos - pos));
                writeString("\n");
                second_xref_end = m->pipeline->getCount();
            } else {
                // Make the file size the same.
                writePad(
                    QIntC::to_size(second_xref_end + hint_length - 1 - m->pipeline->getCount()));
                writeString("\n");

                // If this assertion fails, maybe we didn't have enough padding above.
                if (m->pipeline->getCount() != second_xref_end + hint_length) {
                    throw std::logic_error(
                        "count mismatch after xref stream; possible insufficient padding?");
                }
            }
        } else {
            space_before_zero = writeXRefTable(
                t_lin_second, 0, second_half_end, second_trailer_size, 0, false, 0, 0, 0, pass);
        }
        writeString("startxref\n");
        writeString(std::to_string(first_xref_offset));
        writeString("\n%%EOF\n");

        if (pass == 1) {
            if (m->deterministic_id) {
                QTC::TC("qpdf", "QPDFWriter linearized deterministic ID", need_xref_stream ? 0 : 1);
                computeDeterministicIDData();
                pp_md5 = nullptr;
                qpdf_assert_debug(m->md5_pipeline == nullptr);
            }

            // Close first pass pipeline
            file_size = m->pipeline->getCount();
            pp_pass1 = nullptr;

            // Save hint offset since it will be set to zero by calling openObject.
            qpdf_offset_t hint_offset1 = m->new_obj[hint_id].xref.getOffset();

            // Write hint stream to a buffer
            {
                PipelinePopper pp_hint(this);
                activatePipelineStack(pp_hint, hint_buffer);
                writeHintStream(hint_id);
            }
            hint_length = QIntC::to_offset(hint_buffer.size());

            // Restore hint offset
            m->new_obj[hint_id].xref = QPDFXRefEntry(hint_offset1);
            if (lin_pass1_file) {
                // Write some debugging information
                fprintf(
                    lin_pass1_file, "%% hint_offset=%s\n", std::to_string(hint_offset1).c_str());
                fprintf(lin_pass1_file, "%% hint_length=%s\n", std::to_string(hint_length).c_str());
                fprintf(
                    lin_pass1_file,
                    "%% second_xref_offset=%s\n",
                    std::to_string(second_xref_offset).c_str());
                fprintf(
                    lin_pass1_file,
                    "%% second_xref_end=%s\n",
                    std::to_string(second_xref_end).c_str());
                fclose(lin_pass1_file);
                lin_pass1_file = nullptr;
            }
        }
    }
}

void
QPDFWriter::enqueueObjectsStandard()
{
    if (m->preserve_unreferenced_objects) {
        QTC::TC("qpdf", "QPDFWriter preserve unreferenced standard");
        for (auto const& oh: m->pdf.getAllObjects()) {
            enqueueObject(oh);
        }
    }

    // Put root first on queue.
    QPDFObjectHandle trailer = getTrimmedTrailer();
    enqueueObject(trailer.getKey("/Root"));

    // Next place any other objects referenced from the trailer dictionary into the queue, handling
    // direct objects recursively. Root is already there, so enqueuing it a second time is a no-op.
    for (auto& item: trailer.as_dictionary()) {
        if (!item.second.null()) {
            enqueueObject(item.second);
        }
    }
}

void
QPDFWriter::enqueueObjectsPCLm()
{
    // Image transform stream content for page strip images. Each of this new stream has to come
    // after every page image strip written in the pclm file.
    std::string image_transform_content = "q /image Do Q\n";

    // enqueue all pages first
    std::vector<QPDFObjectHandle> all = m->pdf.getAllPages();
    for (auto& page: all) {
        // enqueue page
        enqueueObject(page);

        // enqueue page contents stream
        enqueueObject(page.getKey("/Contents"));

        // enqueue all the strips for each page
        QPDFObjectHandle strips = page.getKey("/Resources").getKey("/XObject");
        for (auto& image: strips.as_dictionary()) {
            if (!image.second.null()) {
                enqueueObject(image.second);
                enqueueObject(QPDFObjectHandle::newStream(&m->pdf, image_transform_content));
            }
        }
    }

    // Put root in queue.
    QPDFObjectHandle trailer = getTrimmedTrailer();
    enqueueObject(trailer.getKey("/Root"));
}

void
QPDFWriter::indicateProgress(bool decrement, bool finished)
{
    if (decrement) {
        --m->events_seen;
        return;
    }

    ++m->events_seen;

    if (!m->progress_reporter.get()) {
        return;
    }

    if (finished || (m->events_seen >= m->next_progress_report)) {
        int percentage =
            (finished ? 100
                 : m->next_progress_report == 0
                 ? 0
                 : std::min(99, 1 + ((100 * m->events_seen) / m->events_expected)));
        m->progress_reporter->reportProgress(percentage);
    }
    int increment = std::max(1, (m->events_expected / 100));
    while (m->events_seen >= m->next_progress_report) {
        m->next_progress_report += increment;
    }
}

void
QPDFWriter::registerProgressReporter(std::shared_ptr<ProgressReporter> pr)
{
    m->progress_reporter = pr;
}

void
QPDFWriter::writeStandard()
{
    auto pp_md5 = PipelinePopper(this);
    if (m->deterministic_id) {
        pushMD5Pipeline(pp_md5);
    }

    // Start writing

    writeHeader();
    writeString(m->extra_header_text);

    if (m->pclm) {
        enqueueObjectsPCLm();
    } else {
        enqueueObjectsStandard();
    }

    // Now start walking queue, outputting each object.
    while (m->object_queue_front < m->object_queue.size()) {
        QPDFObjectHandle cur_object = m->object_queue.at(m->object_queue_front);
        ++m->object_queue_front;
        writeObject(cur_object);
    }

    // Write out the encryption dictionary, if any
    if (m->encrypted) {
        writeEncryptionDictionary();
    }

    // Now write out xref.  next_objid is now the number of objects.
    qpdf_offset_t xref_offset = m->pipeline->getCount();
    if (m->object_stream_to_objects.empty()) {
        // Write regular cross-reference table
        writeXRefTable(t_normal, 0, m->next_objid - 1, m->next_objid);
    } else {
        // Write cross-reference stream.
        int xref_id = m->next_objid++;
        writeXRefStream(
            xref_id, xref_id, xref_offset, t_normal, 0, m->next_objid - 1, m->next_objid);
    }
    writeString("startxref\n");
    writeString(std::to_string(xref_offset));
    writeString("\n%%EOF\n");

    if (m->deterministic_id) {
        QTC::TC(
            "qpdf",
            "QPDFWriter standard deterministic ID",
            m->object_stream_to_objects.empty() ? 0 : 1);
    }
}
