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

    bool anyBadTokens() const;
    bool lastTokenWasBad() const;

  private:
    bool any_bad_tokens;
    bool last_token_was_bad;
};

#endif // __CONTENTNORMALIZER_HH__
