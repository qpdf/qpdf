#ifndef CONTENTNORMALIZER_HH
#define CONTENTNORMALIZER_HH

#include <qpdf/QPDFObjectHandle.hh>

class ContentNormalizer: public QPDFObjectHandle::TokenFilter
{
  public:
    ContentNormalizer();
    virtual ~ContentNormalizer() = default;
    virtual void handleToken(QPDFTokenizer::Token const&);

    bool anyBadTokens() const;
    bool lastTokenWasBad() const;

  private:
    bool any_bad_tokens;
    bool last_token_was_bad;
};

#endif // CONTENTNORMALIZER_HH
