#include <qpdf/QPDFStreamFilter.hh>
#include <memory>
#include <vector>

#ifndef SF_FLATELZWDECODE_HH
# define SF_FLATELZWDECODE_HH

class SF_FlateLzwDecode final: public QPDFStreamFilter
{
  public:
    enum compression_e { flate, lzw, brotli };

    SF_FlateLzwDecode(compression_e compression) :
        compression(compression)
    {
    }
    ~SF_FlateLzwDecode() final = default;

    bool setDecodeParms(QPDFObjectHandle decode_parms) final;
    Pipeline* getDecodePipeline(Pipeline* next) final;

    static std::shared_ptr<QPDFStreamFilter>
    flate_factory()
    {
        return std::make_shared<SF_FlateLzwDecode>(flate);
    }
    static std::shared_ptr<QPDFStreamFilter>
    lzw_factory()
    {
        return std::make_shared<SF_FlateLzwDecode>(lzw);
    }

    static std::shared_ptr<QPDFStreamFilter>
    brotli_factory()
    {
        return std::make_shared<SF_FlateLzwDecode>(brotli);
    }

  private:
    compression_e compression;
    // Defaults as per the PDF spec
    int predictor{1};
    int columns{1};
    int colors{1};
    int bits_per_component{8};
    bool early_code_change{true};
    std::vector<std::unique_ptr<Pipeline>> pipelines;
};

#endif // SF_FLATELZWDECODE_HH
