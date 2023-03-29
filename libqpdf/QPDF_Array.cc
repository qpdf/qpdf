#include <qpdf/QPDF_Array.hh>

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>

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

QPDF_Array::QPDF_Array() :
    QPDFValue(::ot_array, "array")
{
}

QPDF_Array::QPDF_Array(QPDF_Array const& other) :
    QPDFValue(::ot_array, "array"),
    sparse(other.sparse),
    sp_size(other.sp_size),
    sp_elements(other.sp_elements),
    elements(other.elements)
{
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
    if (sparse) {
        for (auto&& item: v) {
            if (item->getTypeCode() != ::ot_null ||
                item->getObjGen().isIndirect()) {
                sp_elements[sp_size] = std::move(item);
            }
            ++sp_size;
        }
    } else {
        elements = std::move(v);
    }
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
QPDF_Array::copy(bool shallow)
{
    if (shallow) {
        return do_create(new QPDF_Array(*this));
    } else {
        if (sparse) {
            QPDF_Array* result = new QPDF_Array();
            result->sp_size = sp_size;
            for (auto const& element: sp_elements) {
                auto const& obj = element.second;
                result->sp_elements[element.first] =
                    obj->getObjGen().isIndirect() ? obj : obj->copy();
            }
            return do_create(result);
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
            return create(std::move(result), false);
        }
    }
}

void
QPDF_Array::disconnect()
{
    if (sparse) {
        for (auto& item: sp_elements) {
            auto& obj = item.second;
            if (!obj->getObjGen().isIndirect()) {
                obj->disconnect();
            }
        }
    } else {
        for (auto& obj: elements) {
            if (!obj->getObjGen().isIndirect()) {
                obj->disconnect();
            }
        }
    }
}

std::string
QPDF_Array::unparse()
{
    if (sparse) {
        std::string result = "[ ";
        for (int i = 0; i < sp_size; ++i) {
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
        for (int i = 0; i < sp_size; ++i) {
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
        auto const& iter = sp_elements.find(n);
        return iter == sp_elements.end() ? null_oh : (*iter).second;
    } else {
        return elements[size_t(n)];
    }
}

std::vector<QPDFObjectHandle>
QPDF_Array::getAsVector() const
{
    if (sparse) {
        std::vector<QPDFObjectHandle> v;
        v.reserve(size_t(size()));
        for (auto const& item: sp_elements) {
            v.resize(size_t(item.first), null_oh);
            v.push_back(item.second);
        }
        v.resize(size_t(size()), null_oh);
        return v;
    } else {
        return {elements.cbegin(), elements.cend()};
    }
}

bool
QPDF_Array::setAt(int at, QPDFObjectHandle const& oh)
{
    if (at < 0 || at >= size()) {
        return false;
    }
    checkOwnership(oh);
    if (sparse) {
        sp_elements[at] = oh.getObj();
    } else {
        elements[size_t(at)] = oh.getObj();
    }
    return true;
}

void
QPDF_Array::setFromVector(std::vector<QPDFObjectHandle> const& v)
{
    elements.resize(0);
    elements.reserve(v.size());
    for (auto const& item: v) {
        checkOwnership(item);
        elements.push_back(item.getObj());
    }
}

bool
QPDF_Array::insert(int at, QPDFObjectHandle const& item)
{
    int sz = size();
    if (at < 0 || at > sz) {
        // As special case, also allow insert beyond the end
        return false;
    } else if (at == sz) {
        push_back(item);
    } else {
        checkOwnership(item);
        if (sparse) {
            auto iter = sp_elements.crbegin();
            while (iter != sp_elements.crend()) {
                auto key = (iter++)->first;
                if (key >= at) {
                    auto nh = sp_elements.extract(key);
                    ++nh.key();
                    sp_elements.insert(std::move(nh));
                } else {
                    break;
                }
            }
            sp_elements[at] = item.getObj();
            ++sp_size;
        } else {
            elements.insert(elements.cbegin() + at, item.getObj());
        }
    }
    return true;
}

void
QPDF_Array::push_back(QPDFObjectHandle const& item)
{
    checkOwnership(item);
    if (sparse) {
        sp_elements[sp_size++] = item.getObj();
    } else {
        elements.push_back(item.getObj());
    }
}

bool
QPDF_Array::erase(int at)
{
    if (at < 0 || at >= size()) {
        return false;
    }
    if (sparse) {
        auto end = sp_elements.end();
        if (auto iter = sp_elements.lower_bound(at); iter != end) {
            if (iter->first == at) {
                iter++;
                sp_elements.erase(at);
            }

            while (iter != end) {
                auto nh = sp_elements.extract(iter++);
                --nh.key();
                sp_elements.insert(std::move(nh));
            }
        }
        --sp_size;
    } else {
        elements.erase(elements.cbegin() + at);
    }
    return true;
}
