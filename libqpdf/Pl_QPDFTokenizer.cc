#include <qpdf/Pl_QPDFTokenizer.hh>

#include <qpdf/InputSource_private.hh>
#include <qpdf/Pipeline_private.hh>
#include <qpdf/QTC.hh>

#include <stdexcept>

using namespace qpdf;

class Pl_QPDFTokenizer::Members
{
  public:
    Members() = default;
    Members(Members const&) = delete;
    ~Members() = default;

    QPDFObjectHandle::TokenFilter* filter{nullptr};
    QPDFTokenizer tokenizer;
    std::string buffer;
    pl::String buf{"pl_tokenizer", nullptr, buffer};
};

Pl_QPDFTokenizer::Pl_QPDFTokenizer(
    char const* identifier, QPDFObjectHandle::TokenFilter* filter, Pipeline* next) :
    Pipeline(identifier, next),
    m(std::make_unique<Members>())
{
    m->filter = filter;
    QPDFObjectHandle::TokenFilter::PipelineAccessor::setPipeline(m->filter, next);
    m->tokenizer.allowEOF();
    m->tokenizer.includeIgnorable();
}

// Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
Pl_QPDFTokenizer::~Pl_QPDFTokenizer() = default;

void
Pl_QPDFTokenizer::write(unsigned char const* data, size_t len)
{
    m->buf.write(data, len);
}

void
Pl_QPDFTokenizer::finish()
{
    auto input = is::OffsetBuffer("tokenizer data", m->buffer);
    std::string empty;
    while (true) {
        auto token = m->tokenizer.readToken(input, empty, true);
        m->filter->handleToken(token);
        if (token.getType() == QPDFTokenizer::tt_eof) {
            break;
        } else if (token.isWord("ID")) {
            // Read the space after the ID.
            char ch = ' ';
            input.read(&ch, 1);
            m->filter->handleToken(
                // line-break
                QPDFTokenizer::Token(QPDFTokenizer::tt_space, std::string(1, ch)));
            QTC::TC("qpdf", "Pl_QPDFTokenizer found ID");
            m->tokenizer.expectInlineImage(input);
        }
    }
    m->filter->handleEOF();
    QPDFObjectHandle::TokenFilter::PipelineAccessor::setPipeline(m->filter, nullptr);
    if (next()) {
        next()->finish();
    }
}
