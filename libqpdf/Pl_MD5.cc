#include <qpdf/Pl_MD5.hh>

#include <stdexcept>

void
Pl_MD5::write(unsigned char const* buf, size_t len)
{
    if (enabled) {
        if (!in_progress) {
            md5.reset();
            in_progress = true;
        }

        // Write in chunks in case len is too big to fit in an int. Assume int is at least 32 bits.
        static size_t const max_bytes = 1 << 30;
        size_t bytes_left = len;
        unsigned char const* data = buf;
        while (bytes_left > 0) {
            size_t bytes = (bytes_left >= max_bytes ? max_bytes : bytes_left);
            md5.encodeDataIncrementally(reinterpret_cast<char const*>(data), bytes);
            bytes_left -= bytes;
            data += bytes;
        }
    }

    if (next()) {
        next()->write(buf, len);
    }
}
