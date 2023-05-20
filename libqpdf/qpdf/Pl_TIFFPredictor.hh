#ifndef PL_TIFFPREDICTOR_HH
#define PL_TIFFPREDICTOR_HH

// This pipeline reverses the application of a TIFF predictor as
// described in the TIFF specification.

#include <qpdf/Pipeline.hh>

class Pl_TIFFPredictor: public Pipeline
{
  public:
    enum action_e { a_encode, a_decode };

    Pl_TIFFPredictor(
        char const* identifier,
        Pipeline* next,
        action_e action,
        unsigned int columns,
        unsigned int samples_per_pixel = 1,
        unsigned int bits_per_sample = 8);
    ~Pl_TIFFPredictor() override = default;

    void write(unsigned char const* data, size_t len) override;
    void finish() override;

  private:
    void processRow();

    action_e action;
    unsigned int columns;
    unsigned int bytes_per_row;
    unsigned int samples_per_pixel;
    unsigned int bits_per_sample;
    std::shared_ptr<unsigned char> cur_row;
    size_t pos;
};

#endif // PL_TIFFPREDICTOR_HH
