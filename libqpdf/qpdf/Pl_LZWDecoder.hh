#ifndef __PL_LZWDECODER_HH__
#define __PL_LZWDECODER_HH__

#include <qpdf/Pipeline.hh>

#include <qpdf/Buffer.hh>
#include <vector>

class Pl_LZWDecoder: public Pipeline
{
  public:
    QPDF_DLL
    Pl_LZWDecoder(char const* identifier, Pipeline* next,
		  bool early_code_change);
    QPDF_DLL
    virtual ~Pl_LZWDecoder();
    QPDF_DLL
    virtual void write(unsigned char* buf, size_t len);
    QPDF_DLL
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
