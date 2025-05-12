#include <qpdf/Pl_AES_PDF.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QUtil.hh>
#include <cstring>
#include <stdexcept>
#include <string>

bool Pl_AES_PDF::use_static_iv = false;

Pl_AES_PDF::Pl_AES_PDF(
    char const* identifier,
    Pipeline* next,
    bool encrypt,
    unsigned char const* key,
    size_t key_bytes) :
    Pipeline(identifier, next),
    crypto(QPDFCryptoProvider::getImpl()),
    encrypt(encrypt),
    key_bytes(key_bytes)
{
    if (!next) {
        throw std::logic_error("Attempt to create Pl_AES_PDF with nullptr as next");
    }
    if (!(key_bytes == 32 || key_bytes == 16)) {
        throw std::runtime_error("unsupported key length");
    }
    this->key = std::make_unique<unsigned char[]>(key_bytes);
    std::memcpy(this->key.get(), key, key_bytes);
    std::memset(this->inbuf, 0, this->buf_size);
    std::memset(this->outbuf, 0, this->buf_size);
    std::memset(this->cbc_block, 0, this->buf_size);
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
Pl_AES_PDF::setIV(unsigned char const* iv, size_t bytes)
{
    if (bytes != buf_size) {
        throw std::logic_error(
            "Pl_AES_PDF: specified initialization vector"
            " size in bytes must be " +
            std::to_string(bytes));
    }
    use_specified_iv = true;
    memcpy(specified_iv, iv, bytes);
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
    size_t bytes_left = len;
    unsigned char const* p = data;

    while (bytes_left > 0) {
        if (offset == buf_size) {
            flush(false);
        }

        size_t available = buf_size - offset;
        size_t bytes = (bytes_left < available ? bytes_left : available);
        bytes_left -= bytes;
        std::memcpy(inbuf + offset, p, bytes);
        offset += bytes;
        p += bytes;
    }
}

void
Pl_AES_PDF::finish()
{
    if (encrypt) {
        if (offset == buf_size) {
            flush(false);
        }
        if (!disable_padding) {
            // Pad as described in section 3.5.1 of version 1.7 of the PDF specification, including
            // providing an entire block of padding if the input was a multiple of 16 bytes.
            unsigned char pad = QIntC::to_uchar(buf_size - offset);
            memset(inbuf + offset, pad, pad);
            offset = buf_size;
            flush(false);
        }
    } else {
        if (offset != buf_size) {
            // This is never supposed to happen as the output is always supposed to be padded.
            // However, we have encountered files for which the output is not a multiple of the
            // block size.  In this case, pad with zeroes and hope for the best.
            if (offset >= buf_size) {
                throw std::logic_error("buffer overflow in AES encryption pipeline");
            }
            std::memset(inbuf + offset, 0, buf_size - offset);
            offset = buf_size;
        }
        flush(!disable_padding);
    }
    crypto->rijndael_finalize();
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
        std::memcpy(cbc_block, specified_iv, buf_size);
    } else if (use_static_iv) {
        for (unsigned int i = 0; i < buf_size; ++i) {
            cbc_block[i] = static_cast<unsigned char>(14U * (1U + i));
        }
    } else {
        QUtil::initializeWithRandomBytes(cbc_block, buf_size);
    }
}

void
Pl_AES_PDF::flush(bool strip_padding)
{
    if (offset != buf_size) {
        throw std::logic_error("AES pipeline: flush called when buffer was not full");
    }

    if (first) {
        first = false;
        bool return_after_init = false;
        if (cbc_mode) {
            if (encrypt) {
                // Set cbc_block to the initialization vector, and if not zero, write it to the
                // output stream.
                initializeVector();
                if (!(use_zero_iv || use_specified_iv)) {
                    next()->write(cbc_block, buf_size);
                }
            } else if (use_zero_iv || use_specified_iv) {
                // Initialize vector with zeroes; zero vector was not written to the beginning of
                // the input file.
                initializeVector();
            } else {
                // Take the first block of input as the initialization vector.  There's nothing to
                // write at this time.
                memcpy(cbc_block, inbuf, buf_size);
                offset = 0;
                return_after_init = true;
            }
        }
        crypto->rijndael_init(encrypt, key.get(), key_bytes, cbc_mode, cbc_block);
        if (return_after_init) {
            return;
        }
    }

    crypto->rijndael_process(inbuf, outbuf);
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
    offset = 0;
    next()->write(outbuf, bytes);
}
