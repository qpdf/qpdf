#include <qpdf/SHA2_native.hh>

#include <qpdf/QUtil.hh>
#include <cstdio>
#include <stdexcept>

SHA2_native::SHA2_native(int bits) :
    bits(bits)
{
    switch (bits) {
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
}

void
SHA2_native::badBits()
{
    throw std::logic_error("SHA2_native has bits != 256, 384, or 512");
}

void
SHA2_native::update(unsigned char const* buf, size_t len)
{
    switch (bits) {
    case 256:
        sph_sha256(&this->ctx256, buf, len);
        break;
    case 384:
        sph_sha384(&this->ctx384, buf, len);
        break;
    case 512:
        sph_sha512(&this->ctx512, buf, len);
        break;
    default:
        badBits();
        break;
    }
}

void
SHA2_native::finalize()
{
    switch (bits) {
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
}

std::string
SHA2_native::getRawDigest()
{
    std::string result;
    switch (bits) {
    case 256:
        result = std::string(reinterpret_cast<char*>(this->sha256sum), sizeof(this->sha256sum));
        break;
    case 384:
        result = std::string(reinterpret_cast<char*>(this->sha384sum), sizeof(this->sha384sum));
        break;
    case 512:
        result = std::string(reinterpret_cast<char*>(this->sha512sum), sizeof(this->sha512sum));
        break;
    default:
        badBits();
        break;
    }
    return result;
}
