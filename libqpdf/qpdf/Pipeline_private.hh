#ifndef PIPELINE_PRIVATE_HH
#define PIPELINE_PRIVATE_HH

#include <qpdf/Pipeline.hh>

#include <qpdf/Pl_Flate.hh>
#include <qpdf/Types.h>

namespace qpdf::pl
{
    class String final: public Pipeline
    {
      public:
        String(char const* identifier, Pipeline*, std::string& str) :
            Pipeline(identifier, nullptr),
            str(str)
        {
        }

        String(std::string& str) :
            Pipeline("", nullptr),
            str(str)
        {
        }

        ~String() final = default;

        void
        write(unsigned char const* buf, size_t len) final
        {
            if (len) {
                str.append(reinterpret_cast<char const*>(buf), len);
            }
        }

        void
        finish() final
        {
        }

      private:
        std::string& str;
    };

    class Count final: public Pipeline
    {
      public:
        // Count the number of characters written. If 'next' is not set, the content written will be
        // discarded.
        Count(unsigned long id, Pipeline* next = nullptr) :
            Pipeline("", next),
            id_(id),
            pass_immediately_to_next(next)
        {
        }

        // Write to 'str'. If 'next' is set, 'str' will be written to 'next' when 'finish' is
        // called.
        Count(unsigned long id, std::string& str, std::unique_ptr<Pipeline> link = nullptr) :
            Pipeline("", link.get()),
            str(&str),
            link(std::move(link)),
            id_(id)
        {
        }

        ~Count() final = default;

        void
        write(unsigned char const* buf, size_t len) final
        {
            if (len) {
                Count::write(std::string_view(reinterpret_cast<char const*>(buf), len));
            }
        }

        void
        write(std::string_view sv)
        {
            if (sv.size()) {
                if (str) {
                    str->append(sv);
                    return;
                }
                count += static_cast<qpdf_offset_t>(sv.size());
                if (pass_immediately_to_next) {
                    next()->write(reinterpret_cast<char const*>(sv.data()), sv.size());
                }
            }
        }

        void
        write(size_t len, char c)
        {
            if (len) {
                if (str) {
                    str->append(len, c);
                    return;
                }
                count += static_cast<qpdf_offset_t>(len);
                if (pass_immediately_to_next) {
                    next()->writeString(std::string(len, c));
                }
            }
        }

        void
        finish() final
        {
            if (next()) {
                if (!pass_immediately_to_next) {
                    next()->write(reinterpret_cast<unsigned char const*>(str->data()), str->size());
                }
                next()->finish();
            }
        }

        qpdf_offset_t
        getCount() const
        {
            return str ? static_cast<qpdf_offset_t>(str->size()) : count;
        }

        unsigned long long
        id() const
        {
            return id_;
        }

      private:
        qpdf_offset_t count{0};
        std::string* str{nullptr};
        std::unique_ptr<Pipeline> link{nullptr};
        unsigned long id_{0};
        bool pass_immediately_to_next{false};
    };

    template <typename P, typename... Args>
    std::string
    pipe(std::string_view data, Args&&... args)
    {
        std::string result;
        String s("", nullptr, result);
        P pl("", &s, std::forward<Args>(args)...);
        pl.write(reinterpret_cast<unsigned char const*>(data.data()), data.size());
        pl.finish();
        return result;
    }
} // namespace qpdf::pl

#endif // PIPELINE_PRIVATE_HH
