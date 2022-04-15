#ifndef PL_LZWDECODER_HH
#define PL_LZWDECODER_HH

#include <qpdf/Pipeline.hh>

#include <qpdf/Buffer.hh>
#include <vector>

class Pl_LZWDecoder: public Pipeline
{
  public:
    Pl_LZWDecoder(
        char const* identifier, Pipeline* next, bool early_code_change);
    virtual ~Pl_LZWDecoder() = default;
    virtual void write(unsigned char* buf, size_t len);
    virtual void finish();

  private:
    void sendNextCode();
    void handleCode(unsigned int code);
    unsigned char getFirstChar(unsigned int code);
    void addToTable(unsigned char next);

    // members used for converting bits to codes
    unsigned char buf[3];
    unsigned int code_size;
    unsigned int next;
    unsigned int byte_pos;
    unsigned int bit_pos; // left to right: 01234567
    unsigned int bits_available;

    // members used for handle LZW decompression
    bool code_change_delta;
    bool eod;
    std::vector<Buffer> table;
    unsigned int last_code;
};

#endif // PL_LZWDECODER_HH
