#ifndef GLOBAL_PRIVATE_HH
#define GLOBAL_PRIVATE_HH

#include <qpdf/Util.hh>
#include <qpdf/global.hh>

#include <limits>
#include <utility>

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

        static long const&
        dct_max_memory()
        {
            return l.dct_max_memory_;
        }

        static void
        dct_max_memory(long value)
        {
            l.dct_max_memory_ = util::fits<uint32_t>(value) ? value : 0;
        }

        static int const&
        dct_max_progressive_scans()
        {
            return l.dct_max_progressive_scans_;
        }

        static void
        dct_max_progressive_scans(int value)
        {
            l.dct_max_progressive_scans_ = util::fits<uint32_t>(value) ? value : 0;
        }

        static unsigned long long const&
        flate_max_memory()
        {
            return l.flate_max_memory_;
        }

        static void
        flate_max_memory(unsigned long long value)
        {
            l.flate_max_memory_ = util::fits<uint32_t>(value) ? value : 0;
        }

        static unsigned long long const&
        png_max_memory()
        {
            return l.png_max_memory_;
        }

        static void
        png_max_memory(unsigned long long value)
        {
            l.png_max_memory_ = util::fits<uint32_t>(value) ? value : 0;
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
        long dct_max_memory_{0};                 // constraint to uint32_t range in setter
        int dct_max_progressive_scans_{0};       // constraint to uint32_t range in setter
        unsigned long long flate_max_memory_{0}; // constraint to uint32_t range in setter
        unsigned long long png_max_memory_{0};   // constraint to uint32_t range in setter
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
        fuzz_mode()
        {
            return static_cast<bool>(o.fuzz_mode_);
        }

        static void fuzz_mode(bool value);

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

        static bool const&
        dct_throw_on_corrupt_data()
        {
            return o.dct_throw_on_corrupt_data_;
        }

        static void
        dct_throw_on_corrupt_data(bool value)
        {
            o.dct_throw_on_corrupt_data_ = value;
        }

      private:
        static Options o;

        bool inspection_mode_{false};
        bool default_limits_{true};
        bool fuzz_mode_{false};
        bool dct_throw_on_corrupt_data_{true};
    };
} // namespace qpdf::global

#endif // GLOBAL_PRIVATE_HH
