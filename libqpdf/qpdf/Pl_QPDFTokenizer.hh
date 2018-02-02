#ifndef __PL_QPDFTOKENIZER_HH__
#define __PL_QPDFTOKENIZER_HH__

#include <qpdf/Pipeline.hh>

#include <qpdf/QPDFTokenizer.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/QPDFObjectHandle.hh>

//
// Treat incoming text as a stream consisting of valid PDF tokens, but
// output bad tokens just the same.  The idea here is to be able to
// use pipeline for content streams to normalize newlines without
// interfering with meaningful newlines such as those that occur
// inside of strings.
//

class Pl_QPDFTokenizer: public Pipeline
{
  public:
    Pl_QPDFTokenizer(char const* identifier,
                     QPDFObjectHandle::TokenFilter* filter);
    virtual ~Pl_QPDFTokenizer();
    virtual void write(unsigned char* buf, size_t len);
    virtual void finish();

  private:
    void processChar(char ch);
    void checkUnread();

    class Members
    {
        friend class Pl_QPDFTokenizer;

      public:
        ~Members();

      private:
        Members();
        Members(Members const&);

        QPDFObjectHandle::TokenFilter* filter;
        QPDFTokenizer tokenizer;
        bool last_char_was_cr;
        bool unread_char;
        char char_to_unread;
    };
    PointerHolder<Members> m;
};

#endif // __PL_QPDFTOKENIZER_HH__
