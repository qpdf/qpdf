#include <qpdf/Pl_MD5.hh>

#include <stdexcept>

Pl_MD5::Pl_MD5(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    in_progress(false),
    enabled(true),
    persist_across_finish(false)
{
}

void
Pl_MD5::write(unsigned char const* buf, size_t len)
{
    if (this->enabled) {
        if (!this->in_progress) {
            this->md5.reset();
            this->in_progress = true;
        }

        // Write in chunks in case len is too big to fit in an int.
        // Assume int is at least 32 bits.
        static size_t const max_bytes = 1 << 30;
        size_t bytes_left = len;
        unsigned char const* data = buf;
        while (bytes_left > 0) {
            size_t bytes = (bytes_left >= max_bytes ? max_bytes : bytes_left);
            this->md5.encodeDataIncrementally(
                reinterpret_cast<char const*>(data), bytes);
            bytes_left -= bytes;
            data += bytes;
        }
    }

    this->getNext()->write(buf, len);
}

void
Pl_MD5::finish()
{
    this->getNext()->finish();
    if (!this->persist_across_finish) {
        this->in_progress = false;
    }
}

void
Pl_MD5::enable(bool enabled)
{
    this->enabled = enabled;
}

void
Pl_MD5::persistAcrossFinish(bool persist)
{
    this->persist_across_finish = persist;
}

std::string
Pl_MD5::getHexDigest()
{
    if (!this->enabled) {
        throw std::logic_error("digest requested for a disabled MD5 Pipeline");
    }
    this->in_progress = false;
    return this->md5.unparse();
}
