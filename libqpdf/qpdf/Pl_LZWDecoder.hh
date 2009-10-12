#ifndef __PL_LZWDECODER_HH__
#define __PL_LZWDECODER_HH__

#include <qpdf/Pipeline.hh>

#include <qpdf/Buffer.hh>
#include <vector>

class DLL_EXPORT Pl_LZWDecoder: public Pipeline
{
  public:
    Pl_LZWDecoder(char const* identifier, Pipeline* next,
		  bool early_code_change);
    virtual ~Pl_LZWDecoder();
    virtual void write(unsigned char* buf, int len);
    virtual void finish();

  private:
    void sendNextCode();
    void handleCode(int code);
    unsigned char getFirstChar(int code);
    void addToTable(unsigned char next);

    // members used for converting bits to codes
    unsigned char buf[3];
    int code_size;
    int next;
    int byte_pos;
    int bit_pos;		// left to right: 01234567
    int bits_available;

    // members used for handle LZW decompression
    bool code_change_delta;
    bool eod;
    std::vector<Buffer> table;
    int last_code;
};

#endif // __PL_LZWDECODER_HH__
