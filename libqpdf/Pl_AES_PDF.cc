#include <qpdf/Pl_AES_PDF.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QUtil.hh>
#include <cstring>
#include <stdexcept>
#include <string>

bool Pl_AES_PDF::use_static_iv = false;

Pl_AES_PDF::Pl_AES_PDF(
    char const* identifier, Pipeline* next, bool encrypt, std::string const& key) :
    Pipeline(identifier, next),
    key(key),
    crypto(QPDFCryptoProvider::getImpl()),
    encrypt_(encrypt)
{
    if (!(key.size() == 32 || key.size() == 16)) {
        throw std::runtime_error("unsupported key length");
    }
    inbuf.fill(0);
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
Pl_AES_PDF::setIV(std::string const& iv)
{
    if (iv.size() != buf_size) {
        throw std::logic_error(
            "Pl_AES_PDF: specified initialization vector size in bytes must be " +
            std::to_string(buf_size));
    }
    use_specified_iv = true;
    specified_iv = iv;
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

    if (encrypt_) {
        write_encrypt({reinterpret_cast<const char*>(data), len});
    } else {
        write_decrypt({reinterpret_cast<const char*>(data), len});
    }
}

std::string
Pl_AES_PDF::encrypt(
    std::string const& key, std::string const& data, bool aes_v3, std::string const& iv)
{
    Pl_AES_PDF aes("", nullptr, true, key);
    if (iv.empty()) {
        if (aes_v3) {
            aes.useZeroIV();
        }
    } else {
        aes.setIV(iv);
    }
    if (aes_v3) {
        aes.disablePadding();
    }
    aes.write_encrypt(data);
    return std::move(aes.out);
}

std::string
Pl_AES_PDF::decrypt(std::string const& key, std::string const& data, bool aes_v3)
{
    Pl_AES_PDF aes("", nullptr, false, key);
    if (aes_v3) {
        aes.useZeroIV();
    }
    aes.write_decrypt(data);
    return std::move(aes.out);
}

void
Pl_AES_PDF::write_decrypt(std::string_view sv)
{
    char const* p = sv.data();
    size_t reps = sv.size() < buf_size ? 0 : (sv.size() - 1) / buf_size;
    size_t bytes_left = sv.size() - (buf_size * reps);

    out.reserve((reps + 2) * buf_size);

    if (cbc_mode) {
        if (use_zero_iv || use_specified_iv) {
            // Initialize vector with zeroes; zero vector was not written to the beginning of
            // the input file.
            initializeVector();
        } else {
            // Take the first block of input as the initialization vector. Don't write it to output.
            if (sv.size() < buf_size) {
                // This should never happen. In any case, there is no data to decrypt.
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
    crypto->rijndael_init(false, key, cbc_mode, cbc_block.data());

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
Pl_AES_PDF::flush_decrypt(const char* in, bool strip_padding)
{
    crypto->rijndael_process(in, outbuf.data());
    unsigned int bytes = buf_size;
    if (strip_padding) {
        unsigned char last = static_cast<unsigned char>(outbuf.back());
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
    out.append(outbuf.data(), bytes);
}

void
Pl_AES_PDF::write_encrypt(std::string_view sv)
{
    char const* p = sv.data();
    size_t reps = sv.size() < buf_size ? 0 : (sv.size() - 1) / buf_size;
    out.reserve((reps + 4) * buf_size);

    if (cbc_mode) {
        // Set cbc_block to the initialization vector, and if not zero, write it to the
        // output stream.
        initializeVector();
        if (!(use_zero_iv || use_specified_iv)) {
            out.append(cbc_block.data(), buf_size);
        }
    }
    crypto->rijndael_init(true, key, cbc_mode, cbc_block.data());

    for (size_t i = 0; i < reps; ++i) {
        flush_encrypt(p);
        p += buf_size;
    }

    size_t bytes_left = sv.size() - (buf_size * reps);
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
Pl_AES_PDF::flush_encrypt(const char* in)
{
    crypto->rijndael_process(in, outbuf.data());
    out.append(outbuf.data(), buf_size);
}

void
Pl_AES_PDF::finish()
{
    if (next()) {
        next()->write(reinterpret_cast<unsigned char const*>(out.data()), out.size());
        next()->finish();
    }
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
            cbc_block[i] = static_cast<char>(14U * (1U + i));
        }
    } else {
        QUtil::initializeWithRandomBytes(
            reinterpret_cast<unsigned char*>(cbc_block.data()), buf_size);
    }
}
