#include <qpdf/Pl_MD5.hh>

#include <stdexcept>

Pl_MD5::Pl_MD5(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next)
{
    if (!next) {
        throw std::logic_error("Attempt to create Pl_MD5 with nullptr as next");
    }
}

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

    next()->write(buf, len);
}

void
Pl_MD5::finish()
{
    next()->finish();
    if (!persist_across_finish) {
        in_progress = false;
    }
}

void
Pl_MD5::enable(bool is_enabled)
{
    enabled = is_enabled;
}

void
Pl_MD5::persistAcrossFinish(bool persist)
{
    persist_across_finish = persist;
}

std::string
Pl_MD5::getHexDigest()
{
    if (!enabled) {
        throw std::logic_error("digest requested for a disabled MD5 Pipeline");
    }
    in_progress = false;
    return md5.unparse();
}
