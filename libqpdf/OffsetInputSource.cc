#include <qpdf/OffsetInputSource.hh>

#include <qpdf/Util.hh>

#include <limits>
#include <sstream>
#include <stdexcept>

using namespace qpdf;

OffsetInputSource::OffsetInputSource(
    std::shared_ptr<InputSource> proxied, qpdf_offset_t global_offset) :
    proxied(proxied),
    global_offset(global_offset)
{
    util::assertion(global_offset >= 0, "OffsetInputSource constructed with negative offset");
    max_safe_offset = std::numeric_limits<qpdf_offset_t>::max() - global_offset;
}

qpdf_offset_t
OffsetInputSource::findAndSkipNextEOL()
{
    return proxied->findAndSkipNextEOL() - global_offset;
}

std::string const&
OffsetInputSource::getName() const
{
    return proxied->getName();
}

qpdf_offset_t
OffsetInputSource::tell()
{
    return proxied->tell() - global_offset;
}

void
OffsetInputSource::seek(qpdf_offset_t offset, int whence)
{
    if (whence == SEEK_SET) {
        if (offset > max_safe_offset) {
            std::ostringstream msg;
            msg.imbue(std::locale::classic());
            msg << "seeking to " << offset << " offset by " << global_offset
                << " would cause an overflow of the offset type";
            throw std::range_error(msg.str());
        }
        proxied->seek(offset + global_offset, whence);
    } else {
        proxied->seek(offset, whence);
    }
    util::no_ci_rt_error_if(tell() < 0, "offset input source: seek before beginning of file");
}

void
OffsetInputSource::rewind()
{
    seek(0, SEEK_SET);
}

size_t
OffsetInputSource::read(char* buffer, size_t length)
{
    size_t result = proxied->read(buffer, length);
    setLastOffset(proxied->getLastOffset() - global_offset);
    return result;
}

void
OffsetInputSource::unreadCh(char ch)
{
    proxied->unreadCh(ch);
}
