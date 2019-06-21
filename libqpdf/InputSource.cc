#include <qpdf/InputSource.hh>
#include <string.h>
#include <stdexcept>
#include <qpdf/QTC.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/QIntC.hh>


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
    size_t line_length = QIntC::to_size(eol - offset);
    if (line_length < max_line_length)
    {
        buf[line_length] = '\0';
    }
    return std::string(buf);
}

bool
InputSource::findFirst(char const* start_chars,
                       qpdf_offset_t offset, size_t len,
                       Finder& finder)
{
    // Basic approach: search for the first character of start_chars
    // starting from offset but not going past len (if len != 0). Once
    // the first character is found, see if it is the beginning of a
    // sequence of characters matching start_chars. If so, call
    // finder.check() to do caller-specific additional checks. If not,
    // keep searching.

    // This code is tricky and highly subject to off-by-one or other
    // edge case logic errors. See comments throughout that explain
    // how we're not missing any edge cases. There are also tests
    // specifically constructed to make sure we caught the edge cases
    // in testing.

    char buf[1025]; // size known to input_source.cc in libtests
    // To enable us to guarantee null-termination, save an extra byte
    // so that buf[size] is valid memory.
    size_t size = sizeof(buf) - 1;
    if ((strlen(start_chars) < 1) || (strlen(start_chars) > size))
    {
        throw std::logic_error(
            "InputSource::findSource called with"
            " too small or too large of a character sequence");
    }

    char* p = 0;
    qpdf_offset_t buf_offset = offset;
    size_t bytes_read = 0;

    // Guarantee that we return from this loop. Each time through, we
    // either return, advance p, or restart the loop with a condition
    // that will cause return on the next pass. Eventually we will
    // either be out of range or hit EOF, either of which forces us to
    // return.
    while (true)
    {
        // Do we need to read more data? Pretend size = 5, buf starts
        // at 0, and start_chars has 3 characters. buf[5] is valid and
        // null. If p == 2, start_chars could be buf[2] through
        // buf[4], so p + strlen(start_chars) == buf + size is okay.
        // If p points to buf[size], since strlen(start_chars) is
        // always >= 1, this overflow test will be correct for that
        // case regardless of start_chars.
        if ((p == 0) || ((p + strlen(start_chars)) > (buf + bytes_read)))
        {
            if (p)
            {
                QTC::TC("libtests", "InputSource read next block",
                        ((p == buf + bytes_read) ? 0 : 1));
                buf_offset += (p - buf);
            }
            this->seek(buf_offset, SEEK_SET);
            // Read into buffer and zero out the rest of the buffer
            // including buf[size]. We allocated an extra byte so that
            // we could guarantee null termination as an extra
            // protection against overrun when using string functions.
            bytes_read = this->read(buf, size);
            if (bytes_read < strlen(start_chars))
            {
                QTC::TC("libtests", "InputSource find EOF",
                        bytes_read == 0 ? 0 : 1);
                return false;
            }
            memset(buf + bytes_read, '\0', 1 + (size - bytes_read));
            p = buf;
        }

        // Search for the first character.
        if ((p = static_cast<char*>(
                 memchr(p, start_chars[0],
                        bytes_read - QIntC::to_size(p - buf)))) != 0)
        {
            if (p == buf)
            {
                QTC::TC("libtests", "InputSource found match at buf[0]");
            }
            // Found first letter.
            if (len != 0)
            {
                // Make sure it's in range.
                size_t p_relative_offset =
                    QIntC::to_size((p - buf) + (buf_offset - offset));
                if (p_relative_offset >= len)
                {
                    // out of range
                    QTC::TC("libtests", "InputSource out of range");
                    return false;
                }
            }
            if ((p + strlen(start_chars)) > (buf + bytes_read))
            {
                // If there are not enough bytes left in the file for
                // start_chars, we will detect this on the next pass
                // as EOF and return.
                QTC::TC("libtests", "InputSource not enough bytes");
                continue;
            }

            // See if p points to a sequence matching start_chars. We
            // already checked above to make sure we are not going to
            // overrun memory.
            if (strncmp(p, start_chars, strlen(start_chars)) == 0)
            {
                // Call finder.check() with the input source
                // positioned to the point of the match.
                this->seek(buf_offset + (p - buf), SEEK_SET);
                if (finder.check())
                {
                    return true;
                }
                else
                {
                    QTC::TC("libtests", "InputSource start_chars matched but not check");
                }
            }
            else
            {
                QTC::TC("libtests", "InputSource first char matched but not string");
            }
            // This occurrence of the first character wasn't a match.
            // Skip over it and keep searching.
            ++p;
        }
        else
        {
            // Trigger reading the next block
            p = buf + bytes_read;
        }
    }
    throw std::logic_error("InputSource after while (true)");
}

bool
InputSource::findLast(char const* start_chars,
                      qpdf_offset_t offset, size_t len,
                      Finder& finder)
{
    bool found = false;
    qpdf_offset_t after_found_offset = 0;
    qpdf_offset_t cur_offset = offset;
    size_t cur_len = len;
    while (this->findFirst(start_chars, cur_offset, cur_len, finder))
    {
        if (found)
        {
            QTC::TC("libtests", "InputSource findLast found more than one");
        }
        else
        {
            found = true;
        }
        after_found_offset = this->tell();
        cur_offset = after_found_offset;
        cur_len = len - QIntC::to_size((cur_offset - offset));
    }
    if (found)
    {
        this->seek(after_found_offset, SEEK_SET);
    }
    return found;
}
