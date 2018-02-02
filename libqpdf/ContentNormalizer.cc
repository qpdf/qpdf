#include <qpdf/ContentNormalizer.hh>
#include <qpdf/QUtil.hh>

ContentNormalizer::ContentNormalizer()
{
}

ContentNormalizer::~ContentNormalizer()
{
}

void
ContentNormalizer::handleToken(QPDFTokenizer::Token const& token)
{
    std::string value = token.getRawValue();
    QPDFTokenizer::token_type_e token_type = token.getType();

    switch (token_type)
    {
      case QPDFTokenizer::tt_space:
        {
            size_t len = value.length();
            for (size_t i = 0; i < len; ++i)
            {
                char ch = value.at(i);
                if (ch == '\r')
                {
                    if ((i + 1 < len) && (value.at(i + 1) == '\n'))
                    {
                        // ignore
                    }
                    else
                    {
                        write("\n");
                    }
                }
                else
                {
                    write(&ch, 1);
                }
            }
        }
        break;

      case QPDFTokenizer::tt_string:
        // Replacing string and name tokens in this way normalizes
        // their representation as this will automatically handle
        // quoting of unprintable characters, etc.
        writeToken(QPDFTokenizer::Token(
                       QPDFTokenizer::tt_string, token.getValue()));
	break;

      case QPDFTokenizer::tt_name:
        writeToken(QPDFTokenizer::Token(
                       QPDFTokenizer::tt_name, token.getValue()));
	break;

      default:
        writeToken(token);
	break;
    }

    value = token.getRawValue();
    if (((token_type == QPDFTokenizer::tt_string) ||
         (token_type == QPDFTokenizer::tt_name)) &&
        ((value.find('\r') != std::string::npos) ||
         (value.find('\n') != std::string::npos)))
    {
        write("\n");
    }
}

void
ContentNormalizer::handleEOF()
{
    finish();
}
