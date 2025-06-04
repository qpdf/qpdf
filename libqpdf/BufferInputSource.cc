#include <qpdf/BufferInputSource.hh>
#include <qpdf/InputSource_private.hh>

#include <qpdf/Buffer.hh>
#include <qpdf/QIntC.hh>

#include <algorithm>
#include <cstring>
#include <sstream>

using namespace qpdf;

BufferInputSource::BufferInputSource(std::string const& description, Buffer* buf, bool own_memory) :
    own_memory(own_memory),
    description(description),
    buf(buf),
    cur_offset(0),
    max_offset(buf ? QIntC::to_offset(buf->getSize()) : 0)
{
}

BufferInputSource::BufferInputSource(std::string const& description, std::string const& contents) :
    own_memory(true),
    description(description),
    buf(new Buffer(contents.length())),
    cur_offset(0),
    max_offset(QIntC::to_offset(buf->getSize()))
{
    memcpy(buf->getBuffer(), contents.c_str(), contents.length());
}

BufferInputSource::~BufferInputSource()
{
    if (own_memory) {
        delete buf;
    }
}

qpdf_offset_t
BufferInputSource::findAndSkipNextEOL()
{
    if (cur_offset < 0) {
        throw std::logic_error("INTERNAL ERROR: BufferInputSource offset < 0");
    }
    qpdf_offset_t end_pos = max_offset;
    if (cur_offset >= end_pos) {
        last_offset = end_pos;
        cur_offset = end_pos;
        return end_pos;
    }

    qpdf_offset_t result = 0;
    unsigned char const* buffer = buf->getBuffer();
    unsigned char const* end = buffer + end_pos;
    unsigned char const* p = buffer + cur_offset;

    while ((p < end) && !((*p == '\r') || (*p == '\n'))) {
        ++p;
    }
    if (p < end) {
        result = p - buffer;
        cur_offset = result + 1;
        ++p;
        while ((cur_offset < end_pos) && ((*p == '\r') || (*p == '\n'))) {
            ++p;
            ++cur_offset;
        }
    } else {
        cur_offset = end_pos;
        result = end_pos;
    }
    return result;
}

std::string const&
BufferInputSource::getName() const
{
    return description;
}

qpdf_offset_t
BufferInputSource::tell()
{
    return cur_offset;
}

void
BufferInputSource::seek(qpdf_offset_t offset, int whence)
{
    switch (whence) {
    case SEEK_SET:
        cur_offset = offset;
        break;

    case SEEK_END:
        QIntC::range_check(max_offset, offset);
        cur_offset = max_offset + offset;
        break;

    case SEEK_CUR:
        QIntC::range_check(cur_offset, offset);
        cur_offset += offset;
        break;

    default:
        throw std::logic_error("INTERNAL ERROR: invalid argument to BufferInputSource::seek");
        break;
    }

    if (cur_offset < 0) {
        throw std::runtime_error(description + ": seek before beginning of buffer");
    }
}

void
BufferInputSource::rewind()
{
    cur_offset = 0;
}

size_t
BufferInputSource::read(char* buffer, size_t length)
{
    if (cur_offset < 0) {
        throw std::logic_error("INTERNAL ERROR: BufferInputSource offset < 0");
    }
    qpdf_offset_t end_pos = max_offset;
    if (cur_offset >= end_pos) {
        last_offset = end_pos;
        return 0;
    }

    last_offset = cur_offset;
    size_t len = std::min(QIntC::to_size(end_pos - cur_offset), length);
    memcpy(buffer, buf->getBuffer() + cur_offset, len);
    cur_offset += QIntC::to_offset(len);
    return len;
}

void
BufferInputSource::unreadCh(char ch)
{
    if (cur_offset > 0) {
        --cur_offset;
    }
}

qpdf_offset_t
is::OffsetBuffer::findAndSkipNextEOL_internal()
{
    if (cur_offset < 0) {
        throw std::logic_error("INTERNAL ERROR: is::OffsetBuffer offset < 0");
    }
    qpdf_offset_t end_pos = max_offset;
    if (cur_offset >= end_pos) {
        last_offset = end_pos + global_offset;
        cur_offset = end_pos;
        return end_pos;
    }

    qpdf_offset_t result = 0;
    unsigned char const* buffer = buf->getBuffer();
    unsigned char const* end = buffer + end_pos;
    unsigned char const* p = buffer + cur_offset;

    while (p < end && !(*p == '\r' || *p == '\n')) {
        ++p;
    }
    if (p < end) {
        result = p - buffer;
        cur_offset = result + 1;
        ++p;
        while (cur_offset < end_pos && (*p == '\r' || *p == '\n')) {
            ++p;
            ++cur_offset;
        }
    } else {
        cur_offset = end_pos;
        result = end_pos;
    }
    return result;
}

void
is::OffsetBuffer::seek_internal(qpdf_offset_t offset, int whence)
{
    switch (whence) {
    case SEEK_SET:
        cur_offset = offset;
        break;

    case SEEK_END:
        QIntC::range_check(max_offset, offset);
        cur_offset = max_offset + offset;
        break;

    case SEEK_CUR:
        QIntC::range_check(cur_offset, offset);
        cur_offset += offset;
        break;

    default:
        throw std::logic_error("INTERNAL ERROR: invalid argument to BufferInputSource::seek");
        break;
    }

    if (cur_offset < 0) {
        throw std::runtime_error(description + ": seek before beginning of buffer");
    }
}

size_t
is::OffsetBuffer::read(char* buffer, size_t length)
{
    if (cur_offset < 0) {
        throw std::logic_error("INTERNAL ERROR: is::OffsetBuffer offset < 0");
    }
    qpdf_offset_t end_pos = max_offset;
    if (cur_offset >= end_pos) {
        last_offset = end_pos + global_offset;
        return 0;
    }

    last_offset = cur_offset + global_offset;
    size_t len = std::min(QIntC::to_size(end_pos - cur_offset), length);
    memcpy(buffer, buf->getBuffer() + cur_offset, len);
    cur_offset += QIntC::to_offset(len);
    return len;
}
