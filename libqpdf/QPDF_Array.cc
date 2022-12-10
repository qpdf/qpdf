#include <qpdf/QPDF_Array.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QUtil.hh>
#include <stdexcept>

static const QPDFObjectHandle null_oh = QPDFObjectHandle::newNull();

inline void
QPDF_Array::checkOwnership(QPDFObjectHandle const& item) const
{
    if (auto obj = item.getObjectPtr()) {
        if (qpdf) {
            if (auto item_qpdf = obj->getQPDF()) {
                if (qpdf != item_qpdf) {
                    throw std::logic_error(
                        "Attempting to add an object from a different QPDF. "
                        "Use QPDF::copyForeignObject to add objects from "
                        "another file.");
                }
            }
        }
    } else {
        throw std::logic_error(
            "Attempting to add an uninitialized object to a QPDF_Array.");
    }
}

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle> const& v) :
    QPDFValue(::ot_array, "array")
{
    setFromVector(v);
}

QPDF_Array::QPDF_Array(
    std::vector<std::shared_ptr<QPDFObject>>&& v, bool sparse) :
    QPDFValue(::ot_array, "array"),
    sparse(sparse)
{
    setFromVector(std::move(v));
}

QPDF_Array::QPDF_Array(SparseOHArray const& items) :
    QPDFValue(::ot_array, "array"),
    sparse(true),
    sp_elements(items)

{
}

QPDF_Array::QPDF_Array(std::vector<std::shared_ptr<QPDFObject>> const& items) :
    QPDFValue(::ot_array, "array"),
    sparse(false),
    elements(items)
{
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(std::vector<QPDFObjectHandle> const& items)
{
    return do_create(new QPDF_Array(items));
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(
    std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse)
{
    return do_create(new QPDF_Array(std::move(items), sparse));
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(SparseOHArray const& items)
{
    return do_create(new QPDF_Array(items));
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(std::vector<std::shared_ptr<QPDFObject>> const& items)
{
    return do_create(new QPDF_Array(items));
}

std::shared_ptr<QPDFObject>
QPDF_Array::copy(bool shallow)
{
    if (sparse) {
        return create(shallow ? sp_elements : sp_elements.copy());
    } else {
        if (shallow) {
            return create(elements);
        } else {
            std::vector<std::shared_ptr<QPDFObject>> result;
            result.reserve(elements.size());
            for (auto const& element: elements) {
                result.push_back(
                    element
                        ? (element->getObjGen().isIndirect() ? element
                                                             : element->copy())
                        : element);
            }
            return create(result);
        }
    }
}

void
QPDF_Array::disconnect()
{
    if (sparse) {
        sp_elements.disconnect();
    } else {
        for (auto const& iter: elements) {
            if (iter) {
                QPDFObjectHandle::DisconnectAccess::disconnect(iter);
            }
        }
    }
}

std::string
QPDF_Array::unparse()
{
    if (sparse) {
        std::string result = "[ ";
        int size = sp_elements.size();
        for (int i = 0; i < size; ++i) {
            result += at(i).unparse();
            result += " ";
        }
        result += "]";
        return result;
    } else {
        std::string result = "[ ";
        auto size = elements.size();
        for (int i = 0; i < int(size); ++i) {
            result += at(i).unparse();
            result += " ";
        }
        result += "]";
        return result;
    }
}

JSON
QPDF_Array::getJSON(int json_version)
{
    if (sparse) {
        JSON j = JSON::makeArray();
        int size = sp_elements.size();
        for (int i = 0; i < size; ++i) {
            j.addArrayElement(at(i).getJSON(json_version));
        }
        return j;
    } else {
        JSON j = JSON::makeArray();
        size_t size = elements.size();
        for (int i = 0; i < int(size); ++i) {
            j.addArrayElement(at(i).getJSON(json_version));
        }
        return j;
    }
}

QPDFObjectHandle
QPDF_Array::at(int n) const noexcept
{
    if (n < 0 || n >= size()) {
        return {};
    } else if (sparse) {
        auto const& iter = sp_elements.elements.find(n);
        return iter == sp_elements.elements.end() ? null_oh : (*iter).second;
    } else {
        return elements[size_t(n)];
    }
}

void
QPDF_Array::getAsVector(std::vector<QPDFObjectHandle>& v) const
{
    if (sparse) {
        int size = sp_elements.size();
        for (int i = 0; i < size; ++i) {
            v.push_back(at(i));
        }
    } else {
        v = std::vector<QPDFObjectHandle>(elements.cbegin(), elements.cend());
    }
}

void
QPDF_Array::setItem(int n, QPDFObjectHandle const& oh)
{
    if (sparse) {
        sp_elements.setAt(n, oh);
    } else {
        size_t idx = size_t(n);
        if (n < 0 || idx >= elements.size()) {
            throw std::logic_error("bounds error setting item in QPDF_Array");
        }
        elements[idx] = oh.getObj();
    }
}

void
QPDF_Array::setFromVector(std::vector<QPDFObjectHandle> const& v)
{
    if (sparse) {
        sp_elements = SparseOHArray();
        for (auto const& iter: v) {
            sp_elements.append(iter);
        }
    } else {
        elements.resize(0);
        for (auto const& iter: v) {
            elements.push_back(iter.getObj());
        }
    }
}

void
QPDF_Array::setFromVector(std::vector<std::shared_ptr<QPDFObject>>&& v)
{
    if (sparse) {
        sp_elements = SparseOHArray();
        for (auto&& item: v) {
            if (item->getTypeCode() != ::ot_null ||
                item->getObjGen().isIndirect()) {
                sp_elements.append(std::move(item));
            } else {
                ++sp_elements.n_elements;
            }
        }
    } else {
        elements = std::move(v);
    }
}

void
QPDF_Array::insertItem(int at, QPDFObjectHandle const& item)
{
    if (sparse) {
        // As special case, also allow insert beyond the end
        if ((at < 0) || (at > sp_elements.size())) {
            throw std::logic_error(
                "INTERNAL ERROR: bounds error accessing QPDF_Array element");
        }
        sp_elements.insert(at, item);
    } else {
        // As special case, also allow insert beyond the end
        size_t idx = QIntC::to_size(at);
        if ((at < 0) || (at > QIntC::to_int(elements.size()))) {
            throw std::logic_error(
                "INTERNAL ERROR: bounds error accessing QPDF_Array element");
        }
        if (idx == elements.size()) {
            // Allow inserting to the last position
            elements.push_back(item.getObj());
        } else {
            int n = int(idx);
            elements.insert(elements.cbegin() + n, item.getObj());
        }
    }
}

void
QPDF_Array::push_back(QPDFObjectHandle const& item)
{
    checkOwnership(item);
    if (sparse) {
        sp_elements.elements[sp_elements.n_elements++] = item.getObj();
    } else {
        elements.push_back(item.getObj());
    }
}

void
QPDF_Array::eraseItem(int at)
{
    if (sparse) {
        sp_elements.erase(at);
    } else {
        size_t idx = QIntC::to_size(at);
        if (idx >= elements.size()) {
            throw std::logic_error("bounds error erasing item from OHArray");
        }
        int n = int(idx);
        elements.erase(elements.cbegin() + n);
    }
}
