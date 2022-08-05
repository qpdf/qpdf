#include <qpdf/QPDFExc.hh>

#include <qpdf/QUtil.hh>

QPDFExc::QPDFExc(
    qpdf_error_code_e error_code,
    std::string const& filename,
    std::string const& object,
    qpdf_offset_t offset,
    std::string const& message) :
    std::runtime_error(createWhat(filename, object, offset, message)),
    error_code(error_code),
    filename(filename),
    object(object),
    offset(offset),
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
    if (!object.empty() || offset != 0) {
        if (!filename.empty()) {
            result += " (";
        }
        if (!object.empty()) {
            result += object;
            if (offset > 0) {
                result += ", ";
            }
        }
        if (offset > 0) {
            result += "offset " + QUtil::int_to_string(offset);
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
    return this->offset;
}

std::string const&
QPDFExc::getMessageDetail() const
{
    return this->message;
}
