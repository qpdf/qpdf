#include <qpdf/Pl_SHA2.hh>
#include <stdexcept>
#include <cstdio>
#include <qpdf/PointerHolder.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFCryptoProvider.hh>

Pl_SHA2::Pl_SHA2(int bits, Pipeline* next) :
    Pipeline("sha2", next),
    in_progress(false)
{
    if (bits)
    {
        resetBits(bits);
    }
}

Pl_SHA2::~Pl_SHA2()
{
}

void
Pl_SHA2::write(unsigned char* buf, size_t len)
{
    if (! this->in_progress)
    {
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
        this->crypto->SHA2_update(data, bytes);
	bytes_left -= bytes;
        data += bytes;
    }

    if (this->getNext(true))
    {
        this->getNext()->write(buf, len);
    }
}

void
Pl_SHA2::finish()
{
    if (this->getNext(true))
    {
        this->getNext()->finish();
    }
    this->crypto->SHA2_finalize();
    this->in_progress = false;
}

void
Pl_SHA2::resetBits(int bits)
{
    if (this->in_progress)
    {
	throw std::logic_error(
	    "bit reset requested for in-progress SHA2 Pipeline");
    }
    this->crypto = QPDFCryptoProvider::getImpl();
    this->crypto->SHA2_init(bits);
}

std::string
Pl_SHA2::getRawDigest()
{
    if (this->in_progress)
    {
	throw std::logic_error(
	    "digest requested for in-progress SHA2 Pipeline");
    }
    return this->crypto->SHA2_digest();
}

std::string
Pl_SHA2::getHexDigest()
{
    if (this->in_progress)
    {
	throw std::logic_error(
	    "digest requested for in-progress SHA2 Pipeline");
    }
    return QUtil::hex_encode(getRawDigest());
}
