#include <qpdf/BufferInputSource.hh>
#include <string.h>
#include <stdexcept>
#include <algorithm>

BufferInputSource::BufferInputSource(std::string const& description,
                                     Buffer* buf, bool own_memory) :
    own_memory(own_memory),
    description(description),
    buf(buf),
    cur_offset(0)
{
}

BufferInputSource::BufferInputSource(std::string const& description,
                                     std::string const& contents) :
    own_memory(true),
    description(description),
    buf(0),
    cur_offset(0)
{
    this->buf = new Buffer(contents.length());
    unsigned char* bp = buf->getBuffer();
    memcpy(bp, contents.c_str(), contents.length());
}

BufferInputSource::~BufferInputSource()
{
    if (own_memory)
    {
	delete this->buf;
    }
}

qpdf_offset_t
BufferInputSource::findAndSkipNextEOL()
{
    if (this->cur_offset < 0)
    {
        throw std::logic_error("INTERNAL ERROR: BufferInputSource offset < 0");
    }
    qpdf_offset_t end_pos = this->buf->getSize();
    if (this->cur_offset >= end_pos)
    {
	this->last_offset = end_pos;
        this->cur_offset = end_pos;
	return end_pos;
    }

    qpdf_offset_t result = 0;
    size_t len = end_pos - this->cur_offset;
    unsigned char const* buffer = this->buf->getBuffer();

    void* start = const_cast<unsigned char*>(buffer) + this->cur_offset;
    unsigned char* p1 = static_cast<unsigned char*>(memchr(start, '\r', len));
    unsigned char* p2 = static_cast<unsigned char*>(memchr(start, '\n', len));
    unsigned char* p = (p1 && p2) ? std::min(p1, p2) : p1 ? p1 : p2;
    if (p)
    {
        result = p - buffer;
        this->cur_offset = result + 1;
        ++p;
        while ((this->cur_offset < end_pos) &&
               ((*p == '\r') || (*p == '\n')))
        {
            ++p;
            ++this->cur_offset;
        }
    }
    else
    {
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
    switch (whence)
    {
      case SEEK_SET:
	this->cur_offset = offset;
	break;

      case SEEK_END:
	this->cur_offset = this->buf->getSize() + offset;
	break;

      case SEEK_CUR:
	this->cur_offset += offset;
	break;

      default:
	throw std::logic_error(
	    "INTERNAL ERROR: invalid argument to BufferInputSource::seek");
	break;
    }

    if (this->cur_offset < 0)
    {
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
    if (this->cur_offset < 0)
    {
        throw std::logic_error("INTERNAL ERROR: BufferInputSource offset < 0");
    }
    qpdf_offset_t end_pos = this->buf->getSize();
    if (this->cur_offset >= end_pos)
    {
	this->last_offset = end_pos;
	return 0;
    }

    this->last_offset = this->cur_offset;
    size_t len = std::min(
        static_cast<size_t>(end_pos - this->cur_offset), length);
    memcpy(buffer, buf->getBuffer() + this->cur_offset, len);
    this->cur_offset += len;
    return len;
}

void
BufferInputSource::unreadCh(char ch)
{
    if (this->cur_offset > 0)
    {
	--this->cur_offset;
    }
}
