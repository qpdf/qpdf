#include <qpdf/Pl_AES_PDF.hh>
#include <gnutls/crypto.h>
#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/rijndael-gnutls.h>

bool Pl_AES_PDF::use_static_iv = false;

Pl_AES_PDF::Pl_AES_PDF(char const* identifier, Pipeline* next,
		       bool encrypt, unsigned char const* key,
                       size_t key_bytes) :
    Pipeline(identifier, next),
    encrypt(encrypt),
    cbc_mode(true),
    first(true),
    offset(0),
    nrounds(0),
    use_zero_iv(false),
    use_specified_iv(false),
    disable_padding(false)
{
    size_t keybits = 8 * key_bytes;
    this->keylen = key_bytes;
    this->ctx = nullptr;
    assert(key_bytes == KEYLENGTH(keybits));
    this->key = PointerHolder<unsigned char>(
        true, new unsigned char[key_bytes]);
    this->rk = PointerHolder<uint32_t>(
        true, new uint32_t[RKLENGTH(keybits)]);
    size_t rk_bytes = RKLENGTH(keybits) * sizeof(uint32_t);
    std::memcpy(this->key.getPointer(), key, key_bytes);
    std::memset(this->rk.getPointer(), 0, rk_bytes);
    std::memset(this->inbuf, 0, this->buf_size);
    std::memset(this->outbuf, 0, this->buf_size);
    std::memset(this->cbc_block, 0, this->buf_size);
}

Pl_AES_PDF::~Pl_AES_PDF()
{
}

void
Pl_AES_PDF::useZeroIV()
{
    this->use_zero_iv = true;
}

void
Pl_AES_PDF::disablePadding()
{
    this->disable_padding = true;
}

void
Pl_AES_PDF::setIV(unsigned char const* iv, size_t bytes)
{
    if (bytes != this->buf_size)
    {
        throw std::logic_error(
            "Pl_AES_PDF: specified initialization vector"
            " size in bytes must be " + QUtil::uint_to_string(bytes));
    }
    this->use_specified_iv = true;
    memcpy(this->specified_iv, iv, bytes);
}

void
Pl_AES_PDF::disableCBC()
{
    this->cbc_mode = false;
}

void
Pl_AES_PDF::useStaticIV()
{
    use_static_iv = true;
}

void
Pl_AES_PDF::write(unsigned char* data, size_t len)
{
    size_t bytes_left = len;
    unsigned char* p = data;

    while (bytes_left > 0)
    {
	if (this->offset == this->buf_size)
	{
	    flush(false);
	}

	size_t available = this->buf_size - this->offset;
	size_t bytes = (bytes_left < available ? bytes_left : available);
	bytes_left -= bytes;
	std::memcpy(this->inbuf + this->offset, p, bytes);
	this->offset += bytes;
	p += bytes;
    }
}

void
Pl_AES_PDF::finish()
{
    if (this->encrypt)
    {
	if (this->offset == this->buf_size)
	{
	    flush(false);
	}
        if (! this->disable_padding)
        {
            // Pad as described in section 3.5.1 of version 1.7 of the PDF
            // specification, including providing an entire block of padding
            // if the input was a multiple of 16 bytes.
            unsigned char pad =
                QIntC::to_uchar(this->buf_size - this->offset);
            memset(this->inbuf + this->offset, pad, pad);
            this->offset = this->buf_size;
            flush(false);
        }
    }
    else
    {
	if (this->offset != this->buf_size)
	{
	    // This is never supposed to happen as the output is
	    // always supposed to be padded.  However, we have
	    // encountered files for which the output is not a
	    // multiple of the block size.  In this case, pad with
	    // zeroes and hope for the best.
	    assert(this->buf_size > this->offset);
	    std::memset(this->inbuf + this->offset, 0,
			this->buf_size - this->offset);
	    this->offset = this->buf_size;
	}
	flush(! this->disable_padding);
    }

    if (this->cbc_mode)
	rijndaelFinish(this->ctx);

    getNext()->finish();
}

void
Pl_AES_PDF::initializeVector()
{
    if (use_zero_iv)
    {
	for (unsigned int i = 0; i < this->buf_size; ++i)
	{
	    this->cbc_block[i] = 0;
	}
    }
    else if (use_specified_iv)
    {
        std::memcpy(this->cbc_block, this->specified_iv, this->buf_size);
    }
    else if (use_static_iv)
    {
	for (unsigned int i = 0; i < this->buf_size; ++i)
	{
	    this->cbc_block[i] = static_cast<unsigned char>(14U * (1U + i));
	}
    }
    else
    {
        QUtil::initializeWithRandomBytes(this->cbc_block, this->buf_size);
    }
}

void
Pl_AES_PDF::flush(bool strip_padding)
{
    assert(this->offset == this->buf_size);

    if (first)
    {
	first = false;
	if (this->cbc_mode)
	{
	    if (encrypt)
	    {
		// Set cbc_block to the initialization vector, and if
		// not zero, write it to the output stream.
		initializeVector();
                if (! (this->use_zero_iv || this->use_specified_iv))
                {
                    getNext()->write(this->cbc_block, this->buf_size);
                }
	    }
	    else if (this->use_zero_iv || this->use_specified_iv)
            {
                // Initialize vector with zeroes; zero vector was not
                // written to the beginning of the input file.
                initializeVector();
            }
            else
	    {
		// Take the first block of input as the initialization
		// vector.  There's nothing to write at this time.
		memcpy(this->cbc_block, this->inbuf, this->buf_size);
		this->offset = 0;
		rijndaelInit(&(this->ctx), this->key.getPointer(), this->keylen, this->cbc_block, this->buf_size);
		return;
	    }
	    rijndaelInit(&(this->ctx), this->key.getPointer(), this->keylen, this->cbc_block, this->buf_size);
	}
    }

    if (this->encrypt)
    {
	if (this->cbc_mode)
	    rijndaelCBCEncrypt(this->ctx, this->inbuf, this->outbuf, this->buf_size);
	else
	    rijndaelECBEncrypt(this->key.getPointer(), this->keylen, this->cbc_block, this->inbuf, this->outbuf, this->buf_size);
    }
    else
    {
	if (this->cbc_mode)
	    rijndaelCBCDecrypt(this->ctx, this->inbuf, this->outbuf, this->buf_size);
	else
	    rijndaelECBDecrypt(this->key.getPointer(), this->keylen, this->cbc_block, this->inbuf, this->outbuf, this->buf_size);
    }
    unsigned int bytes = this->buf_size;
    if (strip_padding)
    {
	unsigned char last = this->outbuf[this->buf_size - 1];
	if (last <= this->buf_size)
	{
	    bool strip = true;
	    for (unsigned int i = 1; i <= last; ++i)
	    {
		if (this->outbuf[this->buf_size - i] != last)
		{
		    strip = false;
		    break;
		}
	    }
	    if (strip)
	    {
		bytes -= last;
	    }
	}
    }
    getNext()->write(this->outbuf, bytes);
    this->offset = 0;
}
