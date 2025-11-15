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
        objects_max_nesting()
        {
            return l.objects_max_nesting_;
        }

        static void
        objects_max_nesting(uint32_t value)
        {
            l.objects_max_nesting_ = value;
        }

        static uint32_t const&
        objects_max_errors()
        {
            return l.objects_max_errors_;
        }

        static void
        objects_max_errors(uint32_t value)
        {
            l.objects_max_errors_set_ = true;
            l.objects_max_errors_ = value;
        }

        static uint32_t const&
        objects_max_container_size(bool damaged)
        {
            return damaged ? l.objects_max_container_size_damaged_ : l.objects_max_container_size_;
        }

        static void objects_max_container_size(bool damaged, uint32_t value);

        static void disable_defaults();

      private:
        Limits() = default;
        ~Limits() = default;

        static Limits l;

        uint32_t objects_max_nesting_{499};
        uint32_t objects_max_errors_{15};
        bool objects_max_errors_set_{false};
        uint32_t objects_max_container_size_{std::numeric_limits<uint32_t>::max()};
        uint32_t objects_max_container_size_damaged_{5'000};
        bool objects_max_container_size_damaged_set_{false};
    };

    class Options
    {
      public:
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

        bool default_limits_{true};
    };
} // namespace qpdf::global

#endif // GLOBAL_PRIVATE_HH
