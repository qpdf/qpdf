#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/BufferInputSource.hh>
#include <stdexcept>
#include <string.h>

Pl_QPDFTokenizer::Members::Members() :
    filter(0),
    buf("tokenizer buffer")
{
}

Pl_QPDFTokenizer::Members::~Members()
{
}

Pl_QPDFTokenizer::Pl_QPDFTokenizer(char const* identifier,
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
}

void
Pl_QPDFTokenizer::write(unsigned char* data, size_t len)
{
    this->m->buf.write(data, len);
}

void
Pl_QPDFTokenizer::finish()
{
    this->m->buf.finish();
    PointerHolder<InputSource> input =
        new BufferInputSource("tokenizer data",
                              this->m->buf.getBuffer(), true);

    while (true)
    {
        QPDFTokenizer::Token token = this->m->tokenizer.readToken(
            input, "offset " + QUtil::int_to_string(input->tell()),
            true);
	this->m->filter->handleToken(token);
        if (token.getType() == QPDFTokenizer::tt_eof)
        {
            break;
        }
        else if ((token.getType() == QPDFTokenizer::tt_word) &&
                 (token.getValue() == "ID"))
        {
            QTC::TC("qpdf", "Pl_QPDFTokenizer found ID");
            this->m->tokenizer.expectInlineImage(input);
        }
    }
    this->m->filter->handleEOF();
    QPDFObjectHandle::TokenFilter::PipelineAccessor::setPipeline(
        m->filter, 0);
    Pipeline* next = this->getNext(true);
    if (next)
    {
        next->finish();
    }
}
