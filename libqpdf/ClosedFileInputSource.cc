#include <qpdf/ClosedFileInputSource.hh>

#include <qpdf/FileInputSource.hh>

ClosedFileInputSource::Members::Members(char const* filename) :
    filename(filename),
    offset(0),
    stay_open(false)
{
}

ClosedFileInputSource::ClosedFileInputSource(char const* filename) :
    m(new Members(filename))
{
}

ClosedFileInputSource::~ClosedFileInputSource()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
}

void
ClosedFileInputSource::before()
{
    if (0 == this->m->fis.get()) {
        this->m->fis = std::make_shared<FileInputSource>();
        this->m->fis->setFilename(this->m->filename.c_str());
        this->m->fis->seek(this->m->offset, SEEK_SET);
        this->m->fis->setLastOffset(this->last_offset);
    }
}

void
ClosedFileInputSource::after()
{
    this->last_offset = this->m->fis->getLastOffset();
    this->m->offset = this->m->fis->tell();
    if (this->m->stay_open) {
        return;
    }
    this->m->fis = 0;
}

qpdf_offset_t
ClosedFileInputSource::findAndSkipNextEOL()
{
    before();
    qpdf_offset_t r = this->m->fis->findAndSkipNextEOL();
    after();
    return r;
}

std::string const&
ClosedFileInputSource::getName() const
{
    return this->m->filename;
}

qpdf_offset_t
ClosedFileInputSource::tell()
{
    before();
    qpdf_offset_t r = this->m->fis->tell();
    after();
    return r;
}

void
ClosedFileInputSource::seek(qpdf_offset_t offset, int whence)
{
    before();
    this->m->fis->seek(offset, whence);
    after();
}

void
ClosedFileInputSource::rewind()
{
    this->m->offset = 0;
    if (this->m->fis.get()) {
        this->m->fis->rewind();
    }
}

size_t
ClosedFileInputSource::read(char* buffer, size_t length)
{
    before();
    size_t r = this->m->fis->read(buffer, length);
    after();
    return r;
}

void
ClosedFileInputSource::unreadCh(char ch)
{
    before();
    this->m->fis->unreadCh(ch);
    // Don't call after -- the file has to stay open after this
    // operation.
}

void
ClosedFileInputSource::stayOpen(bool val)
{
    this->m->stay_open = val;
    if ((!val) && this->m->fis.get()) {
        after();
    }
}
