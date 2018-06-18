#include <qpdf/QPDFPageObjectHelper.hh>

QPDFPageObjectHelper::Members::~Members()
{
}

QPDFPageObjectHelper::Members::Members()
{
}

QPDFPageObjectHelper::QPDFPageObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh)
{
}

std::map<std::string, QPDFObjectHandle>
QPDFPageObjectHelper::getPageImages()
{
    return this->oh.getPageImages();
}

std::vector<QPDFObjectHandle>
QPDFPageObjectHelper::getPageContents()
{
    return this->oh.getPageContents();
}

void
QPDFPageObjectHelper::addPageContents(QPDFObjectHandle contents, bool first)
{
    this->oh.addPageContents(contents, first);
}

void
QPDFPageObjectHelper::rotatePage(int angle, bool relative)
{
    this->oh.rotatePage(angle, relative);
}

void
QPDFPageObjectHelper::coalesceContentStreams()
{
    this->oh.coalesceContentStreams();
}

void
QPDFPageObjectHelper::parsePageContents(
    QPDFObjectHandle::ParserCallbacks* callbacks)
{
    this->oh.parsePageContents(callbacks);
}

void
QPDFPageObjectHelper::filterPageContents(
    QPDFObjectHandle::TokenFilter* filter,
    Pipeline* next)
{
    this->oh.filterPageContents(filter, next);
}

void
QPDFPageObjectHelper::pipePageContents(Pipeline* p)
{
    this->oh.pipePageContents(p);
}

void
QPDFPageObjectHelper::addContentTokenFilter(
    PointerHolder<QPDFObjectHandle::TokenFilter> token_filter)
{
    this->oh.addContentTokenFilter(token_filter);
}
