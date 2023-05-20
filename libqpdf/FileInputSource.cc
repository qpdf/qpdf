#include <qpdf/FileInputSource.hh>

#include <qpdf/QPDFExc.hh>
#include <qpdf/QUtil.hh>
#include <algorithm>
#include <cstring>

FileInputSource::FileInputSource() :
    close_file(false),
    file(nullptr)
{
}

FileInputSource::FileInputSource(char const* filename) :
    close_file(true),
    filename(filename),
    file(QUtil::safe_fopen(filename, "rb"))
{
}

FileInputSource::FileInputSource(
    char const* description, FILE* filep, bool close_file) :
    close_file(close_file),
    filename(description),
    file(filep)
{
}

FileInputSource::~FileInputSource()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
    if (this->file && this->close_file) {
        fclose(this->file);
    }
}

void
FileInputSource::setFilename(char const* filename)
{
    this->close_file = true;
    this->filename = filename;
    this->file = QUtil::safe_fopen(filename, "rb");
}

void
FileInputSource::setFile(char const* description, FILE* filep, bool close_file)
{
    this->filename = description;
    this->file = filep;
    this->seek(0, SEEK_SET);
}

qpdf_offset_t
FileInputSource::findAndSkipNextEOL()
{
    qpdf_offset_t result = 0;
    bool done = false;
    char buf[10240];
    while (!done) {
        qpdf_offset_t cur_offset = QUtil::tell(this->file);
        size_t len = this->read(buf, sizeof(buf));
        if (len == 0) {
            done = true;
            result = this->tell();
        } else {
            char* p1 = static_cast<char*>(memchr(buf, '\r', len));
            char* p2 = static_cast<char*>(memchr(buf, '\n', len));
            char* p = (p1 && p2) ? std::min(p1, p2) : p1 ? p1 : p2;
            if (p) {
                result = cur_offset + (p - buf);
                // We found \r or \n.  Keep reading until we get past
                // \r and \n characters.
                this->seek(result + 1, SEEK_SET);
                char ch;
                while (!done) {
                    if (this->read(&ch, 1) == 0) {
                        done = true;
                    } else if (!((ch == '\r') || (ch == '\n'))) {
                        this->unreadCh(ch);
                        done = true;
                    }
                }
            }
        }
    }
    return result;
}

std::string const&
FileInputSource::getName() const
{
    return this->filename;
}

qpdf_offset_t
FileInputSource::tell()
{
    return QUtil::tell(this->file);
}

void
FileInputSource::seek(qpdf_offset_t offset, int whence)
{
    if (QUtil::seek(this->file, offset, whence) == -1) {
        QUtil::throw_system_error(
            std::string("seek to ") + this->filename + ", offset " +
            std::to_string(offset) + " (" + std::to_string(whence) + ")");
    }
}

void
FileInputSource::rewind()
{
    ::rewind(this->file);
}

size_t
FileInputSource::read(char* buffer, size_t length)
{
    this->last_offset = QUtil::tell(this->file);
    size_t len = fread(buffer, 1, length, this->file);
    if (len == 0) {
        if (ferror(this->file)) {
            throw QPDFExc(
                qpdf_e_system,
                this->filename,
                "",
                this->last_offset,
                (std::string("read ") + std::to_string(length) + " bytes"));
        } else if (length > 0) {
            this->seek(0, SEEK_END);
            this->last_offset = this->tell();
        }
    }
    return len;
}

void
FileInputSource::unreadCh(char ch)
{
    if (ungetc(static_cast<unsigned char>(ch), this->file) == -1) {
        QUtil::throw_system_error(this->filename + ": unread character");
    }
}
