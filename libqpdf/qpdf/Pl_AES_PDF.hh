#ifndef PL_AES_PDF_HH
#define PL_AES_PDF_HH

#include <qpdf/Pipeline.hh>
#include <qpdf/QPDFCryptoImpl.hh>

#include <array>
#include <memory>

// This pipeline implements AES-128 and AES-256 with CBC and block padding as specified in the PDF
// specification.
class Pl_AES_PDF final: public Pipeline
{
  public:
    // key should be a pointer to key_bytes bytes of data
    Pl_AES_PDF(char const* identifier, Pipeline* next, bool encrypt, std::string const& key);
    ~Pl_AES_PDF() final = default;

    static std::string
    decrypt(std::string const& key, std::string const& data, bool aes_v3 = false);

    static std::string encrypt(
        std::string const& key,
        std::string const& data,
        bool aes_v3 = false,
        std::string const& iv = "");

    // Pl_AES_PDF only supports calling write a single time.
    void write(unsigned char const* data, size_t len) final;
    void finish() final;

    // Use zero initialization vector; needed for AESV3
    void useZeroIV();
    // Disable padding; needed for AESV3
    void disablePadding();
    // Specify an initialization vector, which will not be included in
    // the output.
    void setIV(std::string const& iv);

    // For testing only; PDF always uses CBC
    void disableCBC();
    // For testing only: use a fixed initialization vector for CBC
    static void useStaticIV();

  private:
    void write_decrypt(std::string_view sv);
    void write_encrypt(std::string_view sv);
    void flush_decrypt(const char* in, bool discard_padding);
    void flush_encrypt(const char* in);
    void initializeVector();

    static unsigned int const buf_size = QPDFCryptoImpl::rijndael_buf_size;
    static bool use_static_iv;

    std::string key;
    std::shared_ptr<QPDFCryptoImpl> crypto;
    bool encrypt_;
    bool cbc_mode{true};
    bool first{true};
    std::array<char, buf_size> inbuf;
    std::array<char, buf_size> outbuf;
    std::string out;
    std::array<char, buf_size> cbc_block;
    std::string specified_iv;
    bool use_zero_iv{false};
    bool use_specified_iv{false};
    bool disable_padding{false};
};

#endif // PL_AES_PDF_HH
