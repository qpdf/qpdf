#ifndef GLOBAL_PRIVATE_HH
#define GLOBAL_PRIVATE_HH

#include <qpdf/global.hh>

#include <limits>

namespace qpdf::global
{
    class Limits
    {
      public:
        Limits(Limits const&) = delete;
        Limits(Limits&&) = delete;
        Limits& operator=(Limits const&) = delete;
        Limits& operator=(Limits&&) = delete;

        static uint32_t const&
        parser_max_nesting()
        {
            return l.parser_max_nesting_;
        }

        static void
        parser_max_nesting(uint32_t value)
        {
            l.parser_max_nesting_ = value;
        }

        static uint32_t const&
        parser_max_errors()
        {
            return l.parser_max_errors_;
        }

        static void
        parser_max_errors(uint32_t value)
        {
            l.parser_max_errors_set_ = true;
            l.parser_max_errors_ = value;
        }

        static uint32_t const&
        parser_max_container_size(bool damaged)
        {
            return damaged ? l.parser_max_container_size_damaged_ : l.parser_max_container_size_;
        }

        static void parser_max_container_size(bool damaged, uint32_t value);

        static uint32_t const&
        max_stream_filters()
        {
            return l.max_stream_filters_;
        }

        static void
        max_stream_filters(uint32_t value)
        {
            l.max_stream_filters_set_ = true;
            l.max_stream_filters_ = value;
        }

        /// Record a limit error.
        static void
        error()
        {
            if (l.errors_ < std::numeric_limits<uint32_t>::max()) {
                ++l.errors_;
            }
        }

        static uint32_t const&
        errors()
        {
            return l.errors_;
        }

        static void disable_defaults();

      private:
        Limits() = default;
        ~Limits() = default;

        static Limits l;

        uint32_t errors_{0};

        uint32_t parser_max_nesting_{499};
        uint32_t parser_max_errors_{15};
        bool parser_max_errors_set_{false};
        uint32_t parser_max_container_size_{std::numeric_limits<uint32_t>::max()};
        uint32_t parser_max_container_size_damaged_{5'000};
        bool parser_max_container_size_damaged_set_{false};
        uint32_t max_stream_filters_{25};
        bool max_stream_filters_set_{false};
    };

    class Options
    {
      public:
        static bool
        inspection_mode()
        {
            return static_cast<bool>(o.inspection_mode_);
        }

        static void
        inspection_mode(bool value)
        {
            if (value) {
                o.inspection_mode_ = true;
            }
        }

        static bool
        default_limits()
        {
            return static_cast<bool>(o.default_limits_);
        }

        static void
        default_limits(bool value)
        {
            if (!value) {
                o.default_limits_ = false;
                Limits::disable_defaults();
            }
        }

      private:
        static Options o;

        bool inspection_mode_{false};
        bool default_limits_{true};
    };
} // namespace qpdf::global

#endif // GLOBAL_PRIVATE_HH
