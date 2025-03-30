#include <qpdf/QPDFExc.hh>

QPDFExc::QPDFExc(
    qpdf_error_code_e error_code,
    std::string const& filename,
    std::string const& object,
    qpdf_offset_t offset,
    std::string const& message) :
    std::runtime_error(createWhat(filename, object, (offset ? offset : -1), message)),
    error_code(error_code),
    filename(filename),
    object(object),
    offset(offset ? offset : -1),
    message(message)
{
}

QPDFExc::QPDFExc(
    qpdf_error_code_e error_code,
    std::string const& filename,
    std::string const& object,
    qpdf_offset_t offset,
    std::string const& message,
    bool zero_offset_valid) :
    std::runtime_error(
        createWhat(filename, object, (offset || zero_offset_valid ? offset : -1), message)),
    error_code(error_code),
    filename(filename),
    object(object),
    offset(offset || zero_offset_valid ? offset : -1),
    message(message)
{
}

std::string
QPDFExc::createWhat(
    std::string const& filename,
    std::string const& object,
    qpdf_offset_t offset,
    std::string const& message)
{
    std::string result;
    if (!filename.empty()) {
        result += filename;
    }
    if (!(object.empty() && offset < 0)) {
        if (!filename.empty()) {
            result += " (";
        }
        if (!object.empty()) {
            result += object;
            if (offset >= 0) {
                result += ", ";
            }
        }
        if (offset >= 0) {
            result += "offset " + std::to_string(offset);
        }
        if (!filename.empty()) {
            result += ")";
        }
    }
    if (!result.empty()) {
        result += ": ";
    }
    result += message;
    return result;
}

qpdf_error_code_e
QPDFExc::getErrorCode() const
{
    return this->error_code;
}

std::string const&
QPDFExc::getFilename() const
{
    return this->filename;
}

std::string const&
QPDFExc::getObject() const
{
    return this->object;
}

qpdf_offset_t
QPDFExc::getFilePosition() const
{
    return offset < 0 ? 0 : offset;
}

std::string const&
QPDFExc::getMessageDetail() const
{
    return this->message;
}
