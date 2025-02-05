#include <qpdf/QPDFStreamFilter.hh>
#include <memory>
#include <vector>

#ifndef SF_FLATELZWDECODE_HH
# define SF_FLATELZWDECODE_HH

class SF_FlateLzwDecode final: public QPDFStreamFilter
{
  public:
    SF_FlateLzwDecode(bool lzw) :
        lzw(lzw)
    {
    }
    ~SF_FlateLzwDecode() final = default;

    bool setDecodeParms(QPDFObjectHandle decode_parms) final;
    Pipeline* getDecodePipeline(Pipeline* next) final;

    static std::shared_ptr<QPDFStreamFilter> flate_factory();
    static std::shared_ptr<QPDFStreamFilter> lzw_factory();

  private:
    bool lzw{};
    // Defaults as per the PDF spec
    int predictor{1};
    int columns{1};
    int colors{1};
    int bits_per_component{8};
    bool early_code_change{true};
    std::vector<std::shared_ptr<Pipeline>> pipelines;
};

#endif // SF_FLATELZWDECODE_HH
