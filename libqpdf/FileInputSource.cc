#include <qpdf/FileInputSource.hh>
#include <string.h>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFExc.hh>
#include <algorithm>

FileInputSource::FileInputSource() :
    close_file(false),
    file(0)
{
}

void
FileInputSource::setFilename(char const* filename)
{
    destroy();
    this->filename = filename;
    this->close_file = true;
    this->file = QUtil::safe_fopen(this->filename.c_str(), "rb");
}

void
FileInputSource::setFile(
    char const* description, FILE* filep, bool close_file)
{
    destroy();
    this->filename = description;
    this->close_file = close_file;
    this->file = filep;
    this->seek(0, SEEK_SET);
}

FileInputSource::~FileInputSource()
{
    destroy();
}

void
FileInputSource::destroy()
{
    if (this->file && this->close_file)
    {
	fclose(this->file);
	this->file = 0;
    }
}

qpdf_offset_t
FileInputSource::findAndSkipNextEOL()
{
    qpdf_offset_t result = 0;
    bool done = false;
    char buf[10240];
    while (! done)
    {
        qpdf_offset_t cur_offset = QUtil::tell(this->file);
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
                        this->unreadCh(ch);
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
    return this->filename;
}

qpdf_offset_t
FileInputSource::tell()
{
    return QUtil::tell(this->file);
}

void
FileInputSource::seek(qpdf_offset_t offset, int whence)
{
    QUtil::os_wrapper(std::string("seek to ") + this->filename + ", offset " +
		      QUtil::int_to_string(offset) + " (" +
		      QUtil::int_to_string(whence) + ")",
		      QUtil::seek(this->file, offset, whence));
}

void
FileInputSource::rewind()
{
    ::rewind(this->file);
}

size_t
FileInputSource::read(char* buffer, size_t length)
{
    this->last_offset = QUtil::tell(this->file);
    size_t len = fread(buffer, 1, length, this->file);
    if ((len == 0) && ferror(this->file))
    {
	throw QPDFExc(qpdf_e_system,
		      this->filename, "",
		      this->last_offset,
		      std::string("read ") +
		      QUtil::int_to_string(length) + " bytes");
    }
    return len;
}

void
FileInputSource::unreadCh(char ch)
{
    QUtil::os_wrapper(this->filename + ": unread character",
		      ungetc(static_cast<unsigned char>(ch), this->file));
}
