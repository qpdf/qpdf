#ifndef PL_BROTLI_HH
#define PL_BROTLI_HH

#include <qpdf/Pipeline.hh>

#include <array>
#include <memory>

// Internal, streaming decoder for the PDF Association Brotli extension.
namespace qpdf
{
    class Pl_Brotli: public Pipeline
    {
      public:
        Pl_Brotli(char const* identifier, Pipeline* next);
        ~Pl_Brotli() override;

        void write(unsigned char const* data, size_t len) override;
        void finish() override;

      private:
        struct State;
        std::unique_ptr<State> state_;
        std::array<unsigned char, 65536> outbuf_;
        unsigned long long written_{0};
        bool complete_{false};
    };

} // namespace qpdf

#endif // PL_BROTLI_HH
