#ifndef __CONTENTNORMALIZER_HH__
#define __CONTENTNORMALIZER_HH__

#include <qpdf/QPDFObjectHandle.hh>

class ContentNormalizer: public QPDFObjectHandle::TokenFilter
{
  public:
    ContentNormalizer();
    virtual ~ContentNormalizer();
    virtual void handleToken(QPDFTokenizer::Token const&);
    virtual void handleEOF();
};

#endif // __CONTENTNORMALIZER_HH__
