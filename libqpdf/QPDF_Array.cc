#include <qpdf/QPDF_Array.hh>

#include <qpdf/JSON_writer.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QTC.hh>

static const QPDFObjectHandle null_oh = QPDFObjectHandle::newNull();

inline void
QPDF_Array::checkOwnership(QPDFObjectHandle const& item) const
{
    if (auto obj = item.getObjectPtr()) {
        if (qpdf) {
            if (auto item_qpdf = obj->getQPDF()) {
                if (qpdf != item_qpdf) {
                    throw std::logic_error(
                        "Attempting to add an object from a different QPDF. Use "
                        "QPDF::copyForeignObject to add objects from another file.");
                }
            }
        }
    } else {
        throw std::logic_error("Attempting to add an uninitialized object to a QPDF_Array.");
    }
}

QPDF_Array::QPDF_Array() :
    QPDFValue(::ot_array, "array")
{
}

QPDF_Array::QPDF_Array(QPDF_Array const& other) :
    QPDFValue(::ot_array, "array"),
    sp(other.sp ? std::make_unique<Sparse>(*other.sp) : nullptr)
{
}

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle> const& v) :
    QPDFValue(::ot_array, "array")
{
    setFromVector(v);
}

QPDF_Array::QPDF_Array(std::vector<std::shared_ptr<QPDFObject>>&& v, bool sparse) :
    QPDFValue(::ot_array, "array")
{
    if (sparse) {
        sp = std::make_unique<Sparse>();
        for (auto&& item: v) {
            if (item->getTypeCode() != ::ot_null || item->getObjGen().isIndirect()) {
                sp->elements[sp->size] = std::move(item);
            }
            ++sp->size;
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
QPDF_Array::create(std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse)
{
    return do_create(new QPDF_Array(std::move(items), sparse));
}

std::shared_ptr<QPDFObject>
QPDF_Array::copy(bool shallow)
{
    if (shallow) {
        return do_create(new QPDF_Array(*this));
    } else {
        QTC::TC("qpdf", "QPDF_Array copy", sp ? 0 : 1);
        if (sp) {
            auto* result = new QPDF_Array();
            result->sp = std::make_unique<Sparse>();
            result->sp->size = sp->size;
            for (auto const& element: sp->elements) {
                auto const& obj = element.second;
                result->sp->elements[element.first] =
                    obj->getObjGen().isIndirect() ? obj : obj->copy();
            }
            return do_create(result);
        } else {
            std::vector<std::shared_ptr<QPDFObject>> result;
            result.reserve(elements.size());
            for (auto const& element: elements) {
                result.push_back(
                    element ? (element->getObjGen().isIndirect() ? element : element->copy())
                            : element);
            }
            return create(std::move(result), false);
        }
    }
}

void
QPDF_Array::disconnect()
{
    if (sp) {
        for (auto& item: sp->elements) {
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
    std::string result = "[ ";
    if (sp) {
        int next = 0;
        for (auto& item: sp->elements) {
            int key = item.first;
            for (int j = next; j < key; ++j) {
                result += "null ";
            }
            item.second->resolve();
            auto og = item.second->getObjGen();
            result += og.isIndirect() ? og.unparse(' ') + " R " : item.second->unparse() + " ";
            next = ++key;
        }
        for (int j = next; j < sp->size; ++j) {
            result += "null ";
        }
    } else {
        for (auto const& item: elements) {
            item->resolve();
            auto og = item->getObjGen();
            result += og.isIndirect() ? og.unparse(' ') + " R " : item->unparse() + " ";
        }
    }
    result += "]";
    return result;
}

JSON
QPDF_Array::getJSON(int json_version)
{
    static const JSON j_null = JSON::makeNull();
    JSON j_array = JSON::makeArray();
    if (sp) {
        int next = 0;
        for (auto& item: sp->elements) {
            int key = item.first;
            for (int j = next; j < key; ++j) {
                j_array.addArrayElement(j_null);
            }
            auto og = item.second->getObjGen();
            j_array.addArrayElement(
                og.isIndirect() ? JSON::makeString(og.unparse(' ') + " R")
                                : item.second->getJSON(json_version));
            next = ++key;
        }
        for (int j = next; j < sp->size; ++j) {
            j_array.addArrayElement(j_null);
        }
    } else {
        for (auto const& item: elements) {
            auto og = item->getObjGen();
            j_array.addArrayElement(
                og.isIndirect() ? JSON::makeString(og.unparse(' ') + " R")
                                : item->getJSON(json_version));
        }
    }
    return j_array;
}

void
QPDF_Array::writeJSON(int json_version, JSON::Writer& p)
{
    p.writeStart('[');
    if (sp) {
        int next = 0;
        for (auto& item: sp->elements) {
            int key = item.first;
            for (int j = next; j < key; ++j) {
                p.writeNext() << "null";
            }
            p.writeNext();
            auto og = item.second->getObjGen();
            if (og.isIndirect()) {
                p << "\"" << og.unparse(' ') << " R\"";
            } else {
                item.second->writeJSON(json_version, p);
            }
            next = ++key;
        }
        for (int j = next; j < sp->size; ++j) {
            p.writeNext() << "null";
        }
    } else {
        for (auto const& item: elements) {
            p.writeNext();
            auto og = item->getObjGen();
            if (og.isIndirect()) {
                p << "\"" << og.unparse(' ') << " R\"";
            } else {
                item->writeJSON(json_version, p);
            }
        }
    }
    p.writeEnd(']');
}

QPDFObjectHandle
QPDF_Array::at(int n) const noexcept
{
    if (n < 0 || n >= size()) {
        return {};
    } else if (sp) {
        auto const& iter = sp->elements.find(n);
        return iter == sp->elements.end() ? null_oh : (*iter).second;
    } else {
        return elements[size_t(n)];
    }
}

std::vector<QPDFObjectHandle>
QPDF_Array::getAsVector() const
{
    if (sp) {
        std::vector<QPDFObjectHandle> v;
        v.reserve(size_t(size()));
        for (auto const& item: sp->elements) {
            v.resize(size_t(item.first), null_oh);
            v.emplace_back(item.second);
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
    if (sp) {
        sp->elements[at] = oh.getObj();
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
        if (sp) {
            auto iter = sp->elements.crbegin();
            while (iter != sp->elements.crend()) {
                auto key = (iter++)->first;
                if (key >= at) {
                    auto nh = sp->elements.extract(key);
                    ++nh.key();
                    sp->elements.insert(std::move(nh));
                } else {
                    break;
                }
            }
            sp->elements[at] = item.getObj();
            ++sp->size;
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
    if (sp) {
        sp->elements[(sp->size)++] = item.getObj();
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
    if (sp) {
        auto end = sp->elements.end();
        if (auto iter = sp->elements.lower_bound(at); iter != end) {
            if (iter->first == at) {
                iter++;
                sp->elements.erase(at);
            }

            while (iter != end) {
                auto nh = sp->elements.extract(iter++);
                --nh.key();
                sp->elements.insert(std::move(nh));
            }
        }
        --(sp->size);
    } else {
        elements.erase(elements.cbegin() + at);
    }
    return true;
}
