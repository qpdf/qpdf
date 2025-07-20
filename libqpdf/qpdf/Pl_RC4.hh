#ifndef PL_RC4_HH
#define PL_RC4_HH

#include <qpdf/Pipeline.hh>

#include <qpdf/RC4.hh>

class Pl_RC4 final: public Pipeline
{
  public:
    static size_t const def_bufsize = 65536;

    // key_len of -1 means treat key_data as a null-terminated string
    Pl_RC4(
        char const* identifier, Pipeline* next, std::string key, size_t out_bufsize = def_bufsize);
    ~Pl_RC4() final = default;

    void write(unsigned char const* data, size_t len) final;
    void finish() final;

  private:
    std::shared_ptr<unsigned char> outbuf;
    size_t out_bufsize;
    RC4 rc4;
};

#endif // PL_RC4_HH
