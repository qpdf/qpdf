#include <qpdf/FileInputSource.hh>
#include <string.h>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFExc.hh>
#include <algorithm>

FileInputSource::Members::Members(bool close_file) :
    close_file(close_file),
    file(0)
{
}

FileInputSource::Members::~Members()
{
    if (this->file && this->close_file)
    {
	fclose(this->file);
    }
}

FileInputSource::FileInputSource() :
    m(new Members(false))
{
}

void
FileInputSource::setFilename(char const* filename)
{
    this->m = new Members(true);
    this->m->filename = filename;
    this->m->file = QUtil::safe_fopen(filename, "rb");
}

void
FileInputSource::setFile(
    char const* description, FILE* filep, bool close_file)
{
    this->m = new Members(close_file);
    this->m->filename = description;
    this->m->file = filep;
    this->seek(0, SEEK_SET);
}

FileInputSource::~FileInputSource()
{
}

qpdf_offset_t
FileInputSource::findAndSkipNextEOL()
{
    qpdf_offset_t result = 0;
    bool done = false;
    char buf[10240];
    while (! done)
    {
        qpdf_offset_t cur_offset = QUtil::tell(this->m->file);
        size_t len = this->read(buf, sizeof(buf));
        if (len == 0)
        {
            done = true;
            result = this->tell();
        }
        else
        {
            char* p1 = static_cast<char*>(memchr(buf, '\r', len));
            char* p2 = static_cast<char*>(memchr(buf, '\n', len));
            char* p = (p1 && p2) ? std::min(p1, p2) : p1 ? p1 : p2;
            if (p)
            {
                result = cur_offset + (p - buf);
                // We found \r or \n.  Keep reading until we get past
                // \r and \n characters.
                this->seek(result + 1, SEEK_SET);
                char ch;
                while (! done)
                {
                    if (this->read(&ch, 1) == 0)
                    {
                        done = true;
                    }
                    else if (! ((ch == '\r') || (ch == '\n')))
                    {
                        this->seek(-1, SEEK_CUR);
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
    return this->m->filename;
}

qpdf_offset_t
FileInputSource::tell()
{
    return QUtil::tell(this->m->file);
}

void
FileInputSource::seek(qpdf_offset_t offset, int whence)
{
    QUtil::os_wrapper(std::string("seek to ") +
                      this->m->filename + ", offset " +
		      QUtil::int_to_string(offset) + " (" +
		      QUtil::int_to_string(whence) + ")",
		      QUtil::seek(this->m->file, offset, whence));
}

void
FileInputSource::rewind()
{
    ::rewind(this->m->file);
}

size_t
FileInputSource::read(char* buffer, size_t length)
{
    this->last_offset = this->tell();
    size_t len = fread(buffer, 1, length, this->m->file);
    if (len == 0)
    {
        if (ferror(this->m->file))
        {
            throw QPDFExc(qpdf_e_system,
                          this->m->filename, "",
                          this->last_offset,
                          std::string("read ") +
                          QUtil::uint_to_string(length) + " bytes");
        }
        else if (length > 0)
        {
            this->seek(0, SEEK_END);
            this->last_offset = this->tell();
        }
    }
    return len;
}

void
FileInputSource::unreadCh(char ch)
{
    QUtil::os_wrapper(this->m->filename + ": unread character",
		      ungetc(static_cast<unsigned char>(ch), this->m->file));
}
