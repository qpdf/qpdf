#ifndef __PL_TIFFPREDICTOR_HH__
#define __PL_TIFFPREDICTOR_HH__

// This pipeline reverses the application of a TIFF predictor as
// described in the TIFF specification.

#include <qpdf/Pipeline.hh>

class Pl_TIFFPredictor: public Pipeline
{
  public:
    enum action_e { a_encode, a_decode };

    QPDF_DLL
    Pl_TIFFPredictor(char const* identifier, Pipeline* next,
                     action_e action, unsigned int columns,
                     unsigned int samples_per_pixel = 1,
                     unsigned int bits_per_sample = 8);
    QPDF_DLL
    virtual ~Pl_TIFFPredictor();

    QPDF_DLL
    virtual void write(unsigned char* data, size_t len);
    QPDF_DLL
    virtual void finish();

  private:
    void processRow();

    action_e action;
    unsigned int columns;
    unsigned int bytes_per_row;
    unsigned int samples_per_pixel;
    unsigned int bits_per_sample;
    unsigned char* cur_row;
    size_t pos;
};

#endif // __PL_TIFFPREDICTOR_HH__
