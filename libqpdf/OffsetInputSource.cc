#include <qpdf/OffsetInputSource.hh>

#include <limits>
#include <sstream>
#include <stdexcept>

OffsetInputSource::OffsetInputSource(
    std::shared_ptr<InputSource> proxied, qpdf_offset_t global_offset) :
    proxied(proxied),
    global_offset(global_offset)
{
    if (global_offset < 0) {
        throw std::logic_error("OffsetInputSource constructed with negative offset");
    }
    this->max_safe_offset = std::numeric_limits<qpdf_offset_t>::max() - global_offset;
}

qpdf_offset_t
OffsetInputSource::findAndSkipNextEOL()
{
    return this->proxied->findAndSkipNextEOL() - this->global_offset;
}

std::string const&
OffsetInputSource::getName() const
{
    return this->proxied->getName();
}

qpdf_offset_t
OffsetInputSource::tell()
{
    return this->proxied->tell() - this->global_offset;
}

void
OffsetInputSource::seek(qpdf_offset_t offset, int whence)
{
    if (whence == SEEK_SET) {
        if (offset > this->max_safe_offset) {
            std::ostringstream msg;
            msg.imbue(std::locale::classic());
            msg << "seeking to " << offset << " offset by " << global_offset
                << " would cause an overflow of the offset type";
            throw std::range_error(msg.str());
        }
        this->proxied->seek(offset + global_offset, whence);
    } else {
        this->proxied->seek(offset, whence);
    }
    if (tell() < 0) {
        throw std::runtime_error("offset input source: seek before beginning of file");
    }
}

void
OffsetInputSource::rewind()
{
    seek(0, SEEK_SET);
}

size_t
OffsetInputSource::read(char* buffer, size_t length)
{
    size_t result = this->proxied->read(buffer, length);
    this->setLastOffset(this->proxied->getLastOffset() - global_offset);
    return result;
}

void
OffsetInputSource::unreadCh(char ch)
{
    this->proxied->unreadCh(ch);
}
