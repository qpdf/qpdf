#include <qpdf/ClosedFileInputSource.hh>

#include <qpdf/FileInputSource.hh>

ClosedFileInputSource::ClosedFileInputSource(char const* filename) :
    filename(filename),
    offset(0),
    stay_open(false)
{
}

ClosedFileInputSource::~ClosedFileInputSource() // NOLINT (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
}

void
ClosedFileInputSource::before()
{
    if (nullptr == this->fis) {
        this->fis = std::make_shared<FileInputSource>(this->filename.c_str());
        this->fis->seek(this->offset, SEEK_SET);
        this->fis->setLastOffset(this->last_offset);
    }
}

void
ClosedFileInputSource::after()
{
    this->last_offset = this->fis->getLastOffset();
    this->offset = this->fis->tell();
    if (this->stay_open) {
        return;
    }
    this->fis = nullptr;
}

qpdf_offset_t
ClosedFileInputSource::findAndSkipNextEOL()
{
    before();
    qpdf_offset_t r = this->fis->findAndSkipNextEOL();
    after();
    return r;
}

std::string const&
ClosedFileInputSource::getName() const
{
    return this->filename;
}

qpdf_offset_t
ClosedFileInputSource::tell()
{
    before();
    qpdf_offset_t r = this->fis->tell();
    after();
    return r;
}

void
ClosedFileInputSource::seek(qpdf_offset_t offset, int whence)
{
    before();
    this->fis->seek(offset, whence);
    after();
}

void
ClosedFileInputSource::rewind()
{
    this->offset = 0;
    if (this->fis.get()) {
        this->fis->rewind();
    }
}

size_t
ClosedFileInputSource::read(char* buffer, size_t length)
{
    before();
    size_t r = this->fis->read(buffer, length);
    after();
    return r;
}

void
ClosedFileInputSource::unreadCh(char ch)
{
    before();
    this->fis->unreadCh(ch);
    // Don't call after -- the file has to stay open after this operation.
}

void
ClosedFileInputSource::stayOpen(bool val)
{
    this->stay_open = val;
    if ((!val) && this->fis.get()) {
        after();
    }
}
