#ifndef PL_LZWDECODER_HH
#define PL_LZWDECODER_HH

#include <qpdf/Pipeline.hh>

#include <qpdf/Buffer.hh>
#include <vector>

class Pl_LZWDecoder final: public Pipeline
{
  public:
    Pl_LZWDecoder(char const* identifier, Pipeline* next, bool early_code_change);
    ~Pl_LZWDecoder() final = default;
    void write(unsigned char const* buf, size_t len) final;
    void finish() final;

  private:
    void sendNextCode();
    void handleCode(unsigned int code);
    unsigned char getFirstChar(unsigned int code);
    void addToTable(unsigned char next);

    // members used for converting bits to codes
    unsigned char buf[3]{0, 0, 0};
    unsigned int code_size{9};
    unsigned int next_char_{0};
    unsigned int byte_pos{0};
    unsigned int bit_pos{0}; // left to right: 01234567
    unsigned int bits_available{0};

    // members used for handle LZW decompression
    bool code_change_delta{false};
    bool eod{false};
    std::vector<Buffer> table;
    unsigned int last_code{256};
};

#endif // PL_LZWDECODER_HH
