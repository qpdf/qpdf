#include <qpdf/assert_debug.h>

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
    in_buffer.append(reinterpret_cast<const char*>(data), len);
}

void
Pl_Base64::decode(std::string_view data)
{
    auto len = data.size();
    auto res = (len / 4u + 1u) * 3u;
    out_buffer.reserve(res);
    unsigned char const* p = reinterpret_cast<const unsigned char*>(data.data());
    while (len > 0) {
        if (!util::is_space(to_c(*p))) {
            buf[pos++] = *p;
            if (pos == 4) {
                flush_decode();
            }
        }
        ++p;
        --len;
    }
    if (pos > 0) {
        for (size_t i = pos; i < 4; ++i) {
            buf[i] = '=';
        }
        flush_decode();
    }
    qpdf_assert_debug(out_buffer.size() <= res);
}

void
Pl_Base64::encode(std::string_view data)
{
    auto len = data.size();
    static const size_t max_len = (std::string().max_size() / 4u - 1u) * 3u;
    // Change to constexpr once AppImage is build with GCC >= 12
    if (len > max_len) {
        throw std::length_error(getIdentifier() + ": base64 decode: data exceeds maximum length");
    }

    auto res = (len / 3u + 1u) * 4u;
    out_buffer.reserve(res);
    unsigned char const* p = reinterpret_cast<const unsigned char*>(data.data());
    while (len > 0) {
        buf[pos++] = *p;
        if (pos == 3) {
            flush_encode();
        }
        ++p;
        --len;
    }
    if (pos > 0) {
        flush_encode();
    }
    qpdf_assert_debug(out_buffer.size() <= res);
}

void
Pl_Base64::flush_decode()
{
    if (end_of_data) {
        throw std::runtime_error(getIdentifier() + ": base64 decode: data follows pad characters");
    }
    size_t pad = 0;
    int shift = 18;
    int outval = 0;
    for (size_t i = 0; i < 4; ++i) {
        int v = 0;
        char ch = to_c(buf[i]);
        if (ch >= 'A' && ch <= 'Z') {
            v = ch - 'A';
        } else if (ch >= 'a' && ch <= 'z') {
            v = ch - 'a' + 26;
        } else if (ch >= '0' && ch <= '9') {
            v = ch - '0' + 52;
        } else if (ch == '+' || ch == '-') {
            v = 62;
        } else if (ch == '/' || ch == '_') {
            v = 63;
        } else if (ch == '=' && (i == 3 || (i == 2 && buf[3] == '='))) {
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

    out_buffer.append(reinterpret_cast<const char*>(out), 3u - pad);
    reset();
}

void
Pl_Base64::flush_encode()
{
    int outval = ((buf[0] << 16) | (buf[1] << 8) | buf[2]);
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
    out_buffer.append(reinterpret_cast<const char*>(out), 4);
    reset();
}

void
Pl_Base64::finish()
{
    if (action == a_decode) {
        decode(in_buffer);

    } else {
        encode(in_buffer);
    }
    in_buffer.clear();
    in_buffer.shrink_to_fit();
    next()->write(reinterpret_cast<unsigned char const*>(out_buffer.data()), out_buffer.size());
    out_buffer.clear();
    out_buffer.shrink_to_fit();
    next()->finish();
}

void
Pl_Base64::reset()
{
    pos = 0;
    memset(buf, 0, 4);
}
