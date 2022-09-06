#include <qpdf/BufferInputSource.hh>

#include <qpdf/QIntC.hh>
#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string.h>

BufferInputSource::BufferInputSource(
    std::string const& description, Buffer* buf, bool own_memory) :
    own_memory(own_memory),
    description(description),
    buf(buf)
{
    this->buf_ptr = reinterpret_cast<char*>(buf->getBuffer());
    this->max_offset = buf ? QIntC::to_offset(buf->getSize()) : 0;
    this->fast_enabled = true;
}

BufferInputSource::BufferInputSource(
    std::string const& description, std::string const& contents) :
    own_memory(true),
    description(description),
    buf(new Buffer(contents.length()))
{
    this->buf_ptr = reinterpret_cast<char*>(this->buf->getBuffer());
    this->max_offset = QIntC::to_offset(buf->getSize());
    this->fast_enabled = true;
    memcpy(buf->getBuffer(), contents.c_str(), contents.length());
}

BufferInputSource::~BufferInputSource()
{
    if (this->own_memory) {
        delete this->buf;
    }
}

qpdf_offset_t
BufferInputSource::findAndSkipNextEOL()
{
    if (this->cur_offset < 0) {
        throw std::logic_error("INTERNAL ERROR: BufferInputSource offset < 0");
    }
    qpdf_offset_t end_pos = this->max_offset;
    if (this->cur_offset >= end_pos) {
        this->last_offset = end_pos;
        this->cur_offset = end_pos;
        return end_pos;
    }

    qpdf_offset_t result = 0;
    unsigned char const* buffer = this->buf->getBuffer();
    unsigned char const* end = buffer + end_pos;
    unsigned char const* p = buffer + this->cur_offset;

    while ((p < end) && !((*p == '\r') || (*p == '\n'))) {
        ++p;
    }
    if (p < end) {
        result = p - buffer;
        this->cur_offset = result + 1;
        ++p;
        while ((this->cur_offset < end_pos) && ((*p == '\r') || (*p == '\n'))) {
            ++p;
            ++this->cur_offset;
        }
    } else {
        this->cur_offset = end_pos;
        result = end_pos;
    }
    return result;
}

std::string const&
BufferInputSource::getName() const
{
    return this->description;
}

qpdf_offset_t
BufferInputSource::tell()
{
    return this->cur_offset;
}

void
BufferInputSource::seek(qpdf_offset_t offset, int whence)
{
    switch (whence) {
    case SEEK_SET:
        this->cur_offset = offset;
        break;

    case SEEK_END:
        QIntC::range_check(this->max_offset, offset);
        this->cur_offset = this->max_offset + offset;
        break;

    case SEEK_CUR:
        QIntC::range_check(this->cur_offset, offset);
        this->cur_offset += offset;
        break;

    default:
        throw std::logic_error(
            "INTERNAL ERROR: invalid argument to BufferInputSource::seek");
        break;
    }

    if (this->cur_offset < 0) {
        throw std::runtime_error(
            this->description + ": seek before beginning of buffer");
    }
}

void
BufferInputSource::rewind()
{
    this->cur_offset = 0;
}

size_t
BufferInputSource::read(char* buffer, size_t length)
{
    if (this->cur_offset < 0) {
        throw std::logic_error("INTERNAL ERROR: BufferInputSource offset < 0");
    }
    qpdf_offset_t end_pos = this->max_offset;
    if (this->cur_offset >= end_pos) {
        this->last_offset = end_pos;
        return 0;
    }

    this->last_offset = this->cur_offset;
    size_t len = std::min(QIntC::to_size(end_pos - this->cur_offset), length);
    memcpy(buffer, this->buf->getBuffer() + this->cur_offset, len);
    this->cur_offset += QIntC::to_offset(len);
    return len;
}

void
BufferInputSource::unreadCh(char ch)
{
    if (this->cur_offset > 0) {
        --this->cur_offset;
    }
}
