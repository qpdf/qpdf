#ifndef PIPELINE_PRIVATE_HH
#define PIPELINE_PRIVATE_HH

#include <qpdf/Pipeline.hh>

#include <qpdf/Pl_Flate.hh>

namespace qpdf::pl
{
    struct Link
    {
        Link(std::unique_ptr<Link> next_link, std::unique_ptr<Pipeline> next_pl) :
            next_link(std::move(next_link)),
            next_pl(std::move(next_pl))
        {
        }

        std::unique_ptr<Link> next_link{nullptr};
        std::unique_ptr<Pipeline> next_pl{nullptr};
    };

    template <typename P, typename... Args>
    std::unique_ptr<Link>
    create(Args&&... args)
    {
        return std::make_unique<Link>(
            nullptr, std::make_unique<P>("", nullptr, std::forward<Args>(args)...));
    }

    template <typename P, typename... Args>
    std::unique_ptr<Link>
    create(std::unique_ptr<Link> link, Args&&... args)
    {
        auto* next = link->next_pl.get();
        return std::make_unique<Link>(
            std::move(link), std::make_unique<P>("", next, std::forward<Args>(args)...));
    }

    class String final: public Pipeline
    {
      public:
        String(char const* identifier, Pipeline*, std::string& str) :
            Pipeline(identifier, nullptr),
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
        Count(char const* identifier, Pipeline* next = nullptr) :
            Pipeline(identifier, next),
            pass_immediately_to_next(next)
        {
        }

        // Count the number of characters written. If 'next' is not set, the content written will be
        // discarded.
        Count(char const* identifier, std::unique_ptr<Link> link = nullptr) :
            Pipeline(identifier, link ? link->next_pl.get() : nullptr),
            link(std::move(link)),
            pass_immediately_to_next(link)
        {
        }

        // Write to 'str'. If 'next' is set, 'str' will be written to 'next' when 'finish' is
        // called.
        Count(char const* identifier, std::string& str, std::unique_ptr<Link> link = nullptr) :
            Pipeline(identifier, link ? link->next_pl.get() : nullptr),
            str(&str),
            link(std::move(link))
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
                if (pass_immediately_to_next) {
                    next()->write(buf, len);
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

      private:
        qpdf_offset_t count{0};
        std::string* str{nullptr};
        std::unique_ptr<Link> link{nullptr};
        bool pass_immediately_to_next{false};
    };
} // namespace qpdf::pl

#endif // PIPELINE_PRIVATE_HH
