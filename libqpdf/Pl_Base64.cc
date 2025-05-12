#include <qpdf/Pl_Base64.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>

#include <cstring>
#include <stdexcept>

using namespace qpdf;

static char
to_c(unsigned int ch)
{
    return static_cast<char>(ch);
}

static unsigned char
to_uc(int ch)
{
    return static_cast<unsigned char>(ch);
}

static int
to_i(int i)
{
    return static_cast<int>(i);
}

Pl_Base64::Pl_Base64(char const* identifier, Pipeline* next, action_e action) :
    Pipeline(identifier, next),
    action(action)
{
    if (!next) {
        throw std::logic_error("Attempt to create Pl_Base64 with nullptr as next");
    }
}

void
Pl_Base64::write(unsigned char const* data, size_t len)
{
    if (finished) {
        throw std::logic_error("Pl_Base64 used after finished");
    }
    if (action == a_decode) {
        decode(data, len);
    } else {
        encode(data, len);
    }
}

void
Pl_Base64::decode(unsigned char const* data, size_t len)
{
    unsigned char const* p = data;
    while (len > 0) {
        if (!util::is_space(to_c(*p))) {
            buf[pos++] = *p;
            if (pos == 4) {
                flush();
            }
        }
        ++p;
        --len;
    }
}

void
Pl_Base64::encode(unsigned char const* data, size_t len)
{
    unsigned char const* p = data;
    while (len > 0) {
        buf[pos++] = *p;
        if (pos == 3) {
            flush();
        }
        ++p;
        --len;
    }
}

void
Pl_Base64::flush()
{
    if (action == a_decode) {
        flush_decode();
    } else {
        flush_encode();
    }
    reset();
}

void
Pl_Base64::flush_decode()
{
    if (end_of_data) {
        throw std::runtime_error(getIdentifier() + ": base64 decode: data follows pad characters");
    }
    int pad = 0;
    int shift = 18;
    int outval = 0;
    for (size_t i = 0; i < 4; ++i) {
        int v = 0;
        char ch = to_c(buf[i]);
        if ((ch >= 'A') && (ch <= 'Z')) {
            v = ch - 'A';
        } else if ((ch >= 'a') && (ch <= 'z')) {
            v = ch - 'a' + 26;
        } else if ((ch >= '0') && (ch <= '9')) {
            v = ch - '0' + 52;
        } else if ((ch == '+') || (ch == '-')) {
            v = 62;
        } else if ((ch == '/') || (ch == '_')) {
            v = 63;
        } else if ((ch == '=') && ((i == 3) || ((i == 2) && (buf[3] == '=')))) {
            ++pad;
            end_of_data = true;
            v = 0;
        } else {
            throw std::runtime_error(getIdentifier() + ": base64 decode: invalid input");
        }
        outval |= v << shift;
        shift -= 6;
    }
    unsigned char out[3] = {
        to_uc(outval >> 16),
        to_uc(0xff & (outval >> 8)),
        to_uc(0xff & outval),
    };

    next()->write(out, QIntC::to_size(3 - pad));
}

void
Pl_Base64::flush_encode()
{
    int outval = ((buf[0] << 16) | (buf[1] << 8) | (buf[2]));
    unsigned char out[4] = {
        to_uc(outval >> 18),
        to_uc(0x3f & (outval >> 12)),
        to_uc(0x3f & (outval >> 6)),
        to_uc(0x3f & outval),
    };
    for (size_t i = 0; i < 4; ++i) {
        int ch = to_i(out[i]);
        if (ch < 26) {
            ch += 'A';
        } else if (ch < 52) {
            ch -= 26;
            ch += 'a';
        } else if (ch < 62) {
            ch -= 52;
            ch += '0';
        } else if (ch == 62) {
            ch = '+';
        } else if (ch == 63) {
            ch = '/';
        }
        out[i] = to_uc(ch);
    }
    for (size_t i = 0; i < 3 - pos; ++i) {
        out[3 - i] = '=';
    }
    next()->write(out, 4);
}

void
Pl_Base64::finish()
{
    if (pos > 0) {
        if (finished) {
            throw std::logic_error("Pl_Base64 used after finished");
        }
        if (action == a_decode) {
            for (size_t i = pos; i < 4; ++i) {
                buf[i] = '=';
            }
        }
        flush();
    }
    finished = true;
    next()->finish();
}

void
Pl_Base64::reset()
{
    pos = 0;
    memset(buf, 0, 4);
}
