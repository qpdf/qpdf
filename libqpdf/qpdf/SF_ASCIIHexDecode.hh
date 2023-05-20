#include <qpdf/Pl_ASCIIHexDecoder.hh>
#include <qpdf/QPDFStreamFilter.hh>
#include <memory>

#ifndef SF_ASCIIHEXDECODE_HH
# define SF_ASCIIHEXDECODE_HH

class SF_ASCIIHexDecode: public QPDFStreamFilter
{
  public:
    SF_ASCIIHexDecode() = default;
    virtual ~SF_ASCIIHexDecode() = default;

    Pipeline*
    getDecodePipeline(Pipeline* next) override
    {
        this->pipeline =
            std::make_shared<Pl_ASCIIHexDecoder>("asciiHex decode", next);
        return this->pipeline.get();
    }

    static std::shared_ptr<QPDFStreamFilter>
    factory()
    {
        return std::make_shared<SF_ASCIIHexDecode>();
    }

  private:
    std::shared_ptr<Pipeline> pipeline;
};

#endif // SF_ASCIIHEXDECODE_HH
