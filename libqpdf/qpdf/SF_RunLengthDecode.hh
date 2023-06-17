#include <qpdf/Pl_RunLength.hh>
#include <qpdf/QPDFStreamFilter.hh>
#include <memory>

#ifndef SF_RUNLENGTHDECODE_HH
# define SF_RUNLENGTHDECODE_HH

class SF_RunLengthDecode: public QPDFStreamFilter
{
  public:
    SF_RunLengthDecode() = default;
    ~SF_RunLengthDecode() override = default;

    Pipeline*
    getDecodePipeline(Pipeline* next) override
    {
        this->pipeline =
            std::make_shared<Pl_RunLength>("runlength decode", next, Pl_RunLength::a_decode);
        return this->pipeline.get();
    }

    static std::shared_ptr<QPDFStreamFilter>
    factory()
    {
        return std::make_shared<SF_RunLengthDecode>();
    }

    bool
    isSpecializedCompression() override
    {
        return true;
    }

  private:
    std::shared_ptr<Pipeline> pipeline;
};

#endif // SF_RUNLENGTHDECODE_HH
