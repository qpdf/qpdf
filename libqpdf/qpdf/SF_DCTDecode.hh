#include <qpdf/Pl_DCT.hh>
#include <qpdf/QPDFStreamFilter.hh>
#include <memory>

#ifndef SF_DCTDECODE_HH
# define SF_DCTDECODE_HH

class SF_DCTDecode: public QPDFStreamFilter
{
  public:
    SF_DCTDecode() = default;
    ~SF_DCTDecode() override = default;

    Pipeline*
    getDecodePipeline(Pipeline* next) override
    {
        this->pipeline = std::make_shared<Pl_DCT>("DCT decode", next);
        return this->pipeline.get();
    }

    static std::shared_ptr<QPDFStreamFilter>
    factory()
    {
        return std::make_shared<SF_DCTDecode>();
    }

    bool
    isSpecializedCompression() override
    {
        return true;
    }

    bool
    isLossyCompression() override
    {
        return true;
    }

  private:
    std::shared_ptr<Pipeline> pipeline;
};

#endif // SF_DCTDECODE_HH
