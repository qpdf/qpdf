#ifndef CONTENTNORMALIZER_HH
#define CONTENTNORMALIZER_HH

#include <qpdf/QPDFObjectHandle.hh>

class ContentNormalizer final: public QPDFObjectHandle::TokenFilter
{
  public:
    ContentNormalizer();
    ~ContentNormalizer() final = default;
    void handleToken(QPDFTokenizer::Token const&) final;

    bool
    anyBadTokens() const
    {
        return any_bad_tokens;
    }
    bool
    lastTokenWasBad() const
    {
        return last_token_was_bad;
    }

  private:
    bool any_bad_tokens;
    bool last_token_was_bad;
};

#endif // CONTENTNORMALIZER_HH
