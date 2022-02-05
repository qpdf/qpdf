#include <qpdf/qpdf-c.h>

#include <qpdf/QPDF.hh>

#include <qpdf/QPDFWriter.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>

#include <list>
#include <string>
#include <stdexcept>
#include <cstring>
#include <functional>

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
    bool silence_errors;
    bool oh_error_occurred;
    std::map<qpdf_oh, PointerHolder<QPDFObjectHandle>> oh_cache;
    qpdf_oh next_oh;
    std::set<std::string> cur_iter_dict_keys;
    std::set<std::string>::const_iterator dict_iter;
    std::string cur_dict_key;
};

_qpdf_data::_qpdf_data() :
    write_memory(false),
    silence_errors(false),
    oh_error_occurred(false),
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

static QPDF_ERROR_CODE trap_errors(
    qpdf_data qpdf, std::function<void(qpdf_data)> fn)
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
    // The API guarantees that this is a static value.
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
    if ((*qpdf)->error.get())
    {
        QTC::TC("qpdf", "qpdf-c cleanup warned about unhandled error");
        std::cerr << "WARNING: application did not handle error: "
                  <<  (*qpdf)->error->what() << std::endl;

    }
    delete *qpdf;
    *qpdf = 0;
}

size_t qpdf_get_last_string_length(qpdf_data qpdf)
{
    return qpdf->tmp_string.length();
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
    return (qpdf->error.get() ? QPDF_TRUE : QPDF_FALSE);
}

qpdf_error qpdf_get_error(qpdf_data qpdf)
{
    if (qpdf->error.get())
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

QPDF_ERROR_CODE qpdf_empty_pdf(qpdf_data qpdf)
{
    qpdf->filename = "empty PDF";
    qpdf->qpdf->emptyPDF();
    QTC::TC("qpdf", "qpdf-c called qpdf_empty_pdf");
    return QPDF_SUCCESS;
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
    if (qpdf->qpdf_writer.get())
    {
	QTC::TC("qpdf", "qpdf-c called qpdf_init_write multiple times");
	qpdf->qpdf_writer = 0;
	if (qpdf->output_buffer.get())
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
    if (qpdf->output_buffer.get())
    {
	result = qpdf->output_buffer->getSize();
    }
    return result;
}

unsigned char const* qpdf_get_buffer(qpdf_data qpdf)
{
    unsigned char const* result = 0;
    qpdf_get_buffer_internal(qpdf);
    if (qpdf->output_buffer.get())
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
#ifdef _MSC_VER
# pragma warning (disable: 4996)
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    qpdf->qpdf_writer->setR3EncryptionParameters(
	user_password, owner_password,
	allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        print, modify);
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif
}

void qpdf_set_r4_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify,
    QPDF_BOOL encrypt_metadata, QPDF_BOOL use_aes)
{
#ifdef _MSC_VER
# pragma warning (disable: 4996)
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    qpdf->qpdf_writer->setR4EncryptionParameters(
	user_password, owner_password,
	allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        print, modify,
	encrypt_metadata != QPDF_FALSE, use_aes != QPDF_FALSE);
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif
}

void qpdf_set_r5_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify,
    QPDF_BOOL encrypt_metadata)
{
#ifdef _MSC_VER
# pragma warning (disable: 4996)
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    qpdf->qpdf_writer->setR5EncryptionParameters(
	user_password, owner_password,
	allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        print, modify,
	encrypt_metadata != QPDF_FALSE);
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif
}

void qpdf_set_r6_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify,
    QPDF_BOOL encrypt_metadata)
{
#ifdef _MSC_VER
# pragma warning (disable: 4996)
#endif
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    qpdf->qpdf_writer->setR6EncryptionParameters(
	user_password, owner_password,
	allow_accessibility != QPDF_FALSE, allow_extract != QPDF_FALSE,
        print, modify, encrypt_metadata != QPDF_FALSE);
#if (defined(__GNUC__) || defined(__clang__))
# pragma GCC diagnostic pop
#endif
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
    QTC::TC("qpdf", "qpdf-c registered progress reporter");
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

void qpdf_silence_errors(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c silence oh errors");
    qpdf->silence_errors = true;
}

template<class RET>
static RET trap_oh_errors(
    qpdf_data qpdf,
    std::function<RET()> fallback,
    std::function<RET(qpdf_data)> fn)
{
    // Note: fallback is a function so we don't have to evaluate it
    // unless needed. This is important because sometimes the fallback
    // creates an object.
    RET ret;
    QPDF_ERROR_CODE status = trap_errors(qpdf, [&ret, fn] (qpdf_data q) {
        ret = fn(q);
    });
    if (status & QPDF_ERRORS)
    {
        if (! qpdf->silence_errors)
        {
            QTC::TC("qpdf", "qpdf-c warn about oh error",
                    qpdf->oh_error_occurred ? 0 : 1);
            if (! qpdf->oh_error_occurred)
            {
                qpdf->warnings.push_back(
                    QPDFExc(
                        qpdf_e_internal,
                        qpdf->qpdf->getFilename(),
                        "", 0,
                        "C API function caught an exception that it isn't"
                        " returning; please point the application developer"
                        " to ERROR HANDLING in qpdf-c.h"));
                qpdf->oh_error_occurred = true;
            }
            std::cerr << qpdf->error->what() << std::endl;
        }
        return fallback();
    }
    return ret;
}

static qpdf_oh
new_object(qpdf_data qpdf, QPDFObjectHandle const& qoh)
{
    qpdf_oh oh = ++qpdf->next_oh; // never return 0
    qpdf->oh_cache[oh] = new QPDFObjectHandle(qoh);
    return oh;
}

qpdf_oh qpdf_oh_new_object(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_new_object");
    return new_object(qpdf, *(qpdf->oh_cache[oh]));
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

template <class T>
static std::function<T()> return_T(T const& r)
{
    return [&r]() { return r; };
}

static QPDF_BOOL return_false()
{
    return QPDF_FALSE;
}

static std::function<qpdf_oh()> return_uninitialized(qpdf_data qpdf)
{
    return [qpdf]() { return qpdf_oh_new_uninitialized(qpdf); };
}

static std::function<qpdf_oh()> return_null(qpdf_data qpdf)
{
    return [qpdf]() { return qpdf_oh_new_null(qpdf); };
}

qpdf_oh qpdf_get_trailer(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_trailer");
    return trap_oh_errors<qpdf_oh>(
        qpdf, return_uninitialized(qpdf), [] (qpdf_data q) {
            return new_object(q, q->qpdf->getTrailer());
        });
}

qpdf_oh qpdf_get_root(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_root");
    return trap_oh_errors<qpdf_oh>(
        qpdf, return_uninitialized(qpdf), [] (qpdf_data q) {
            return new_object(q, q->qpdf->getRoot());
        });
}

qpdf_oh qpdf_get_object_by_id(qpdf_data qpdf, int objid, int generation)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_object_by_id");
    return new_object(qpdf, qpdf->qpdf->getObjectByID(objid, generation));
}

template<class RET>
static RET do_with_oh(
    qpdf_data qpdf, qpdf_oh oh,
    std::function<RET()> fallback,
    std::function<RET(QPDFObjectHandle&)> fn)
{
    return trap_oh_errors<RET>(
        qpdf, fallback, [fn, oh](qpdf_data q) {
            auto i = q->oh_cache.find(oh);
            bool result = ((i != q->oh_cache.end()) &&
                           (i->second).get());
            if (! result)
            {
                QTC::TC("qpdf", "qpdf-c invalid object handle");
                throw QPDFExc(
                    qpdf_e_internal,
                    q->qpdf->getFilename(),
                    std::string("C API object handle ") +
                    QUtil::uint_to_string(oh),
                    0, "attempted access to unknown object handle");
            }
            return fn(*(q->oh_cache[oh]));
        });
}

static void do_with_oh_void(
    qpdf_data qpdf, qpdf_oh oh,
    std::function<void(QPDFObjectHandle&)> fn)
{
    do_with_oh<bool>(
        qpdf, oh, return_T<bool>(false), [fn](QPDFObjectHandle& o) {
            fn(o);
            return true; // unused
        });
}

void qpdf_replace_object(qpdf_data qpdf, int objid, int generation, qpdf_oh oh)
{
    do_with_oh_void(
        qpdf, oh, [qpdf, objid, generation](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_replace_object");
            qpdf->qpdf->replaceObject(objid, generation, o);
        });
}

QPDF_BOOL qpdf_oh_is_initialized(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_initialized");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isInitialized();
        });
}

QPDF_BOOL qpdf_oh_is_bool(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_bool");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isBool();
        });
}

QPDF_BOOL qpdf_oh_is_null(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_null");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isNull();
        });
}

QPDF_BOOL qpdf_oh_is_integer(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_integer");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isInteger();
        });
}

QPDF_BOOL qpdf_oh_is_real(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_real");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isReal();
        });
}

QPDF_BOOL qpdf_oh_is_name(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_name");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isName();
        });
}

QPDF_BOOL qpdf_oh_is_string(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_string");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isString();
        });
}

QPDF_BOOL qpdf_oh_is_operator(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_operator");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isOperator();
        });
}

QPDF_BOOL qpdf_oh_is_inline_image(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_inline_image");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isInlineImage();
        });
}

QPDF_BOOL qpdf_oh_is_array(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_array");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isArray();
        });
}

QPDF_BOOL qpdf_oh_is_dictionary(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_dictionary");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isDictionary();
        });
}

QPDF_BOOL qpdf_oh_is_stream(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_stream");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isStream();
        });
}

QPDF_BOOL qpdf_oh_is_indirect(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_indirect");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isIndirect();
        });
}

QPDF_BOOL qpdf_oh_is_scalar(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_scalar");
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            return o.isScalar();
        });
}

QPDF_BOOL qpdf_oh_is_number(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_number");
            return o.isNumber();
        });
}

QPDF_BOOL qpdf_oh_is_name_and_equals(
    qpdf_data qpdf, qpdf_oh oh, char const* name)
{
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [name](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_name_and_equals");
            return o.isNameAndEquals(name);
        });
}

QPDF_BOOL qpdf_oh_is_dictionary_of_type(
    qpdf_data qpdf, qpdf_oh oh, char const* type, char const* subtype)
{
    auto stype = (subtype == nullptr) ? "" : subtype;
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [type, stype](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_dictionary_of_type");
            return o.isDictionaryOfType(type, stype);
        });
}

qpdf_object_type_e qpdf_oh_get_type_code(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<qpdf_object_type_e>(
        qpdf, oh, return_T<qpdf_object_type_e>(ot_uninitialized),
        [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_type_code");
            return o.getTypeCode();
        });
}

char const* qpdf_oh_get_type_name(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<char const*>(
        qpdf, oh, return_T<char const*>(""), [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_type_name");
            qpdf->tmp_string = o.getTypeName();
            return qpdf->tmp_string.c_str();
        });
}

qpdf_oh qpdf_oh_wrap_in_array(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<qpdf_oh>(
        qpdf, oh,
        [qpdf](){ return qpdf_oh_new_array(qpdf); },
        [qpdf](QPDFObjectHandle& qoh) {
            if (qoh.isArray())
            {
                QTC::TC("qpdf", "qpdf-c array to wrap_in_array");
                return new_object(qpdf, qoh);
            }
            else
            {
                QTC::TC("qpdf", "qpdf-c non-array to wrap_in_array");
                return new_object(qpdf,
                                  QPDFObjectHandle::newArray(
                                      std::vector<QPDFObjectHandle>{qoh}));
            }
            });
}

qpdf_oh qpdf_oh_parse(qpdf_data qpdf, char const* object_str)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_parse");
    return trap_oh_errors<qpdf_oh>(
        qpdf, return_uninitialized(qpdf), [object_str] (qpdf_data q) {
            return new_object(q, QPDFObjectHandle::parse(object_str));
        });
}

QPDF_BOOL qpdf_oh_get_bool_value(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_bool_value");
            return o.getBoolValue();
        });
}

long long qpdf_oh_get_int_value(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<long long>(
        qpdf, oh, return_T<long long>(0LL), [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_int_value");
            return o.getIntValue();
        });
}

int qpdf_oh_get_int_value_as_int(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<int>(
        qpdf, oh, return_T<int>(0), [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_int_value_as_int");
            return o.getIntValueAsInt();
        });
}

unsigned long long qpdf_oh_get_uint_value(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<unsigned long long>(
        qpdf, oh, return_T<unsigned long long>(0ULL), [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_uint_value");
            return o.getUIntValue();
        });
}

unsigned int qpdf_oh_get_uint_value_as_uint(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<unsigned int>(
        qpdf, oh, return_T<unsigned int>(0U), [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_uint_value_as_uint");
            return o.getUIntValueAsUInt();
        });
}

char const* qpdf_oh_get_real_value(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<char const*>(
        qpdf, oh, return_T<char const*>(""), [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_real_value");
            qpdf->tmp_string = o.getRealValue();
            return qpdf->tmp_string.c_str();
        });
}

double qpdf_oh_get_numeric_value(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<double>(
        qpdf, oh, return_T<double>(0.0), [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_numeric_value");
            return o.getNumericValue();
        });
}

char const* qpdf_oh_get_name(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<char const*>(
        qpdf, oh, return_T<char const*>(""), [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_name");
            qpdf->tmp_string = o.getName();
            return qpdf->tmp_string.c_str();
        });
}

char const* qpdf_oh_get_string_value(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<char const*>(
        qpdf, oh, return_T<char const*>(""), [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_string_value");
            qpdf->tmp_string = o.getStringValue();
            return qpdf->tmp_string.c_str();
        });
}

char const* qpdf_oh_get_utf8_value(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<char const*>(
        qpdf, oh, return_T<char const*>(""), [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_utf8_value");
            qpdf->tmp_string = o.getUTF8Value();
            return qpdf->tmp_string.c_str();
        });
}

char const* qpdf_oh_get_binary_string_value(
    qpdf_data qpdf, qpdf_oh oh, size_t* length)
{
    return do_with_oh<char const*>(
        qpdf, oh,
        return_T<char const*>(""),
        [qpdf, length](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_binary_string_value");
            qpdf->tmp_string = o.getStringValue();
            *length = qpdf->tmp_string.length();
            return qpdf->tmp_string.c_str();
        });
}

char const* qpdf_oh_get_binary_utf8_value(
    qpdf_data qpdf, qpdf_oh oh, size_t* length)
{
    return do_with_oh<char const*>(
        qpdf, oh,
        return_T<char const*>(""),
        [qpdf, length](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_binary_utf8_value");
            qpdf->tmp_string = o.getUTF8Value();
            *length = qpdf->tmp_string.length();
            return qpdf->tmp_string.c_str();
        });
}

int qpdf_oh_get_array_n_items(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<int>(
        qpdf, oh, return_T<int>(0), [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_array_n_items");
            return o.getArrayNItems();
        });
}

qpdf_oh qpdf_oh_get_array_item(qpdf_data qpdf, qpdf_oh oh, int n)
{
    return do_with_oh<qpdf_oh>(
        qpdf, oh, return_null(qpdf), [qpdf, n](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_array_item");
            return new_object(qpdf, o.getArrayItem(n));
        });
}

void qpdf_oh_begin_dict_key_iter(qpdf_data qpdf, qpdf_oh oh)
{
    qpdf->cur_iter_dict_keys = do_with_oh<std::set<std::string>>(
        qpdf, oh,
        [](){ return std::set<std::string>(); },
        [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_begin_dict_key_iter");
            return o.getKeys();
        });
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
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [key](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_has_key");
            return o.hasKey(key);
        });
}

qpdf_oh qpdf_oh_get_key(qpdf_data qpdf, qpdf_oh oh, char const* key)
{
    return do_with_oh<qpdf_oh>(
        qpdf, oh, return_null(qpdf), [qpdf, key](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_key");
            return new_object(qpdf, o.getKey(key));
        });
}

QPDF_BOOL qpdf_oh_is_or_has_name(qpdf_data qpdf, qpdf_oh oh, char const* key)
{
    return do_with_oh<QPDF_BOOL>(
        qpdf, oh, return_false, [key](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_is_or_has_name");
            return o.isOrHasName(key);
        });
}

qpdf_oh qpdf_oh_new_uninitialized(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_uninitialized");
    return new_object(qpdf, QPDFObjectHandle());
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

qpdf_oh qpdf_oh_new_binary_string(
    qpdf_data qpdf, char const* str, size_t length)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_binary_string");
    return new_object(
        qpdf, QPDFObjectHandle::newString(std::string(str, length)));
}

qpdf_oh qpdf_oh_new_binary_unicode_string(
    qpdf_data qpdf, char const* utf8_str, size_t length)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_binary_unicode_string");
    return new_object(
        qpdf, QPDFObjectHandle::newUnicodeString(std::string(utf8_str, length)));
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

qpdf_oh qpdf_oh_new_stream(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_oh_new_stream");
    return new_object(
        qpdf, QPDFObjectHandle::newStream(qpdf->qpdf.get()));
}

void qpdf_oh_make_direct(qpdf_data qpdf, qpdf_oh oh)
{
    do_with_oh_void(
        qpdf, oh, [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_make_direct");
            o.makeDirect();
        });
}

qpdf_oh qpdf_make_indirect_object(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<qpdf_oh>(
        qpdf, oh,
        return_uninitialized(qpdf),
        [qpdf](QPDFObjectHandle& o) {
            return new_object(qpdf, qpdf->qpdf->makeIndirectObject(o));
        });
}

static QPDFObjectHandle
qpdf_oh_item_internal(qpdf_data qpdf, qpdf_oh item)
{
    return do_with_oh<QPDFObjectHandle>(
        qpdf, item,
        [](){return QPDFObjectHandle::newNull();},
        [](QPDFObjectHandle& o) {
            return o;
        });
}

void qpdf_oh_set_array_item(qpdf_data qpdf, qpdf_oh oh,
                            int at, qpdf_oh item)
{
    do_with_oh_void(
        qpdf, oh, [qpdf, at, item](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_set_array_item");
            o.setArrayItem(at, qpdf_oh_item_internal(qpdf, item));
        });
}

void qpdf_oh_insert_item(qpdf_data qpdf, qpdf_oh oh, int at, qpdf_oh item)
{
    do_with_oh_void(
        qpdf, oh, [qpdf, at, item](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_insert_item");
            o.insertItem(at, qpdf_oh_item_internal(qpdf, item));
        });
}

void qpdf_oh_append_item(qpdf_data qpdf, qpdf_oh oh, qpdf_oh item)
{
    do_with_oh_void(
        qpdf, oh, [qpdf, item](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_append_item");
            o.appendItem(qpdf_oh_item_internal(qpdf, item));
        });
}

void qpdf_oh_erase_item(qpdf_data qpdf, qpdf_oh oh, int at)
{
    do_with_oh_void(
        qpdf, oh, [at](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_erase_item");
            o.eraseItem(at);
        });
}

void qpdf_oh_replace_key(qpdf_data qpdf, qpdf_oh oh,
                         char const* key, qpdf_oh item)
{
    do_with_oh_void(
        qpdf, oh, [qpdf, key, item](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_replace_key");
            o.replaceKey(key, qpdf_oh_item_internal(qpdf, item));
        });
}

void qpdf_oh_remove_key(qpdf_data qpdf, qpdf_oh oh, char const* key)
{
    do_with_oh_void(
        qpdf, oh, [key](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_remove_key");
            o.removeKey(key);
        });
}

void qpdf_oh_replace_or_remove_key(qpdf_data qpdf, qpdf_oh oh,
                                   char const* key, qpdf_oh item)
{
    do_with_oh_void(
        qpdf, oh, [qpdf, key, item](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_replace_or_remove_key");
            o.replaceOrRemoveKey(key, qpdf_oh_item_internal(qpdf, item));
        });
}

qpdf_oh qpdf_oh_get_dict(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<qpdf_oh>(
        qpdf, oh, return_null(qpdf), [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_dict");
            return new_object(qpdf, o.getDict());
        });
}

int qpdf_oh_get_object_id(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<int>(
        qpdf, oh, return_T<int>(0), [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_object_id");
            return o.getObjectID();
        });
}

int qpdf_oh_get_generation(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<int>(
        qpdf, oh, return_T<int>(0), [](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_generation");
            return o.getGeneration();
        });
}

char const* qpdf_oh_unparse(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<char const*>(
        qpdf, oh, return_T<char const*>(""), [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_unparse");
            qpdf->tmp_string = o.unparse();
            return qpdf->tmp_string.c_str();
        });
}

char const* qpdf_oh_unparse_resolved(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<char const*>(
        qpdf, oh, return_T<char const*>(""), [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_unparse_resolved");
            qpdf->tmp_string = o.unparseResolved();
            return qpdf->tmp_string.c_str();
        });
}

char const* qpdf_oh_unparse_binary(qpdf_data qpdf, qpdf_oh oh)
{
    return do_with_oh<char const*>(
        qpdf, oh, return_T<char const*>(""), [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_unparse_binary");
            qpdf->tmp_string = o.unparseBinary();
            return qpdf->tmp_string.c_str();
        });
}

qpdf_oh qpdf_oh_copy_foreign_object(
    qpdf_data qpdf, qpdf_data other_qpdf, qpdf_oh foreign_oh)
{
    return do_with_oh<qpdf_oh>(
        other_qpdf, foreign_oh,
        return_uninitialized(qpdf),
        [qpdf](QPDFObjectHandle& o) {
            QTC::TC("qpdf", "qpdf-c called qpdf_oh_copy_foreign_object");
            return new_object(qpdf, qpdf->qpdf->copyForeignObject(o));
        });
}

QPDF_ERROR_CODE qpdf_oh_get_stream_data(
    qpdf_data qpdf, qpdf_oh stream_oh,
    qpdf_stream_decode_level_e decode_level, QPDF_BOOL* filtered,
    unsigned char** bufp, size_t* len)
{
    return trap_errors(qpdf, [stream_oh, decode_level,
                              filtered, bufp, len] (qpdf_data q) {
        auto stream = qpdf_oh_item_internal(q, stream_oh);
        Pipeline* p = nullptr;
        Pl_Buffer buf("stream data");
        if (bufp)
        {
            p = &buf;
        }
        bool was_filtered = false;
        if (stream.pipeStreamData(
                p, &was_filtered, 0, decode_level, false, false))
        {
            QTC::TC("qpdf", "qpdf-c stream data buf set",
                    bufp ? 0 : 1);
            if (p && bufp && len)
            {
                buf.getMallocBuffer(bufp, len);
            }
            QTC::TC("qpdf", "qpdf-c stream data filtered set",
                    filtered ? 0 : 1);
            if (filtered)
            {
                *filtered = was_filtered ? QPDF_TRUE : QPDF_FALSE;
            }
        }
        else
        {
            throw std::runtime_error(
                "unable to access stream data for stream " + stream.unparse());
        }
    });
}

QPDF_ERROR_CODE qpdf_oh_get_page_content_data(
    qpdf_data qpdf, qpdf_oh page_oh,
    unsigned char** bufp, size_t* len)
{
    return trap_errors(qpdf, [page_oh, bufp, len] (qpdf_data q) {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_get_page_content_data");
        auto o = qpdf_oh_item_internal(q, page_oh);
        Pl_Buffer buf("page contents");
        o.pipePageContents(&buf);
        buf.getMallocBuffer(bufp, len);
    });
}

void qpdf_oh_replace_stream_data(
    qpdf_data qpdf, qpdf_oh stream_oh,
    unsigned char const* buf, size_t len,
    qpdf_oh filter_oh, qpdf_oh decode_parms_oh)
{
    do_with_oh_void(qpdf, stream_oh, [
                        qpdf, buf, len, filter_oh,
                        decode_parms_oh](QPDFObjectHandle& o) {
        QTC::TC("qpdf", "qpdf-c called qpdf_oh_replace_stream_data");
        auto filter = qpdf_oh_item_internal(qpdf, filter_oh);
        auto decode_parms = qpdf_oh_item_internal(qpdf, decode_parms_oh);
        // XXX test with binary data with null
        o.replaceStreamData(
            std::string(reinterpret_cast<char const*>(buf), len),
            filter, decode_parms);
    });
}

int qpdf_get_num_pages(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_num_pages");
    int n = -1;
    QPDF_ERROR_CODE code = trap_errors(qpdf, [&n](qpdf_data q) {
        n = QIntC::to_int(q->qpdf->getAllPages().size());
    });
    if (code & QPDF_ERRORS)
    {
        return -1;
    }
    return n;
}

qpdf_oh qpdf_get_page_n(qpdf_data qpdf, size_t i)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_page_n");
    qpdf_oh result = 0;
    QPDF_ERROR_CODE code = trap_errors(qpdf, [&result, i](qpdf_data q) {
        result = new_object(q, q->qpdf->getAllPages().at(i));
    });
    if ((code & QPDF_ERRORS) || (result == 0))
    {
        return qpdf_oh_new_uninitialized(qpdf);
    }
    return result;
}

QPDF_ERROR_CODE qpdf_update_all_pages_cache(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_update_all_pages_cache");
    return trap_errors(qpdf, [](qpdf_data q) {
        q->qpdf->updateAllPagesCache();
    });
}

int qpdf_find_page_by_id(qpdf_data qpdf, int objid, int generation)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_find_page_by_id");
    int n = -1;
    QPDFObjGen og(objid, generation);
    QPDF_ERROR_CODE code = trap_errors(qpdf, [&n, &og](qpdf_data q) {
        n = QIntC::to_int(q->qpdf->findPage(og));
    });
    if (code & QPDF_ERRORS)
    {
        return -1;
    }
    return n;
}

int qpdf_find_page_by_oh(qpdf_data qpdf, qpdf_oh oh)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_find_page_by_oh");
    return do_with_oh<int>(
        qpdf, oh, return_T<int>(-1), [qpdf](QPDFObjectHandle& o) {
            return qpdf->qpdf->findPage(o);
        });
}

QPDF_ERROR_CODE qpdf_push_inherited_attributes_to_page(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_push_inherited_attributes_to_page");
    return trap_errors(qpdf, [](qpdf_data q) {
        q->qpdf->pushInheritedAttributesToPage();
    });
}

QPDF_ERROR_CODE qpdf_add_page(
    qpdf_data qpdf, qpdf_data newpage_qpdf, qpdf_oh newpage, QPDF_BOOL first)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_add_page");
    auto page = qpdf_oh_item_internal(newpage_qpdf, newpage);
    return trap_errors(qpdf, [&page, first](qpdf_data q) {
        q->qpdf->addPage(page, first);
    });
}

QPDF_ERROR_CODE qpdf_add_page_at(
    qpdf_data qpdf, qpdf_data newpage_qpdf, qpdf_oh newpage,
    QPDF_BOOL before, qpdf_oh refpage)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_add_page_at");
    auto page = qpdf_oh_item_internal(newpage_qpdf, newpage);
    auto ref = qpdf_oh_item_internal(qpdf, refpage);
    return trap_errors(qpdf, [&page, before, &ref](qpdf_data q) {
        q->qpdf->addPageAt(page, before, ref);
    });
}

QPDF_ERROR_CODE qpdf_remove_page(qpdf_data qpdf, qpdf_oh page)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_remove_page");
    auto p = qpdf_oh_item_internal(qpdf, page);
    return trap_errors(qpdf, [&p](qpdf_data q) {
        q->qpdf->removePage(p);
    });
}
