#include <qpdf/Pl_QPDFTokenizer.hh>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/QTC.hh>
#include <stdexcept>

Pl_QPDFTokenizer::Members::Members() :
    filter(nullptr),
    buf("tokenizer buffer")
{
}

Pl_QPDFTokenizer::Pl_QPDFTokenizer(
    char const* identifier,
    QPDFObjectHandle::TokenFilter* filter,
    Pipeline* next) :
    Pipeline(identifier, next),
    m(new Members)
{
    m->filter = filter;
    QPDFObjectHandle::TokenFilter::PipelineAccessor::setPipeline(
        m->filter, next);
    m->tokenizer.allowEOF();
    m->tokenizer.includeIgnorable();
}

Pl_QPDFTokenizer::~Pl_QPDFTokenizer()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
}

void
Pl_QPDFTokenizer::write(unsigned char const* data, size_t len)
{
    this->m->buf.write(data, len);
}

void
Pl_QPDFTokenizer::finish()
{
    this->m->buf.finish();
    auto input = std::shared_ptr<InputSource>(
        // line-break
        new BufferInputSource(
            "tokenizer data", this->m->buf.getBuffer(), true));

    while (true) {
        QPDFTokenizer::Token token = this->m->tokenizer.readToken(
            input, "offset " + std::to_string(input->tell()), true);
        this->m->filter->handleToken(token);
        if (token.getType() == QPDFTokenizer::tt_eof) {
            break;
        } else if (token.isWord("ID")) {
            // Read the space after the ID.
            char ch = ' ';
            input->read(&ch, 1);
            this->m->filter->handleToken(
                // line-break
                QPDFTokenizer::Token(
                    QPDFTokenizer::tt_space, std::string(1, ch)));
            QTC::TC("qpdf", "Pl_QPDFTokenizer found ID");
            this->m->tokenizer.expectInlineImage(input);
        }
    }
    this->m->filter->handleEOF();
    QPDFObjectHandle::TokenFilter::PipelineAccessor::setPipeline(
        m->filter, nullptr);
    Pipeline* next = this->getNext(true);
    if (next) {
        next->finish();
    }
}
