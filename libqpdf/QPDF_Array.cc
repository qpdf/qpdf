#include <qpdf/QPDF_Array.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <stdexcept>

QPDF_Array::QPDF_Array() :
    QPDFValue(::ot_array, "array")
{
}

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle> const& v) :
    QPDFValue(::ot_array, "array")
{
    setFromVector(v);
}

QPDF_Array::QPDF_Array(std::vector<std::shared_ptr<QPDFObject>>&& v) :
    QPDFValue(::ot_array, "array")
{
    setFromVector(std::move(v));
}

QPDF_Array::QPDF_Array(QPDF_Array const& other) :
    QPDFValue(::ot_array, "array"),
    elements(other.elements),
    n_elements(other.n_elements)
{
}

QPDF_Array::QPDF_Array(QPDF_Array&& other) :
    QPDFValue(::ot_array, "array"),
    elements(std::move(other.elements)),
    n_elements(other.n_elements)
{
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(std::vector<QPDFObjectHandle> const& items)
{
    return do_create(new QPDF_Array(items));
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(std::vector<std::shared_ptr<QPDFObject>>&& items)
{
    return do_create(new QPDF_Array(std::move(items)));
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(QPDF_Array const& other)
{
    return do_create(new QPDF_Array(other));
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(QPDF_Array&& other)
{
    return do_create(new QPDF_Array(other));
}

std::shared_ptr<QPDFObject>
QPDF_Array::copy(bool shallow)
{
    if (shallow) {
        return create(*this);
    } else {
        auto* result = new QPDF_Array();
        result->n_elements = n_elements;
        for (auto const& element: elements) {
            auto value = element.second;
            if (auto* obj = value.getObjectPtr()) {
                result->elements[element.first] =
                    obj->getObjGen().isIndirect() ? value : obj->copy();
            } else {
                result->elements[element.first] = value;
            }
        }
        return do_create(result);
    }
}

void
QPDF_Array::disconnect()
{
    for (auto& item: elements) {
        if (auto* obj = item.second.getObjectPtr()) {
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
    int next = 0;
    for (auto& item: elements) {
        int key = item.first;
        for (int j = next; j < key; ++j) {
            result += "null ";
        }
        result += item.second.unparse();
        result += " ";
        next = ++key;
    }
    for (int j = next; j < n_elements; ++j) {
        result += "null ";
    }
    result += "]";
    return result;
}

JSON
QPDF_Array::getJSON(int json_version)
{
    static JSON j_null = JSON::makeNull();
    JSON j_array = JSON::makeArray();
    int next = 0;
    for (auto& item: elements) {
        int key = item.first;
        for (int j = next; j < key; ++j) {
            j_array.addArrayElement(j_null);
        }
        j_array.addArrayElement(item.second.getJSON(json_version));
        next = ++key;
    }
    for (int j = next; j < n_elements; ++j) {
        j_array.addArrayElement(j_null);
    }
    return j_array;
}

int
QPDF_Array::size() const
{
    return n_elements;
}

QPDFObjectHandle
QPDF_Array::at(int n) const
{
    if ((0 <= n) && (n < n_elements)) {
        if (auto result = unchecked_at(n)) {
            return result;
        } else {
            return QPDF_Null::create();
        }
    } else {
        return {};
    }
}

std::vector<QPDFObjectHandle>
QPDF_Array::getAsVector() const
{
    std::vector<QPDFObjectHandle> v;
    for (int i = 0; i < n_elements; ++i) {
        auto ptr = unchecked_at(i);
        v.push_back(ptr ? ptr : QPDF_Null::create());
    }
    return v;
}

bool
QPDF_Array::setAt(int n, QPDFObjectHandle const& oh)
{
    if (0 <= n && n < n_elements) {
        if (auto ptr = oh.getObjectPtr()) {
            if (ptr->isDirectNull()) {
                elements.erase(n);
            } else {
                checkOwnership(ptr);
                elements[n] = oh;
            }
        }
        return true;
    } else {
        return false;
    }
}

void
QPDF_Array::setFromVector(std::vector<QPDFObjectHandle> const& v)
{
    clear();
    for (auto const& iter: v) {
        push_back(iter);
    }
}

void
QPDF_Array::setFromVector(std::vector<std::shared_ptr<QPDFObject>>&& v)
{
    clear();
    for (auto&& item: v) {
        if (item) {
            // This method is only called from QPDFParser. No need to check for
            // ownership or direct nulls.
            elements[n_elements] = item;
        }
        ++n_elements;
    }
}

bool
QPDF_Array::insert(int at, QPDFObjectHandle const& item)
{
    if (at == n_elements) {
        // As special case, also allow inserting to the last position.
        push_back(item);
        return true;
    } else if (0 <= at && (at < n_elements)) {
        checkOwnership(item.getObjectPtr());

        if (auto iter = elements.lower_bound(at); iter != elements.end()) {
            auto nh1 = elements.extract(iter++);
            while (iter != elements.end()) {
                // We need to extract the next element before we can insert the
                // current element because the current elements new key may
                // clash with the next elements existing key.
                auto nh2 = elements.extract(iter++);
                ++nh1.key();
                elements.insert(std::move(nh1));
                nh1 = std::move(nh2);
            }
            ++nh1.key();
            elements.insert(std::move(nh1));
        }
        elements[at] = item;
        ++n_elements;
        return true;
    } else {
        return false;
    }
}

bool
QPDF_Array::push_back(QPDFObjectHandle const& item)
{
    auto ptr = item.getObjectPtr();
    if (ptr && !ptr->isDirectNull()) {
        checkOwnership(ptr);
        elements[n_elements] = item;
    }
    ++n_elements;
    return true;
}

bool
QPDF_Array::erase(int at)
{
    if (0 <= at && at < n_elements) {
        if (auto iter = elements.lower_bound(at); iter != elements.end()) {
            if (iter->first == at) {
                iter++;
                elements.erase(at);
            }

            while (iter != elements.end()) {
                auto nh = elements.extract(iter++);
                --nh.key();
                elements.insert(std::move(nh));
            }
        }
        --n_elements;
        return true;
    } else {
        return false;
    }
}

void
QPDF_Array::addExplicitElementsToList(std::list<QPDFObjectHandle>& l) const
{
    for (auto const& iter: elements) {
        l.push_back(iter.second);
    }
}
