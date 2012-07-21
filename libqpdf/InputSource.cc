#include <qpdf/InputSource.hh>
#include <string.h>
#include <qpdf/PointerHolder.hh>

void
InputSource::setLastOffset(qpdf_offset_t offset)
{
    this->last_offset = offset;
}

qpdf_offset_t
InputSource::getLastOffset() const
{
    return this->last_offset;
}

std::string
InputSource::readLine(size_t max_line_length)
{
    // Return at most max_line_length characters from the next line.
    // Lines are terminated by one or more \r or \n characters.
    // Consume the trailing newline characters but don't return them.
    // After this is called, the file will be positioned after a line
    // terminator or at the end of the file, and last_offset will
    // point to position the file had when this method was called.

    qpdf_offset_t offset = this->tell();
    char* buf = new char[max_line_length + 1];
    PointerHolder<char> bp(true, buf);
    memset(buf, '\0', max_line_length + 1);
    this->read(buf, max_line_length);
    this->seek(offset, SEEK_SET);
    qpdf_offset_t eol = this->findAndSkipNextEOL();
    this->last_offset = offset;
    size_t line_length = eol - offset;
    if (line_length < max_line_length)
    {
        buf[line_length] = '\0';
    }
    return std::string(buf);
}
