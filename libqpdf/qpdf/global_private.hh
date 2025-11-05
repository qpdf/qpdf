
#ifndef GLOBAL_PRIVATE_HH
#define GLOBAL_PRIVATE_HH

#include <qpdf/Constants.h>

#include <cstdint>
#include <limits>

namespace qpdf
{
    namespace global
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

            static uint32_t const&
            objects_max_errors()
            {
                return l.objects_max_errors_;
            }

            static uint32_t const&
            objects_max_container_size(bool damaged)
            {
                return damaged ? l.objects_max_container_size_damaged_
                               : l.objects_max_container_size_;
            }

          private:
            Limits() = default;
            ~Limits() = default;

            static Limits l;

            uint32_t objects_max_nesting_{499};
            uint32_t objects_max_errors_{15};
            uint32_t objects_max_container_size_{std::numeric_limits<uint32_t>::max()};
            uint32_t objects_max_container_size_damaged_{5'000};
        };

    } // namespace global

} // namespace qpdf

#endif // GLOBAL_PRIVATE_HH
