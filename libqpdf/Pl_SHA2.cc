#include <qpdf/Pl_SHA2.hh>
#include <stdexcept>
#include <cstdio>
#include <qpdf/PointerHolder.hh>
#include <qpdf/QUtil.hh>

Pl_SHA2::Pl_SHA2(int bits, Pipeline* next) :
    Pipeline("sha2", next),
    in_progress(false),
    bits(0)
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
Pl_SHA2::badBits()
{
    throw std::logic_error("Pl_SHA2 has unexpected value for bits");
}

void
Pl_SHA2::write(unsigned char* buf, size_t len)
{
    if (! this->in_progress)
    {
        switch (bits)
        {
          case 256:
            sph_sha256_init(&this->ctx256);
            break;
          case 384:
            sph_sha384_init(&this->ctx384);
            break;
          case 512:
            sph_sha512_init(&this->ctx512);
            break;
          default:
            badBits();
            break;
        }
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
        switch (bits)
        {
          case 256:
            sph_sha256(&this->ctx256, data, bytes);
            break;
          case 384:
            sph_sha384(&this->ctx384, data, bytes);
            break;
          case 512:
            sph_sha512(&this->ctx512, data, bytes);
            break;
          default:
            badBits();
            break;
        }
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
    switch (bits)
    {
      case 256:
        sph_sha256_close(&this->ctx256, sha256sum);
        break;
      case 384:
        sph_sha384_close(&this->ctx384, sha384sum);
        break;
      case 512:
        sph_sha512_close(&this->ctx512, sha512sum);
        break;
      default:
        badBits();
        break;
    }
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
    if (! ((bits == 256) || (bits == 384) || (bits == 512)))
    {
	throw std::logic_error("Pl_SHA2 called with bits != 256, 384, or 512");
    }
    this->bits = bits;
}

std::string
Pl_SHA2::getRawDigest()
{
    std::string result;
    switch (bits)
    {
      case 256:
        result = std::string(reinterpret_cast<char*>(this->sha256sum),
                             sizeof(this->sha256sum));
        break;
      case 384:
        result = std::string(reinterpret_cast<char*>(this->sha384sum),
                             sizeof(this->sha384sum));
        break;
      case 512:
        result = std::string(reinterpret_cast<char*>(this->sha512sum),
                             sizeof(this->sha512sum));
        break;
      default:
        badBits();
        break;
    }
    return result;
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
