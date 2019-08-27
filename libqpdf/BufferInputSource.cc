#include <qpdf/BufferInputSource.hh>
#include <qpdf/QIntC.hh>
#include <string.h>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <sstream>

BufferInputSource::Members::Members(bool own_memory,
                                    std::string const& description,
                                    Buffer* buf) :
    own_memory(own_memory),
    description(description),
    buf(buf),
    cur_offset(0),
    max_offset(buf ? QIntC::to_offset(buf->getSize()) : 0)
{
}

BufferInputSource::Members::~Members()
{
}

BufferInputSource::BufferInputSource(std::string const& description,
                                     Buffer* buf, bool own_memory) :
    m(new Members(own_memory, description, buf))
{
}

BufferInputSource::BufferInputSource(std::string const& description,
                                     std::string const& contents) :
    m(new Members(true, description, 0))
{
    this->m->buf = new Buffer(contents.length());
    this->m->max_offset = QIntC::to_offset(this->m->buf->getSize());
    unsigned char* bp = this->m->buf->getBuffer();
    memcpy(bp, contents.c_str(), contents.length());
}

BufferInputSource::~BufferInputSource()
{
    if (this->m->own_memory)
    {
	delete this->m->buf;
    }
}

qpdf_offset_t
BufferInputSource::findAndSkipNextEOL()
{
    if (this->m->cur_offset < 0)
    {
        throw std::logic_error("INTERNAL ERROR: BufferInputSource offset < 0");
    }
    qpdf_offset_t end_pos = this->m->max_offset;
    if (this->m->cur_offset >= end_pos)
    {
	this->last_offset = end_pos;
        this->m->cur_offset = end_pos;
	return end_pos;
    }

    qpdf_offset_t result = 0;
    size_t len = QIntC::to_size(end_pos - this->m->cur_offset);
    unsigned char const* buffer = this->m->buf->getBuffer();

    void* start = const_cast<unsigned char*>(buffer) + this->m->cur_offset;
    unsigned char* p1 = static_cast<unsigned char*>(memchr(start, '\r', len));
    unsigned char* p2 = static_cast<unsigned char*>(memchr(start, '\n', len));
    unsigned char* p = (p1 && p2) ? std::min(p1, p2) : p1 ? p1 : p2;
    if (p)
    {
        result = p - buffer;
        this->m->cur_offset = result + 1;
        ++p;
        while ((this->m->cur_offset < end_pos) &&
               ((*p == '\r') || (*p == '\n')))
        {
            ++p;
            ++this->m->cur_offset;
        }
    }
    else
    {
        this->m->cur_offset = end_pos;
        result = end_pos;
    }
    return result;
}

std::string const&
BufferInputSource::getName() const
{
    return this->m->description;
}

qpdf_offset_t
BufferInputSource::tell()
{
    return this->m->cur_offset;
}

void
BufferInputSource::range_check(qpdf_offset_t cur, qpdf_offset_t delta)
{
    if ((delta > 0) &&
        ((std::numeric_limits<qpdf_offset_t>::max() - cur) < delta))
    {
        std::ostringstream msg;
        msg << "seeking forward from " << cur
            << " by " << delta
            << " would cause an overflow of the offset type";
        throw std::range_error(msg.str());
    }
}

void
BufferInputSource::seek(qpdf_offset_t offset, int whence)
{
    switch (whence)
    {
      case SEEK_SET:
	this->m->cur_offset = offset;
	break;

      case SEEK_END:
        range_check(this->m->max_offset, offset);
	this->m->cur_offset = this->m->max_offset + offset;
	break;

      case SEEK_CUR:
        range_check(this->m->cur_offset, offset);
	this->m->cur_offset += offset;
	break;

      default:
	throw std::logic_error(
	    "INTERNAL ERROR: invalid argument to BufferInputSource::seek");
	break;
    }

    if (this->m->cur_offset < 0)
    {
        throw std::runtime_error(
            this->m->description + ": seek before beginning of buffer");
    }
}

void
BufferInputSource::rewind()
{
    this->m->cur_offset = 0;
}

size_t
BufferInputSource::read(char* buffer, size_t length)
{
    if (this->m->cur_offset < 0)
    {
        throw std::logic_error("INTERNAL ERROR: BufferInputSource offset < 0");
    }
    qpdf_offset_t end_pos = this->m->max_offset;
    if (this->m->cur_offset >= end_pos)
    {
	this->last_offset = end_pos;
	return 0;
    }

    this->last_offset = this->m->cur_offset;
    size_t len = std::min(
        QIntC::to_size(end_pos - this->m->cur_offset), length);
    memcpy(buffer, this->m->buf->getBuffer() + this->m->cur_offset, len);
    this->m->cur_offset += QIntC::to_offset(len);
    return len;
}

void
BufferInputSource::unreadCh(char ch)
{
    if (this->m->cur_offset > 0)
    {
	--this->m->cur_offset;
    }
}
