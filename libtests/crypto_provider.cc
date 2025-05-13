#include <qpdf/assert_test.h>

#include <qpdf/MD5.hh>
#include <qpdf/QPDFCryptoImpl.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <cstring>
#include <iostream>

// This is a dummy crypto implementation that only implements MD5 digest. It doesn't work, but
// it enables us to exercise the wiring of registering, querying, and setting a crypto provider.
class Potato: public QPDFCryptoImpl
{
  public:
    void
    provideRandomData(unsigned char* data, size_t len) override
    {
    }
    void
    MD5_init() override
    {
    }
    void
    MD5_update(const unsigned char* data, size_t len) override
    {
    }
    void
    MD5_finalize() override
    {
    }
    void
    MD5_digest(unsigned char* digest) override
    {
        std::memcpy(digest, "0123456789abcdef", sizeof(QPDFCryptoImpl::MD5_Digest));
    }
    void
    SHA2_init(int bits) override
    {
    }
    void
    SHA2_update(const unsigned char* data, size_t len) override
    {
    }
    void
    SHA2_finalize() override
    {
    }
    std::string
    SHA2_digest() override
    {
        return {};
    }
    void
    RC4_init(const unsigned char* key_data, int key_len) override
    {
    }
    void
    RC4_process(const unsigned char* in_data, size_t len, unsigned char* out_data) override
    {
    }
    void
    RC4_finalize() override
    {
    }
    void
    rijndael_init(
        bool encrypt,
        const unsigned char* key_data,
        size_t key_len,
        bool cbc_mode,
        unsigned char* cbc_block) override
    {
    }
    void
    rijndael_process(unsigned char* in_data, unsigned char* out_data) override
    {
    }
    void
    rijndael_finalize() override
    {
    }
};

int
main()
{
    auto initial = QPDFCryptoProvider::getDefaultProvider();
    QPDFCryptoProvider::registerImpl<Potato>("potato");
    QPDFCryptoProvider::setDefaultProvider("potato");
    assert(QPDFCryptoProvider::getDefaultProvider() == "potato");
    MD5 m;
    m.encodeString("quack"); // anything
    MD5::Digest d;
    m.digest(d);
    // hex for 0123456789abcdef
    assert(m.unparse() == "30313233343536373839616263646566");
    std::cout << "assertions passed\n";
    return 0;
}
