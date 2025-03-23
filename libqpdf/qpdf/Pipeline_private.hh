#ifndef PIPELINE_PRIVATE_HH
#define PIPELINE_PRIVATE_HH

#include <qpdf/Pipeline.hh>

namespace qpdf::pl
{
    class Count final: public Pipeline
    {
      public:
        Count(char const* identifier, Pipeline* next) :
            Pipeline(identifier, next)
        {
        }

        Count(char const* identifier, std::string* str) :
            Pipeline(identifier, nullptr),
            str(str)
        {
        }

        ~Count() final = default;

        void
        write(unsigned char const* buf, size_t len) final
        {
            if (len) {
                if (str) {
                    str->append(reinterpret_cast<char const*>(buf), len);
                    return;
                }
                count += static_cast<qpdf_offset_t>(len);
                if (next()) {
                    next()->write(buf, len);
                }
            }
        }

        void
        finish() final
        {
            if (next()) {
                next()->finish();
            }
        }

        qpdf_offset_t
        getCount() const
        {
            return str ? static_cast<qpdf_offset_t>(str->size()) : count;
        }

      private:
        qpdf_offset_t count{0};
        std::string* str{nullptr};
    };
} // namespace qpdf::pl

#endif // PIPELINE_PRIVATE_HH
