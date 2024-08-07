#ifndef PL_BASE64_HH
#define PL_BASE64_HH

#include <qpdf/Pipeline.hh>

class Pl_Base64 final: public Pipeline
{
  public:
    enum action_e { a_encode, a_decode };
    Pl_Base64(char const* identifier, Pipeline* next, action_e);
    ~Pl_Base64() final = default;
    void write(unsigned char const* buf, size_t len) final;
    void finish() final;

  private:
    void decode(unsigned char const* buf, size_t len);
    void encode(unsigned char const* buf, size_t len);
    void flush();
    void flush_decode();
    void flush_encode();
    void reset();

    action_e action;
    unsigned char buf[4]{0, 0, 0, 0};
    size_t pos{0};
    bool end_of_data{false};
    bool finished{false};
};

#endif // PL_BASE64_HH
