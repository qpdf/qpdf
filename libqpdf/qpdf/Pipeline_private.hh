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

        ~Count() final = default;

        void
        write(unsigned char const* buf, size_t len) final
        {
            if (len) {
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
            return count;
        }

      private:
        qpdf_offset_t count{0};
    };
} // namespace qpdf::pl

#endif // PIPELINE_PRIVATE_HH
