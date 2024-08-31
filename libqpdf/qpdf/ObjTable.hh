#ifndef OBJTABLE_HH
#define OBJTABLE_HH

#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>

#include "qpdf/QIntC.hh"
#include <limits>

// A table of objects indexed by object id. This is intended as a more efficient replacement for
// std::map<QPDFObjGen, T> containers.
//
// The table is implemented as a std::vector, with the object id implicitly represented by the index
// of the object. This has a number of implications, including:
// - operations that change the index of existing elements such as insertion and deletions are not
//   permitted.
// - operations that extend the table may invalidate iterators and references to objects.
//
// The provided overloads of the access operator[] are safe. For out of bounds access they will
// either extend the table or throw a runtime error.
//
// ObjTable has a map 'sparse_elements' to deal with very sparse / extremely large object tables
// (usually as the result of invalid dangling references). This map may contain objects not found in
// the xref table of the original pdf if there are dangling references with an id significantly
// larger than the largest valid object id found in original pdf.

template <class T>
class ObjTable: public std::vector<T>
{
  public:
    ObjTable() = default;
    ObjTable(const ObjTable&) = delete;
    ObjTable(ObjTable&&) = delete;
    ObjTable& operator[](const ObjTable&) = delete;
    ObjTable& operator[](ObjTable&&) = delete;

    // Remove unchecked access.
    T& operator[](unsigned long idx) = delete;
    T const& operator[](unsigned long idx) const = delete;

    inline T const&
    operator[](int idx) const
    {
        return element(static_cast<size_t>(idx));
    }

    inline T const&
    operator[](QPDFObjGen og) const
    {
        return element(static_cast<size_t>(og.getObj()));
    }

    inline T const&
    operator[](QPDFObjectHandle oh) const
    {
        return element(static_cast<size_t>(oh.getObjectID()));
    }

    inline bool
    contains(size_t idx) const
    {
        return idx < std::vector<T>::size() || sparse_elements.count(idx);
    }

    inline bool
    contains(QPDFObjGen og) const
    {
        return contains(static_cast<size_t>(og.getObj()));
    }

    inline bool
    contains(QPDFObjectHandle oh) const
    {
        return contains(static_cast<size_t>(oh.getObjectID()));
    }

  protected:
    inline T&
    operator[](int id)
    {
        return element(static_cast<size_t>(id));
    }

    inline T&
    operator[](QPDFObjGen og)
    {
        return element(static_cast<size_t>(og.getObj()));
    }

    inline T&
    operator[](QPDFObjectHandle oh)
    {
        return element(static_cast<size_t>(oh.getObjectID()));
    }

    inline T&
    operator[](unsigned int id)
    {
        return element(id);
    }

    void
    initialize(size_t idx)
    {
        if (std::vector<T>::size() > 0 || sparse_elements.size() > 0) {
            throw ::std::logic_error("ObjTable accessed before initialization");
        } else if (
            idx >= static_cast<size_t>(std::numeric_limits<int>::max()) ||
            idx >= std::vector<T>::max_size()) {
            throw std::runtime_error("Invalid maximum object id initializing ObjTable.");
        } else {
            std::vector<T>::resize(++idx);
        }
    }


    inline void
    forEach(std::function<void(int, const T&)> fn)
    {
        int i = 0;
        for (auto const& item: *this) {
            fn(i++, item);
        }
        for (auto const& [id, item]: sparse_elements) {
            fn(QIntC::to_int(id), item);
        }
    }

  private:
    std::map<size_t, T> sparse_elements;

    inline T&
    element(size_t idx)
    {
        static const size_t max_size = std::vector<T>::max_size();
        if (idx < std::vector<T>::size()) {
            return std::vector<T>::operator[](idx);
        } else if (idx < max_size) {
            return sparse_elements[idx];
        }
        throw std::runtime_error("Impossibly large object id encountered accessing ObjTable");
        return element(0); // doesn't return
    }

    inline T const&
    element(size_t idx) const
    {
        static const size_t max_size = std::vector<T>::max_size();
        if (idx < std::vector<T>::size()) {
            return std::vector<T>::operator[](idx);
        } else if (idx < max_size) {
            return sparse_elements.at(idx);
        }
        throw std::runtime_error("Impossibly large object id encountered accessing ObjTable");
        return element(0); // doesn't return
    }
};

#endif // OBJTABLE_HH
