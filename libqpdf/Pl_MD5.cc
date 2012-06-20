#include <qpdf/Pl_MD5.hh>
#include <stdexcept>

Pl_MD5::Pl_MD5(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    in_progress(false)
{
}

Pl_MD5::~Pl_MD5()
{
}

void
Pl_MD5::write(unsigned char* buf, size_t len)
{
    if (! this->in_progress)
    {
	this->md5.reset();
	this->in_progress = true;
    }

    // Write in chunks in case len is too big to fit in an int.
    // Assume int is at least 32 bits.
    static size_t const max_bytes = 1 << 30;
    size_t bytes_left = len;
    unsigned char* data = buf;
    while (bytes_left > 0)
    {
	size_t bytes = (bytes_left >= max_bytes ? max_bytes : bytes_left);
        this->md5.encodeDataIncrementally((char*) data, (int)bytes);
	bytes_left -= bytes;
        data += bytes;
    }

    this->getNext()->write(buf, len);
}

void
Pl_MD5::finish()
{
    this->getNext()->finish();
    this->in_progress = false;
}

std::string
Pl_MD5::getHexDigest()
{
    if (this->in_progress)
    {
	throw std::logic_error(
	    "digest requested for in-progress MD5 Pipeline");
    }
    return this->md5.unparse();
}
