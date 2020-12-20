#include <qpdf/qpdf-c.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>

#include <list>
#include <string>
#include <stdexcept>
#include <cstring>

struct _qpdf_error
{
    PointerHolder<QPDFExc> exc;
};

struct _qpdf_data
{
    _qpdf_data();
    ~_qpdf_data();

    PointerHolder<QPDF> qpdf;
    PointerHolder<QPDFWriter> qpdf_writer;

    PointerHolder<QPDFExc> error;
    _qpdf_error tmp_error;
    std::list<QPDFExc> warnings;
    std::string tmp_string;

    // Parameters for functions we call
    char const* filename;	// or description
    char const* buffer;
    unsigned long long size;
    char const* password;
    bool write_memory;
    PointerHolder<Buffer> output_buffer;

    // QPDFObjectHandle support
    std::map<qpdf_oh, PointerHolder<QPDFObjectHandle>> oh_cache;
    qpdf_oh next_oh;
    std::set<std::string> cur_iter_dict_keys;
    std::set<std::string>::const_iterator dict_iter;
    std::string cur_dict_key;
};

_qpdf_data::_qpdf_data() :
    write_memory(false),
    next_oh(0)
{
}

_qpdf_data::~_qpdf_data()
{
}

class ProgressReporter: public QPDFWriter::ProgressReporter
{
  public:
    ProgressReporter(void (*handler)(int, void*), void* data);
    virtual ~ProgressReporter() = default;
    virtual void reportProgress(int);

  private:
    void (*handler)(int, void*);
    void* data;
};

ProgressReporter::ProgressReporter(void (*handler)(int, void*), void* data) :
    handler(handler),
    data(data)
{
}

void
ProgressReporter::reportProgress(int progress)
{
    this->handler(progress, this->data);
}

// must set qpdf->filename and qpdf->password
static void call_read(qpdf_data qpdf)
{
    qpdf->qpdf->processFile(qpdf->filename, qpdf->password);
}

// must set qpdf->filename, qpdf->buffer, qpdf->size, and qpdf->password
static void call_read_memory(qpdf_data qpdf)
{
    qpdf->qpdf->processMemoryFile(qpdf->filename, qpdf->buffer,
				  QIntC::to_size(qpdf->size), qpdf->password);
}

// must set qpdf->filename
static void call_init_write(qpdf_data qpdf)
{
    qpdf->qpdf_writer = new QPDFWriter(*(qpdf->qpdf), qpdf->filename);
}

static void call_init_write_memory(qpdf_data qpdf)
{
    qpdf->qpdf_writer = new QPDFWriter(*(qpdf->qpdf));
    qpdf->qpdf_writer->setOutputMemory();
}

static void call_write(qpdf_data qpdf)
{
    qpdf->qpdf_writer->write();
}

static void call_check(qpdf_data qpdf)
{
    QPDFWriter w(*qpdf->qpdf);
    Pl_Discard discard;
    w.setOutputPipeline(&discard);
    w.setDecodeLevel(qpdf_dl_all);
    w.write();
}

static QPDF_ERROR_CODE trap_errors(qpdf_data qpdf, void (*fn)(qpdf_data))
{
    QPDF_ERROR_CODE status = QPDF_SUCCESS;
    try
    {
	fn(qpdf);
    }
    catch (QPDFExc& e)
    {
	qpdf->error = new QPDFExc(e);
	status |= QPDF_ERRORS;
    }
    catch (std::runtime_error& e)
    {
	qpdf->error = new QPDFExc(qpdf_e_system, "", "", 0, e.what());
	status |= QPDF_ERRORS;
    }
    catch (std::exception& e)
    {
	qpdf->error = new QPDFExc(qpdf_e_internal, "", "", 0, e.what());
	status |= QPDF_ERRORS;
    }

    if (qpdf_more_warnings(qpdf))
    {
	status |= QPDF_WARNINGS;
    }
    return status;
}

char const* qpdf_get_qpdf_version()
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_qpdf_version");
    return QPDF::QPDFVersion().c_str();
}

qpdf_data qpdf_init()
{
    QTC::TC("qpdf", "qpdf-c called qpdf_init");
    qpdf_data qpdf = new _qpdf_data();
    qpdf->qpdf = new QPDF();
    return qpdf;
}

void qpdf_cleanup(qpdf_data* qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_cleanup");
    qpdf_oh_release_all(*qpdf);
    delete *qpdf;
    *qpdf = 0;
}

QPDF_BOOL qpdf_more_warnings(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_more_warnings");

    if (qpdf->warnings.empty())
    {
	std::vector<QPDFExc> w = qpdf->qpdf->getWarnings();
	if (! w.empty())
	{
	    qpdf->warnings.assign(w.begin(), w.end());
	}
    }
    if (qpdf->warnings.empty())
    {
	return QPDF_FALSE;
    }
    else
    {
	return QPDF_TRUE;
    }
}

QPDF_BOOL qpdf_has_error(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_has_error");
    return (qpdf->error.getPointer() ? QPDF_TRUE : QPDF_FALSE);
}

qpdf_error qpdf_get_error(qpdf_data qpdf)
{
    if (qpdf->error.getPointer())
    {
	qpdf->tmp_error.exc = qpdf->error;
	qpdf->error = 0;
	QTC::TC("qpdf", "qpdf-c qpdf_get_error returned error");
	return &qpdf->tmp_error;
    }
    else
    {
	return 0;
    }
}

qpdf_error qpdf_next_warning(qpdf_data qpdf)
{
    if (qpdf_more_warnings(qpdf))
    {
	qpdf->tmp_error.exc = new QPDFExc(qpdf->warnings.front());
	qpdf->warnings.pop_front();
	QTC::TC("qpdf", "qpdf-c qpdf_next_warning returned warning");
	return &qpdf->tmp_error;
    }
    else
    {
	return 0;
    }
}

char const* qpdf_get_error_full_text(qpdf_data qpdf, qpdf_error e)
{
    if (e == 0)
    {
	return "";
    }
    return e->exc->what();
}

enum qpdf_error_code_e qpdf_get_error_code(qpdf_data qpdf, qpdf_error e)
{
    if (e == 0)
    {
	return qpdf_e_success;
    }
    return e->exc->getErrorCode();
}

char const* qpdf_get_error_filename(qpdf_data qpdf, qpdf_error e)
{
    if (e == 0)
    {
	return "";
    }
    return e->exc->getFilename().c_str();
}

unsigned long long qpdf_get_error_file_position(qpdf_data qpdf, qpdf_error e)
{
    if (e == 0)
    {
	return 0;
    }
    return QIntC::to_ulonglong(e->exc->getFilePosition());
}

char const* qpdf_get_error_message_detail(qpdf_data qpdf, qpdf_error e)
{
    if (e == 0)
    {
	return "";
    }
    return e->exc->getMessageDetail().c_str();
}

QPDF_ERROR_CODE qpdf_check_pdf(qpdf_data qpdf)
{
    QPDF_ERROR_CODE status = trap_errors(qpdf, &call_check);
    QTC::TC("qpdf", "qpdf-c called qpdf_check_pdf");
    return status;
}

void qpdf_set_suppress_warnings(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_suppress_warnings");
    qpdf->qpdf->setSuppressWarnings(value != QPDF_FALSE);
}

void qpdf_set_ignore_xref_streams(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_ignore_xref_streams");
    qpdf->qpdf->setIgnoreXRefStreams(value != QPDF_FALSE);
}

void qpdf_set_attempt_recovery(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_attempt_recovery");
    qpdf->qpdf->setAttemptRecovery(value != QPDF_FALSE);
}

QPDF_ERROR_CODE qpdf_read(qpdf_data qpdf, char const* filename,
			  char const* password)
{
    QPDF_ERROR_CODE status = QPDF_SUCCESS;
    qpdf->filename = filename;
    qpdf->password = password;
    status = trap_errors(qpdf, &call_read);
    // We no longer have a good way to exercise a file with both
    // warnings and errors because qpdf is getting much better at
    // recovering.
    QTC::TC("qpdf", "qpdf-c called qpdf_read",
            (status == 0) ? 0
            : (status & QPDF_WARNINGS) ? 1
            : (status & QPDF_ERRORS) ? 2 :
            -1
        );
    return status;
}

QPDF_ERROR_CODE qpdf_read_memory(qpdf_data qpdf,
				 char const* description,
				 char const* buffer,
				 unsigned long long size,
				 char const* password)
{
    QPDF_ERROR_CODE status = QPDF_SUCCESS;
    qpdf->filename = description;
    qpdf->buffer = buffer;
    qpdf->size = size;
    qpdf->password = password;
    status = trap_errors(qpdf, &call_read_memory);
    QTC::TC("qpdf", "qpdf-c called qpdf_read_memory", status);
    return status;
}

char const* qpdf_get_pdf_version(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_pdf_version");
    qpdf->tmp_string = qpdf->qpdf->getPDFVersion();
    return qpdf->tmp_string.c_str();
}

int qpdf_get_pdf_extension_level(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_pdf_extension_level");
    return qpdf->qpdf->getExtensionLevel();
}

char const* qpdf_get_user_password(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_user_password");
    qpdf->tmp_string = qpdf->qpdf->getTrimmedUserPassword();
    return qpdf->tmp_string.c_str();
}

char const* qpdf_get_info_key(qpdf_data qpdf, char const* key)
{
    char const* result = 0;
    QPDFObjectHandle trailer = qpdf->qpdf->getTrailer();
    if (trailer.hasKey("/Info"))
    {
	QPDFObjectHandle info = trailer.getKey("/Info");
	if (info.hasKey(key))
	{
	    QPDFObjectHandle value = info.getKey(key);
	    if (value.isString())
	    {
		qpdf->tmp_string = value.getStringValue();
		result = qpdf->tmp_string.c_str();
	    }
	}
    }
    QTC::TC("qpdf", "qpdf-c get_info_key", (result == 0 ? 0 : 1));
    return result;
}

void qpdf_set_info_key(qpdf_data qpdf, char const* key, char const* value)
{
    if ((key == 0) || (std::strlen(key) == 0) || (key[0] != '/'))
    {
	return;
    }
    QPDFObjectHandle value_object;
    if (value)
    {
	QTC::TC("qpdf", "qpdf-c set_info_key to value");
	value_object = QPDFObjectHandle::newString(value);
    }
    else
    {
	QTC::TC("qpdf", "qpdf-c set_info_key to null");
	value_object = QPDFObjectHandle::newNull();
    }

    QPDFObjectHandle trailer = qpdf->qpdf->getTrailer();
    if (! trailer.hasKey("/Info"))
    {
	QTC::TC("qpdf", "qpdf-c add info to trailer");
	trailer.replaceKey(
	    "/Info",
	    qpdf->qpdf->makeIndirectObject(QPDFObjectHandle::newDictionary()));
    }
    else
    {
	QTC::TC("qpdf", "qpdf-c set-info-key use existing info");
    }

    QPDFObjectHandle info = trailer.getKey("/Info");
    info.replaceOrRemoveKey(key, value_object);
}

QPDF_BOOL qpdf_is_linearized(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_is_linearized");
    return (qpdf->qpdf->isLinearized() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_is_encrypted(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_is_encrypted");
    return (qpdf->qpdf->isEncrypted() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_allow_accessibility(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_accessibility");
    return (qpdf->qpdf->allowAccessibility() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_allow_extract_all(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_extract_all");
    return (qpdf->qpdf->allowExtractAll() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_allow_print_low_res(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_print_low_res");
    return (qpdf->qpdf->allowPrintLowRes() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_allow_print_high_res(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_print_high_res");
    return (qpdf->qpdf->allowPrintHighRes() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_allow_modify_assembly(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_assembly");
    return (qpdf->qpdf->allowModifyAssembly() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_allow_modify_form(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_form");
    return (qpdf->qpdf->allowModifyForm() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_allow_modify_annotation(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_annotation");
    return (qpdf->qpdf->allowModifyAnnotation() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_allow_modify_other(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_other");
    return (qpdf->qpdf->allowModifyOther() ? QPDF_TRUE : QPDF_FALSE);
}

QPDF_BOOL qpdf_allow_modify_all(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_all");
    return (qpdf->qpdf->allowModifyAll() ? QPDF_TRUE : QPDF_FALSE);
}

static void qpdf_init_write_internal(qpdf_data qpdf)
{
    if (qpdf->qpdf_writer.getPointer())
    {
	QTC::TC("qpdf", "qpdf-c called qpdf_init_write multiple times");
	qpdf->qpdf_writer = 0;
	if (qpdf->output_buffer.getPointer())
	{
	    qpdf->output_buffer = 0;
	    qpdf->write_memory = false;
	    qpdf->filename = 0;
	}
    }
}

QPDF_ERROR_CODE qpdf_init_write(qpdf_data qpdf, char const* filename)
{
    qpdf_init_write_internal(qpdf);
    qpdf->filename = filename;
    QPDF_ERROR_CODE status = trap_errors(qpdf, &call_init_write);
    QTC::TC("qpdf", "qpdf-c called qpdf_init_write", status);
    return status;
}

QPDF_ERROR_CODE qpdf_init_write_memory(qpdf_data qpdf)
{
    qpdf_init_write_internal(qpdf);
    QPDF_ERROR_CODE status = trap_errors(qpdf, &call_init_write_memory);
    QTC::TC("qpdf", "qpdf-c called qpdf_init_write_memory");
    qpdf->write_memory = true;
    return status;
}

static void qpdf_get_buffer_internal(qpdf_data qpdf)
{
    if (qpdf->write_memory && (qpdf->output_buffer == 0))
    {
	qpdf->output_buffer = qpdf->qpdf_writer->getBuffer();
    }
}

size_t qpdf_get_buffer_length(qpdf_data qpdf)
{
    qpdf_get_buffer_internal(qpdf);
    size_t result = 0;
    if (qpdf->output_buffer.getPointer())
    {
	result = qpdf->output_buffer->getSize();
    }
    return result;
}

unsigned char const* qpdf_get_buffer(qpdf_data qpdf)
{
    unsigned char const* result = 0;
    qpdf_get_buffer_internal(qpdf);
    if (qpdf->output_buffer.getPointer())
    {
	result = qpdf->output_buffer->getBuffer();
    }
    return result;
}

void qpdf_set_object_stream_mode(qpdf_data qpdf, qpdf_object_stream_e mode)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_object_stream_mode");
    qpdf->qpdf_writer->setObjectStreamMode(mode);
}

void qpdf_set_compress_streams(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_compress_streams");
    qpdf->qpdf_writer->setCompressStreams(value != QPDF_FALSE);
}

void qpdf_set_decode_level(qpdf_data qpdf, qpdf_stream_decode_level_e level)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_decode_level");
    qpdf->qpdf_writer->setDecodeLevel(level);
}

void qpdf_set_preserve_unreferenced_objects(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_preserve_unreferenced_objects");
    qpdf->qpdf_writer->setPreserveUnreferencedObjects(value != QPDF_FALSE);
}

void qpdf_set_newline_before_endstream(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_newline_before_endstream");
    qpdf->qpdf_writer->setNewlineBeforeEndstream(value != QPDF_FALSE);
}

void qpdf_set_stream_data_mode(qpdf_data qpdf, qpdf_stream_data_e mode)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_stream_data_mode");
    qpdf->qpdf_writer->setStreamDataMode(mode);
}

void qpdf_set_content_normalization(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_content_normalization");
    qpdf->qpdf_writer->setContentNormalization(value != QPDF_FALSE);
}

void qpdf_set_qdf_mode(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_qdf_mode");
    qpdf->qpdf_writer->setQDFMode(value != QPDF_FALSE);
}

void qpdf_set_deterministic_ID(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_deterministic_ID");
    qpdf->qpdf_writer->setDeterministicID(value != QPDF_FALSE);
}

void qpdf_set_static_ID(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_static_ID");
    qpdf->qpdf_writer->setStaticID(value != QPDF_FALSE);
}

void qpdf_set_static_aes_IV(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_static_aes_IV");
    qpdf->qpdf_writer->setStaticAesIV(value != QPDF_FALSE);
}

void qpdf_set_suppress_original_object_IDs(
    qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_suppress_original_object_IDs");
    qpdf->qpdf_writer->setSuppressOriginalObjectIDs(value != QPDF_FALSE);
}

void qpdf_set_preserve_encryption(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_preserve_encryption");
    qpdf->qpdf_writer->setPreserveEncryption(value != QPDF_FALSE);
}

void qpdf_set_r2_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_print, QPDF_BOOL allow_modify,
    QPDF_BOOL allow_extract, QPDF_BOOL allow_annotate)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_r2_encryption_parameters");
    qpdf->qpdf_writer->setR2EncryptionParameters(
	user_password, owner_password,
	allow_print != QPDF_FALSE, allow_modify != QPDF_FALSE,
        allow_extract != QPDF_FALSE, allow_annotate != QPDF_FALSE);
}

void qpdf_set_r3_encryption_parameters2(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    QPDF_BOOL allow_assemble, QPDF_BOOL allow_annotate_and_form,
    QPDF_BOOL allow_form_filling, QPDF_BOOL allow_modify_other,
    enum qpdf_r3_print_e print)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_r3_encryption_parameters");
    qpdf->qpdf_writer->setR3EncryptionParameters(
        user_password, owner_password,
        allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        allow_assemble != QPDF_FALSE, allow_annotate_and_form != QPDF_FALSE,
        allow_form_filling != QPDF_FALSE, allow_modify_other != QPDF_FALSE,
        print);
}

void qpdf_set_r4_encryption_parameters2(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    QPDF_BOOL allow_assemble, QPDF_BOOL allow_annotate_and_form,
    QPDF_BOOL allow_form_filling, QPDF_BOOL allow_modify_other,
    enum qpdf_r3_print_e print,
    QPDF_BOOL encrypt_metadata, QPDF_BOOL use_aes)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_r4_encryption_parameters");
    qpdf->qpdf_writer->setR4EncryptionParameters(
        user_password, owner_password,
        allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        allow_assemble != QPDF_FALSE, allow_annotate_and_form != QPDF_FALSE,
        allow_form_filling != QPDF_FALSE, allow_modify_other != QPDF_FALSE,
        print, encrypt_metadata != QPDF_FALSE, use_aes != QPDF_FALSE);
}


void qpdf_set_r5_encryption_parameters2(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    QPDF_BOOL allow_assemble, QPDF_BOOL allow_annotate_and_form,
    QPDF_BOOL allow_form_filling, QPDF_BOOL allow_modify_other,
    enum qpdf_r3_print_e print, QPDF_BOOL encrypt_metadata)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_r5_encryption_parameters");
    qpdf->qpdf_writer->setR5EncryptionParameters(
        user_password, owner_password,
        allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        allow_assemble != QPDF_FALSE, allow_annotate_and_form != QPDF_FALSE,
        allow_form_filling != QPDF_FALSE, allow_modify_other != QPDF_FALSE,
        print, encrypt_metadata != QPDF_FALSE);
}

void qpdf_set_r6_encryption_parameters2(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    QPDF_BOOL allow_assemble, QPDF_BOOL allow_annotate_and_form,
    QPDF_BOOL allow_form_filling, QPDF_BOOL allow_modify_other,
    enum qpdf_r3_print_e print, QPDF_BOOL encrypt_metadata)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_r6_encryption_parameters");
    qpdf->qpdf_writer->setR6EncryptionParameters(
        user_password, owner_password,
        allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        allow_assemble != QPDF_FALSE, allow_annotate_and_form != QPDF_FALSE,
        allow_form_filling != QPDF_FALSE, allow_modify_other != QPDF_FALSE,
        print, encrypt_metadata != QPDF_FALSE);
}

void qpdf_set_r3_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify)
{
    qpdf->qpdf_writer->setR3EncryptionParameters(
	user_password, owner_password,
	allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        print, modify);
}

void qpdf_set_r4_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify,
    QPDF_BOOL encrypt_metadata, QPDF_BOOL use_aes)
{
    qpdf->qpdf_writer->setR4EncryptionParameters(
	user_password, owner_password,
	allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        print, modify,
	encrypt_metadata != QPDF_FALSE, use_aes != QPDF_FALSE);
}

void qpdf_set_r5_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify,
    QPDF_BOOL encrypt_metadata)
{
    qpdf->qpdf_writer->setR5EncryptionParameters(
	user_password, owner_password,
	allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        print, modify,
	encrypt_metadata != QPDF_FALSE);
}

void qpdf_set_r6_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify,
    QPDF_BOOL encrypt_metadata)
{
    qpdf->qpdf_writer->setR6EncryptionParameters(
	user_password, owner_password,
	allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        print, modify, encrypt_metadata != QPDF_FALSE);
}

void qpdf_set_linearization(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_linearization");
    qpdf->qpdf_writer->setLinearization(value != QPDF_FALSE);
}

void qpdf_set_minimum_pdf_version(qpdf_data qpdf, char const* version)
{
    qpdf_set_minimum_pdf_version_and_extension(qpdf, version, 0);
}

void qpdf_set_minimum_pdf_version_and_extension(
    qpdf_data qpdf, char const* version, int extension_level)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_minimum_pdf_version");
    qpdf->qpdf_writer->setMinimumPDFVersion(version, extension_level);
}

void qpdf_force_pdf_version(qpdf_data qpdf, char const* version)
{
    qpdf_force_pdf_version_and_extension(qpdf, version, 0);
}

void qpdf_force_pdf_version_and_extension(
    qpdf_data qpdf, char const* version, int extension_level)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_force_pdf_version");
    qpdf->qpdf_writer->forcePDFVersion(version, extension_level);
}

void qpdf_register_progress_reporter(
    qpdf_data qpdf,
    void (*report_progress)(int percent, void* data),
    void* data)
{
    qpdf->qpdf_writer->registerProgressReporter(
        new ProgressReporter(report_progress, data));
}

QPDF_ERROR_CODE qpdf_write(qpdf_data qpdf)
{
    QPDF_ERROR_CODE status = QPDF_SUCCESS;
    status = trap_errors(qpdf, &call_write);
    QTC::TC("qpdf", "qpdf-c called qpdf_write", (status == 0) ? 0 : 1);
    return status;
}

static qpdf_oh
new_object(qpdf_data qpdf, QPDFObjectHandle const& qoh)
{
    qpdf_oh oh = ++qpdf->next_oh; // never return 0
    qpdf->oh_cache[oh] = new QPDFObjectHandle(qoh);
    return oh;
}

void qpdf_oh_release(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_release");
    qpdf->oh_cache.erase(oh);
}

void qpdf_oh_release_all(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_release_all");
    qpdf->oh_cache.clear();
}

qpdf_oh qpdf_get_trailer(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_trailer");
    return new_object(qpdf, qpdf->qpdf->getTrailer());
}

qpdf_oh qpdf_get_root(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_root");
    return new_object(qpdf, qpdf->qpdf->getRoot());
}

static bool
qpdf_oh_valid_internal(qpdf_data qpdf, qpdf_oh oh)
{
    auto i = qpdf->oh_cache.find(oh);
    bool result = ((i != qpdf->oh_cache.end()) &&
                   (i->second).getPointer() &&
                   (i->second)->isInitialized());
    if (! result)
    {
        QTC::TC("qpdf", "qpdf-c invalid object handle");
        qpdf->warnings.push_back(
            QPDFExc(
                qpdf_e_damaged_pdf,
                qpdf->qpdf->getFilename(),
                std::string("C API object handle ") +
                QUtil::uint_to_string(oh),
                0, "attempted access to unknown/uninitialized object handle"));
    }
    return result;
}

QPDF_BOOL qpdf_oh_is_bool(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_bool");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isBool());
}

QPDF_BOOL qpdf_oh_is_null(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_null");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isNull());
}

QPDF_BOOL qpdf_oh_is_integer(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_integer");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isInteger());
}

QPDF_BOOL qpdf_oh_is_real(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_real");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isReal());
}

QPDF_BOOL qpdf_oh_is_name(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_name");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isName());
}

QPDF_BOOL qpdf_oh_is_string(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_string");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isString());
}

QPDF_BOOL qpdf_oh_is_operator(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_operator");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isOperator());
}

QPDF_BOOL qpdf_oh_is_inline_image(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_inline_image");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isInlineImage());
}

QPDF_BOOL qpdf_oh_is_array(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_array");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isArray());
}

QPDF_BOOL qpdf_oh_is_dictionary(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_dictionary");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isDictionary());
}

QPDF_BOOL qpdf_oh_is_stream(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_stream");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isStream());
}

QPDF_BOOL qpdf_oh_is_indirect(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_indirect");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isIndirect());
}

QPDF_BOOL qpdf_oh_is_scalar(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_scalar");
    return (qpdf_oh_valid_internal(qpdf, oh) &&
            qpdf->oh_cache[oh]->isScalar());
}

qpdf_oh qpdf_oh_wrap_in_array(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return qpdf_oh_new_array(qpdf);
    }
    auto qoh = qpdf->oh_cache[oh];
    if (qoh->isArray())
    {
        QTC::TC("qpdf", "qpdf-c array to wrap_in_array");
        return oh;
    }
    else
    {
        QTC::TC("qpdf", "qpdf-c non-array to wrap_in_array");
        return new_object(qpdf,
                          QPDFObjectHandle::newArray(
                              std::vector<QPDFObjectHandle>{
                                  *qpdf->oh_cache[oh]}));
    }
}

qpdf_oh qpdf_oh_parse(qpdf_data qpdf, char const* object_str)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_parse");
    return new_object(qpdf, QPDFObjectHandle::parse(object_str));
}

QPDF_BOOL qpdf_oh_get_bool_value(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return QPDF_FALSE;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_bool_value");
    return qpdf->oh_cache[oh]->getBoolValue();
}

long long qpdf_oh_get_int_value(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return 0LL;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_int_value");
    return qpdf->oh_cache[oh]->getIntValue();
}

int qpdf_oh_get_int_value_as_int(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return 0;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_int_value_as_int");
    return qpdf->oh_cache[oh]->getIntValueAsInt();
}

unsigned long long qpdf_oh_get_uint_value(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return 0ULL;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_uint_value");
    return qpdf->oh_cache[oh]->getUIntValue();
}

unsigned int qpdf_oh_get_uint_value_as_uint(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return 0U;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_uint_value_as_uint");
    return qpdf->oh_cache[oh]->getUIntValueAsUInt();
}

char const* qpdf_oh_get_real_value(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return "";
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_real_value");
    qpdf->tmp_string = qpdf->oh_cache[oh]->getRealValue();
    return qpdf->tmp_string.c_str();
}

QPDF_BOOL qpdf_oh_is_number(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return QPDF_FALSE;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_number");
    return qpdf->oh_cache[oh]->isNumber();
}

double qpdf_oh_get_numeric_value(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return 0.0;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_numeric_value");
    return qpdf->oh_cache[oh]->getNumericValue();
}

char const* qpdf_oh_get_name(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return "";
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_name");
    qpdf->tmp_string = qpdf->oh_cache[oh]->getName();
    return qpdf->tmp_string.c_str();
}

char const* qpdf_oh_get_string_value(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return "";
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_string_value");
    qpdf->tmp_string = qpdf->oh_cache[oh]->getStringValue();
    return qpdf->tmp_string.c_str();
}

char const* qpdf_oh_get_utf8_value(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return "";
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_utf8_value");
    qpdf->tmp_string = qpdf->oh_cache[oh]->getUTF8Value();
    return qpdf->tmp_string.c_str();
}

int qpdf_oh_get_array_n_items(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return 0;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_array_n_items");
    return qpdf->oh_cache[oh]->getArrayNItems();
}

qpdf_oh qpdf_oh_get_array_item(qpdf_data qpdf, qpdf_oh oh, int n)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return qpdf_oh_new_null(qpdf);
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_array_item");
    return new_object(qpdf, qpdf->oh_cache[oh]->getArrayItem(n));
}

void qpdf_oh_begin_dict_key_iter(qpdf_data qpdf, qpdf_oh oh)
{
    if (qpdf_oh_valid_internal(qpdf, oh) &&
        qpdf_oh_is_dictionary(qpdf, oh))
    {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_begin_dict_key_iter");
        qpdf->cur_iter_dict_keys = qpdf->oh_cache[oh]->getKeys();
    }
    else
    {
        qpdf->cur_iter_dict_keys = {};
    }
    qpdf->dict_iter = qpdf->cur_iter_dict_keys.begin();
}

QPDF_BOOL qpdf_oh_dict_more_keys(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_dict_more_keys");
    return qpdf->dict_iter != qpdf->cur_iter_dict_keys.end();
}

char const* qpdf_oh_dict_next_key(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_dict_next_key");
    if (qpdf_oh_dict_more_keys(qpdf))
    {
        qpdf->cur_dict_key = *qpdf->dict_iter;
        ++qpdf->dict_iter;
        return qpdf->cur_dict_key.c_str();
    }
    else
    {
        return nullptr;
    }
}

QPDF_BOOL qpdf_oh_has_key(qpdf_data qpdf, qpdf_oh oh, char const* key)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return QPDF_FALSE;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_has_key");
    return qpdf->oh_cache[oh]->hasKey(key);
}

qpdf_oh qpdf_oh_get_key(qpdf_data qpdf, qpdf_oh oh, char const* key)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return qpdf_oh_new_null(qpdf);
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_key");
    return new_object(qpdf, qpdf->oh_cache[oh]->getKey(key));
}

QPDF_BOOL qpdf_oh_is_or_has_name(qpdf_data qpdf, qpdf_oh oh, char const* key)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return QPDF_FALSE;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_or_has_name");
    return qpdf->oh_cache[oh]->isOrHasName(key);
}

qpdf_oh qpdf_oh_new_null(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_null");
    return new_object(qpdf, QPDFObjectHandle::newNull());
}

qpdf_oh qpdf_oh_new_bool(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_bool");
    return new_object(qpdf, QPDFObjectHandle::newBool(value));
}

qpdf_oh qpdf_oh_new_integer(qpdf_data qpdf, long long value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_integer");
    return new_object(qpdf, QPDFObjectHandle::newInteger(value));
}

qpdf_oh qpdf_oh_new_real_from_string(qpdf_data qpdf, char const* value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_real_from_string");
    return new_object(qpdf, QPDFObjectHandle::newReal(value));
}

qpdf_oh qpdf_oh_new_real_from_double(qpdf_data qpdf,
                                     double value, int decimal_places)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_real_from_double");
    return new_object(qpdf, QPDFObjectHandle::newReal(value, decimal_places));
}

qpdf_oh qpdf_oh_new_name(qpdf_data qpdf, char const* name)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_name");
    return new_object(qpdf, QPDFObjectHandle::newName(name));
}

qpdf_oh qpdf_oh_new_string(qpdf_data qpdf, char const* str)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_string");
    return new_object(qpdf, QPDFObjectHandle::newString(str));
}

qpdf_oh qpdf_oh_new_unicode_string(qpdf_data qpdf, char const* utf8_str)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_unicode_string");
    return new_object(qpdf, QPDFObjectHandle::newUnicodeString(utf8_str));
}

qpdf_oh qpdf_oh_new_array(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_array");
    return new_object(qpdf, QPDFObjectHandle::newArray());
}

qpdf_oh qpdf_oh_new_dictionary(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_dictionary");
    return new_object(qpdf, QPDFObjectHandle::newDictionary());
}

void qpdf_oh_make_direct(qpdf_data qpdf, qpdf_oh oh)
{
    if (qpdf_oh_valid_internal(qpdf, oh))
    {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_make_direct");
        qpdf->oh_cache[oh]->makeDirect();
    }
}

static QPDFObjectHandle
qpdf_oh_item_internal(qpdf_data qpdf, qpdf_oh item)
{
    if (qpdf_oh_valid_internal(qpdf, item))
    {
        return *(qpdf->oh_cache[item]);
    }
    else
    {
        return QPDFObjectHandle::newNull();
    }
}

void qpdf_oh_set_array_item(qpdf_data qpdf, qpdf_oh oh,
                            int at, qpdf_oh item)
{
    if (qpdf_oh_is_array(qpdf, oh))
    {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_set_array_item");
        qpdf->oh_cache[oh]->setArrayItem(
            at, qpdf_oh_item_internal(qpdf, item));
    }
}

void qpdf_oh_insert_item(qpdf_data qpdf, qpdf_oh oh, int at, qpdf_oh item)
{
    if (qpdf_oh_is_array(qpdf, oh))
    {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_insert_item");
        qpdf->oh_cache[oh]->insertItem(
            at, qpdf_oh_item_internal(qpdf, item));
    }
}

void qpdf_oh_append_item(qpdf_data qpdf, qpdf_oh oh, qpdf_oh item)
{
    if (qpdf_oh_is_array(qpdf, oh))
    {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_append_item");
        qpdf->oh_cache[oh]->appendItem(
            qpdf_oh_item_internal(qpdf, item));
    }
}

void qpdf_oh_erase_item(qpdf_data qpdf, qpdf_oh oh, int at)
{
    if (qpdf_oh_is_array(qpdf, oh))
    {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_erase_item");
        qpdf->oh_cache[oh]->eraseItem(at);
    }
}

void qpdf_oh_replace_key(qpdf_data qpdf, qpdf_oh oh,
                         char const* key, qpdf_oh item)
{
    if (qpdf_oh_is_dictionary(qpdf, oh))
    {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_replace_key");
        qpdf->oh_cache[oh]->replaceKey(
            key, qpdf_oh_item_internal(qpdf, item));
    }
}

void qpdf_oh_remove_key(qpdf_data qpdf, qpdf_oh oh, char const* key)
{
    if (qpdf_oh_is_dictionary(qpdf, oh))
    {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_remove_key");
        qpdf->oh_cache[oh]->removeKey(key);
    }
}

void qpdf_oh_replace_or_remove_key(qpdf_data qpdf, qpdf_oh oh,
                                   char const* key, qpdf_oh item)
{
    if (qpdf_oh_is_dictionary(qpdf, oh))
    {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_replace_or_remove_key");
        qpdf->oh_cache[oh]->replaceOrRemoveKey(
            key, qpdf_oh_item_internal(qpdf, item));
    }
}

qpdf_oh qpdf_oh_get_dict(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return qpdf_oh_new_null(qpdf);
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_dict");
    return new_object(qpdf, qpdf->oh_cache[oh]->getDict());
}

int qpdf_oh_get_object_id(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return 0;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_object_id");
    return qpdf->oh_cache[oh]->getObjectID();
}

int qpdf_oh_get_generation(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return 0;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_generation");
    return qpdf->oh_cache[oh]->getGeneration();
}

char const* qpdf_oh_unparse(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return "";
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_unparse");
    qpdf->tmp_string = qpdf->oh_cache[oh]->unparse();
    return qpdf->tmp_string.c_str();
}

char const* qpdf_oh_unparse_resolved(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return "";
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_unparse_resolved");
    qpdf->tmp_string = qpdf->oh_cache[oh]->unparseResolved();
    return qpdf->tmp_string.c_str();
}

char const* qpdf_oh_unparse_binary(qpdf_data qpdf, qpdf_oh oh)
{
    if (! qpdf_oh_valid_internal(qpdf, oh))
    {
        return "";
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_unparse_binary");
    qpdf->tmp_string = qpdf->oh_cache[oh]->unparseBinary();
    return qpdf->tmp_string.c_str();
}
