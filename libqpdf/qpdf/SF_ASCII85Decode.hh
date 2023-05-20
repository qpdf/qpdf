#include <qpdf/Pl_ASCII85Decoder.hh>
#include <qpdf/QPDFStreamFilter.hh>
#include <memory>

#ifndef SF_ASCII85DECODE_HH
# define SF_ASCII85DECODE_HH

class SF_ASCII85Decode: public QPDFStreamFilter
{
  public:
    SF_ASCII85Decode() = default;
    virtual ~SF_ASCII85Decode() = default;

    Pipeline*
    getDecodePipeline(Pipeline* next) override
    {
        this->pipeline =
            std::make_shared<Pl_ASCII85Decoder>("ascii85 decode", next);
        return this->pipeline.get();
    }

    static std::shared_ptr<QPDFStreamFilter>
    factory()
    {
        return std::make_shared<SF_ASCII85Decode>();
    }

  private:
    std::shared_ptr<Pipeline> pipeline;
};

#endif // SF_ASCII85DECODE_HH
