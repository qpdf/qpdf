#include <qpdf/Pl_AES_PDF.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QUtil.hh>
#include <cstring>
#include <stdexcept>
#include <string>

bool Pl_AES_PDF::use_static_iv = false;

Pl_AES_PDF::Pl_AES_PDF(char const* identifier, Pipeline* next, bool encrypt, std::string key) :
    Pipeline(identifier, next),
    key(key),
    crypto(QPDFCryptoProvider::getImpl()),
    encrypt(encrypt)
{
    if (!next) {
        throw std::logic_error("Attempt to create Pl_AES_PDF with nullptr as next");
    }
    if (!(key.size() == 32 || key.size() == 16)) {
        throw std::runtime_error("unsupported key length");
    }
    inbuf.fill(0);
    outbuf.fill(0);
    cbc_block.fill(0);
}

void
Pl_AES_PDF::useZeroIV()
{
    use_zero_iv = true;
}

void
Pl_AES_PDF::disablePadding()
{
    disable_padding = true;
}

void
Pl_AES_PDF::setIV(std::string&& iv)
{
    if (iv.size() != buf_size) {
        throw std::logic_error(
            "Pl_AES_PDF: specified initialization vector size in bytes must be " +
            std::to_string(buf_size));
    }
    use_specified_iv = true;
    specified_iv = std::move(iv);
}

void
Pl_AES_PDF::disableCBC()
{
    cbc_mode = false;
}

void
Pl_AES_PDF::useStaticIV()
{
    use_static_iv = true;
}

void
Pl_AES_PDF::write(unsigned char const* data, size_t len)
{
    if (!first) {
        throw std::logic_error("AES pipeline: write called more than once");
    }
    first = false;

    if (encrypt) {
        write_encrypt(data, len);
    } else {
        write_decrypt(data, len);
    }
}

void
Pl_AES_PDF::write_decrypt(unsigned char const* data, size_t len)
{
    unsigned char const* p = data;
    size_t reps = len < buf_size ? 0 : (len - 1) / buf_size;
    size_t bytes_left = len - (buf_size * reps);

    if (cbc_mode) {
        if (use_zero_iv || use_specified_iv) {
            // Initialize vector with zeroes; zero vector was not written to the beginning of
            // the input file.
            initializeVector();
        } else {
            // Take the first block of input as the initialization vector. Don't write it to output.
            if (len < buf_size) {
                return;
            }
            memcpy(cbc_block.data(), p, buf_size);
            p += buf_size;
            if (reps > 0) {
                --reps;
            } else {
                bytes_left = 0;
            }
        }
    }
    crypto->rijndael_init(
        false,
        reinterpret_cast<const unsigned char*>(key.data()),
        key.size(),
        cbc_mode,
        cbc_block.data());

    for (size_t i = 0; i < reps; ++i) {
        flush_decrypt(p, false);
        p += buf_size;
    }

    if (bytes_left != buf_size) {
        // This is never supposed to happen as the output is always supposed to be padded.
        // However, we have encountered files for which the output is not a multiple of the
        // block size.  In this case, pad with zeroes and hope for the best.
        if (bytes_left >= buf_size) {
            throw std::logic_error("buffer overflow in AES encryption pipeline");
        }
        std::memcpy(inbuf.data(), p, bytes_left);
        std::memset(inbuf.data() + bytes_left, 0, buf_size - bytes_left);
        flush_decrypt(inbuf.data(), !disable_padding);
    } else {
        flush_decrypt(p, !disable_padding);
    }

    crypto->rijndael_finalize();
}

void
Pl_AES_PDF::flush_decrypt(const unsigned char* const_in, bool strip_padding)
{
    auto in = const_cast<unsigned char*>(const_in);
    crypto->rijndael_process(in, outbuf.data());
    unsigned int bytes = buf_size;
    if (strip_padding) {
        unsigned char last = outbuf[buf_size - 1];
        if (last <= buf_size) {
            bool strip = true;
            for (unsigned int i = 1; i <= last; ++i) {
                if (outbuf[buf_size - i] != last) {
                    strip = false;
                    break;
                }
            }
            if (strip) {
                bytes -= last;
            }
        }
    }
    next()->write(outbuf.data(), bytes);
}

void
Pl_AES_PDF::write_encrypt(const unsigned char* data, size_t len)
{
    if (cbc_mode) {
        // Set cbc_block to the initialization vector, and if not zero, write it to the
        // output stream.
        initializeVector();
        if (!(use_zero_iv || use_specified_iv)) {
            next()->write(cbc_block.data(), buf_size);
        }
    }
    crypto->rijndael_init(
        true,
        reinterpret_cast<const unsigned char*>(key.data()),
        key.size(),
        cbc_mode,
        cbc_block.data());

    unsigned char const* p = data;
    size_t reps = len < buf_size ? 0 : (len - 1) / buf_size;

    for (size_t i = 0; i < reps; ++i) {
        flush_encrypt(p);
        p += buf_size;
    }

    size_t bytes_left = len - (buf_size * reps);
    std::memcpy(inbuf.data(), p, bytes_left);

    if (bytes_left == buf_size) {
        flush_encrypt(inbuf.data());
    }
    if (!disable_padding) {
        // Pad as described in section 3.5.1 of version 1.7 of the PDF specification, including
        // providing an entire block of padding if the input was a multiple of 16 bytes.
        size_t offset = bytes_left == buf_size ? 0 : bytes_left;
        unsigned char pad = QIntC::to_uchar(buf_size - offset);
        std::memcpy(inbuf.data(), p, bytes_left);
        memset(inbuf.data() + offset, pad, pad);
        flush_encrypt(inbuf.data());
    }
    crypto->rijndael_finalize();
}

void
Pl_AES_PDF::flush_encrypt(const unsigned char* const_in)
{
    auto in = const_cast<unsigned char*>(const_in);
    crypto->rijndael_process(in, outbuf.data());
    next()->write(outbuf.data(), buf_size);
}

void
Pl_AES_PDF::finish()
{
    next()->finish();
}

void
Pl_AES_PDF::initializeVector()
{
    if (use_zero_iv) {
        for (unsigned int i = 0; i < buf_size; ++i) {
            cbc_block[i] = 0;
        }
    } else if (use_specified_iv) {
        std::memcpy(cbc_block.data(), specified_iv.data(), buf_size);
    } else if (use_static_iv) {
        for (unsigned int i = 0; i < buf_size; ++i) {
            cbc_block[i] = static_cast<unsigned char>(14U * (1U + i));
        }
    } else {
        QUtil::initializeWithRandomBytes(cbc_block.data(), buf_size);
    }
}
