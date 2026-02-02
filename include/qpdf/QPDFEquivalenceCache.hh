#ifndef QPDFEQUIVALENCECACHE_HH
#define QPDFEQUIVALENCECACHE_HH

#include <unordered_map>
#include <utility>

class QPDFObject;

namespace qpdf
{
    /**
     * @brief Memoization cache for is_equivalent.
     * @warning Valid only while QPDFObject lifetimes are stable.
     * Risk of ABA collision if objects are freed and reallocated.
     */
    struct EquivalenceCache
    {
        struct PointerPairHash
        {
            size_t
            operator()(const std::pair<QPDFObject*, QPDFObject*>& p) const
            {
                // Robust hash combiner (0x9e3779b9) to prevent collisions
                size_t h1 = std::hash<void*>{}(p.first);
                size_t h2 = std::hash<void*>{}(p.second);
                return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            }
        };

        // maps {objA_ptr, objB_ptr} -> are_equivalent
        std::unordered_map<std::pair<QPDFObject*, QPDFObject*>, bool, PointerPairHash> table;
        size_t insertions = 0;
    };
} // namespace qpdf

#endif // QPDFEQUIVALENCECACHE_HH
