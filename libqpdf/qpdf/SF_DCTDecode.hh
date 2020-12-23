#include <qpdf/QPDFStreamFilter.hh>
#include <qpdf/Pl_DCT.hh>
#include <memory>

#ifndef SF_DCTDECODE_HH
#define SF_DCTDECODE_HH

class SF_DCTDecode: public QPDFStreamFilter
{
  public:
    SF_DCTDecode() = default;
    virtual ~SF_DCTDecode() = default;

    virtual Pipeline* getDecodePipeline(Pipeline* next) override
    {
        this->pipeline = std::make_shared<Pl_DCT>("DCT decode", next);
        return this->pipeline.get();
    }

    static std::shared_ptr<QPDFStreamFilter> factory()
    {
        return std::make_shared<SF_DCTDecode>();
    }

    virtual bool isSpecializedCompression() override
    {
        return true;
    }

    virtual bool isLossyCompression() override
    {
        return true;
    }

  private:
    std::shared_ptr<Pipeline> pipeline;
};

#endif // SF_DCTDECODE_HH
