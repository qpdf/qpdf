#include <qpdf/Pl_RC4.hh>

#include <qpdf/QUtil.hh>

Pl_RC4::Pl_RC4(char const* identifier, Pipeline* next,
	       unsigned char const* key_data, int key_len,
	       size_t out_bufsize) :
    Pipeline(identifier, next),
    out_bufsize(out_bufsize),
    rc4(key_data, key_len)
{
    this->outbuf = make_array_pointer_holder<unsigned char>(out_bufsize);
}

Pl_RC4::~Pl_RC4()
{
}

void
Pl_RC4::write(unsigned char* data, size_t len)
{
    if (this->outbuf.get() == 0)
    {
	throw std::logic_error(
	    this->identifier +
	    ": Pl_RC4: write() called after finish() called");
    }

    size_t bytes_left = len;
    unsigned char* p = data;

    while (bytes_left > 0)
    {
	size_t bytes =
            (bytes_left < this->out_bufsize ? bytes_left : out_bufsize);
	bytes_left -= bytes;
        // lgtm[cpp/weak-cryptographic-algorithm]
	rc4.process(p, bytes, outbuf.get());
	p += bytes;
	getNext()->write(outbuf.get(), bytes);
    }
}

void
Pl_RC4::finish()
{
    this->outbuf = 0;
    this->getNext()->finish();
}
