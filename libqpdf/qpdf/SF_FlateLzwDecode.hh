#include <qpdf/QPDFStreamFilter.hh>
#include <memory>
#include <vector>

#ifndef SF_FLATELZWDECODE_HH
# define SF_FLATELZWDECODE_HH

class SF_FlateLzwDecode: public QPDFStreamFilter
{
  public:
    SF_FlateLzwDecode(bool lzw);
    ~SF_FlateLzwDecode() override = default;

    bool setDecodeParms(QPDFObjectHandle decode_parms) override;
    Pipeline* getDecodePipeline(Pipeline* next) override;

    static std::shared_ptr<QPDFStreamFilter> flate_factory();
    static std::shared_ptr<QPDFStreamFilter> lzw_factory();

  private:
    bool lzw;
    int predictor;
    int columns;
    int colors;
    int bits_per_component;
    bool early_code_change;
    std::vector<std::shared_ptr<Pipeline>> pipelines;
};

#endif // SF_FLATELZWDECODE_HH
