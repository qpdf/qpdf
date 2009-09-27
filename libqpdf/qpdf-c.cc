#include <qpdf/qpdf-c.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QTC.hh>

#include <list>
#include <string>
#include <stdexcept>

struct _qpdf_data
{
    _qpdf_data();
    ~_qpdf_data();

    QPDF* qpdf;
    QPDFWriter* qpdf_writer;

    std::string error;
    std::list<std::string> warnings;
    std::string tmp_string;
};

_qpdf_data::_qpdf_data() :
    qpdf(0),
    qpdf_writer(0)
{
}

_qpdf_data::~_qpdf_data()
{
    delete qpdf_writer;
    delete qpdf;
}

DLL_EXPORT
qpdf_data qpdf_init()
{
    QTC::TC("qpdf", "qpdf-c called qpdf_init");
    qpdf_data qpdf = new _qpdf_data();
    qpdf->qpdf = new QPDF();
    return qpdf;
}

DLL_EXPORT
void qpdf_cleanup(qpdf_data* qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_cleanup");
    delete *qpdf;
    *qpdf = 0;
}

DLL_EXPORT
QPDF_BOOL qpdf_more_errors(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_more_errors");
    return (qpdf->error.empty() ? QPDF_FALSE : QPDF_TRUE);
}

DLL_EXPORT
QPDF_BOOL qpdf_more_warnings(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_more_warnings");

    if (qpdf->warnings.empty())
    {
	std::vector<std::string> w = qpdf->qpdf->getWarnings();
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

DLL_EXPORT
char const* qpdf_next_error(qpdf_data qpdf)
{
    if (qpdf_more_errors(qpdf))
    {
	qpdf->tmp_string = qpdf->error;
	qpdf->error.clear();
	QTC::TC("qpdf", "qpdf-c qpdf_next_error returned error");
	return qpdf->tmp_string.c_str();
    }
    else
    {
	return 0;
    }
}

DLL_EXPORT
char const* qpdf_next_warning(qpdf_data qpdf)
{
    if (qpdf_more_warnings(qpdf))
    {
	qpdf->tmp_string = qpdf->warnings.front();
	qpdf->warnings.pop_front();
	QTC::TC("qpdf", "qpdf-c qpdf_next_warning returned warning");
	return qpdf->tmp_string.c_str();
    }
    else
    {
	return 0;
    }
}

DLL_EXPORT
void qpdf_set_suppress_warnings(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_suppress_warnings");
    qpdf->qpdf->setSuppressWarnings(value);
}

DLL_EXPORT
void qpdf_set_ignore_xref_streams(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_ignore_xref_streams");
    qpdf->qpdf->setIgnoreXRefStreams(value);
}

DLL_EXPORT
void qpdf_set_attempt_recovery(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_attempt_recovery");
    qpdf->qpdf->setAttemptRecovery(value);
}

DLL_EXPORT
QPDF_ERROR_CODE qpdf_read(qpdf_data qpdf, char const* filename,
			  char const* password)
{
    QPDF_ERROR_CODE status = QPDF_SUCCESS;
    try
    {
	qpdf->qpdf->processFile(filename, password);
    }
    catch (std::exception& e)
    {
	qpdf->error = e.what();
	status |= QPDF_ERRORS;
    }
    if (qpdf_more_warnings(qpdf))
    {
	status |= QPDF_WARNINGS;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_read", status);
    return status;
}

DLL_EXPORT
char const* qpdf_get_pdf_version(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_pdf_version");
    qpdf->tmp_string = qpdf->qpdf->getPDFVersion();
    return qpdf->tmp_string.c_str();
}

DLL_EXPORT
char const* qpdf_get_user_password(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_get_user_password");
    qpdf->tmp_string = qpdf->qpdf->getTrimmedUserPassword();
    return qpdf->tmp_string.c_str();
}

DLL_EXPORT
QPDF_BOOL qpdf_is_linearized(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_is_linearized");
    return (qpdf->qpdf->isLinearized() ? QPDF_TRUE : QPDF_FALSE);
}

DLL_EXPORT
QPDF_BOOL qpdf_is_encrypted(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_is_encrypted");
    return (qpdf->qpdf->isEncrypted() ? QPDF_TRUE : QPDF_FALSE);
}

DLL_EXPORT
QPDF_BOOL qpdf_allow_accessibility(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_accessibility");
    return qpdf->qpdf->allowAccessibility();
}

DLL_EXPORT
QPDF_BOOL qpdf_allow_extract_all(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_extract_all");
    return qpdf->qpdf->allowExtractAll();
}

DLL_EXPORT
QPDF_BOOL qpdf_allow_print_low_res(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_print_low_res");
    return qpdf->qpdf->allowPrintLowRes();
}

DLL_EXPORT
QPDF_BOOL qpdf_allow_print_high_res(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_print_high_res");
    return qpdf->qpdf->allowPrintHighRes();
}

DLL_EXPORT
QPDF_BOOL qpdf_allow_modify_assembly(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_assembly");
    return qpdf->qpdf->allowModifyAssembly();
}

DLL_EXPORT
QPDF_BOOL qpdf_allow_modify_form(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_form");
    return qpdf->qpdf->allowModifyForm();
}

DLL_EXPORT
QPDF_BOOL qpdf_allow_modify_annotation(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_annotation");
    return qpdf->qpdf->allowModifyAnnotation();
}

DLL_EXPORT
QPDF_BOOL qpdf_allow_modify_other(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_other");
    return qpdf->qpdf->allowModifyOther();
}

DLL_EXPORT
QPDF_BOOL qpdf_allow_modify_all(qpdf_data qpdf)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_allow_modify_all");
    return qpdf->qpdf->allowModifyAll();
}

DLL_EXPORT
QPDF_ERROR_CODE qpdf_init_write(qpdf_data qpdf, char const* filename)
{
    QPDF_ERROR_CODE status = QPDF_SUCCESS;
    try
    {
	qpdf->qpdf_writer = new QPDFWriter(*(qpdf->qpdf), filename);
    }
    catch (std::exception& e)
    {
	qpdf->error = e.what();
	status |= QPDF_ERRORS;
    }
    if (qpdf_more_warnings(qpdf))
    {
	status |= QPDF_WARNINGS;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_init_write", status);
    return status;
}

DLL_EXPORT
void qpdf_set_object_stream_mode(qpdf_data qpdf, int mode)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_object_stream_mode");
    QPDFWriter::object_stream_e omode = QPDFWriter::o_preserve;
    switch (mode)
    {
      case QPDF_OBJECT_STREAM_DISABLE:
	omode = QPDFWriter::o_disable;
	break;

      case QPDF_OBJECT_STREAM_GENERATE:
	omode = QPDFWriter::o_generate;
	break;

      default:
	// already set to o_preserve; treate out of range values as
	// the default.
	break;
    }

    qpdf->qpdf_writer->setObjectStreamMode(omode);
}

DLL_EXPORT
void qpdf_set_stream_data_mode(qpdf_data qpdf, int mode)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_stream_data_mode");
    QPDFWriter::stream_data_e smode = QPDFWriter::s_preserve;
    switch (mode)
    {
      case QPDF_STREAM_DATA_UNCOMPRESS:
	smode = QPDFWriter::s_uncompress;
	break;

      case QPDF_STREAM_DATA_COMPRESS:
	smode = QPDFWriter::s_compress;
	break;

      default:
	// Treat anything else as default
	break;
    }
    qpdf->qpdf_writer->setStreamDataMode(smode);
}

DLL_EXPORT
void qpdf_set_content_normalization(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_content_normalization");
    qpdf->qpdf_writer->setContentNormalization(value);
}

DLL_EXPORT
void qpdf_set_qdf_mode(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_qdf_mode");
    qpdf->qpdf_writer->setQDFMode(value);
}

DLL_EXPORT
void qpdf_set_static_ID(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_static_ID");
    qpdf->qpdf_writer->setStaticID(value);
}

DLL_EXPORT
void qpdf_set_suppress_original_object_IDs(
    qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_suppress_original_object_IDs");
    qpdf->qpdf_writer->setSuppressOriginalObjectIDs(value);
}

DLL_EXPORT
void qpdf_set_preserve_encryption(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_preserve_encryption");
    qpdf->qpdf_writer->setPreserveEncryption(value);
}

DLL_EXPORT
void qpdf_set_r2_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_print, QPDF_BOOL allow_modify,
    QPDF_BOOL allow_extract, QPDF_BOOL allow_annotate)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_r2_encryption_parameters");
    qpdf->qpdf_writer->setR2EncryptionParameters(
	user_password, owner_password,
	allow_print, allow_modify, allow_extract, allow_annotate);
}

DLL_EXPORT
void qpdf_set_r3_encryption_parameters(
    qpdf_data qpdf, char const* user_password, char const* owner_password,
    QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
    int print, int modify)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_r3_encryption_parameters");
    qpdf->qpdf_writer->setR3EncryptionParameters(
	user_password, owner_password,
	allow_accessibility, allow_extract,
	((print == QPDF_R3_PRINT_LOW) ? QPDFWriter::r3p_low :
	 (print == QPDF_R3_PRINT_NONE) ? QPDFWriter::r3p_none :
	 QPDFWriter::r3p_full),
	((print == QPDF_R3_MODIFY_ANNOTATE) ? QPDFWriter::r3m_annotate :
	 (print == QPDF_R3_MODIFY_FORM) ? QPDFWriter::r3m_form :
	 (print == QPDF_R3_MODIFY_ASSEMBLY) ? QPDFWriter::r3m_assembly :
	 (print == QPDF_R3_MODIFY_NONE) ? QPDFWriter::r3m_none :
	 QPDFWriter::r3m_all));
}

DLL_EXPORT
void qpdf_set_linearization(qpdf_data qpdf, QPDF_BOOL value)
{
    QTC::TC("qpdf", "qpdf-c called qpdf_set_linearization");
    qpdf->qpdf_writer->setLinearization(value);
}

DLL_EXPORT
QPDF_ERROR_CODE qpdf_write(qpdf_data qpdf)
{
    QPDF_ERROR_CODE status = QPDF_SUCCESS;
    try
    {
	qpdf->qpdf_writer->write();
    }
    catch (std::exception& e)
    {
	qpdf->error = e.what();
	status |= QPDF_ERRORS;
    }
    if (qpdf_more_warnings(qpdf))
    {
	status |= QPDF_WARNINGS;
    }
    QTC::TC("qpdf", "qpdf-c called qpdf_write", status);
    return status;
}
