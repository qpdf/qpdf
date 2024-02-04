#ifndef PL_TIFFPREDICTOR_HH
#define PL_TIFFPREDICTOR_HH

// This pipeline reverses the application of a TIFF predictor as described in the TIFF
// specification.

#include <qpdf/Pipeline.hh>

#include <vector>

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
    std::vector<unsigned char> cur_row;
    std::vector<long long> previous;
    std::vector<unsigned char> out;
    Pipeline* p_next;
};

#endif // PL_TIFFPREDICTOR_HH
