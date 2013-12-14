#include <qpdf/OffsetInputSource.hh>

OffsetInputSource::OffsetInputSource(PointerHolder<InputSource> proxied,
                                     qpdf_offset_t global_offset) :
    proxied(proxied),
    global_offset(global_offset)
{
}

OffsetInputSource::~OffsetInputSource()
{
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
    if (whence == SEEK_SET)
    {
        this->proxied->seek(offset + global_offset, whence);
    }
    else
    {
        this->proxied->seek(offset, whence);
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
