#include <qpdf/QPDFObjectHandle_private.hh>

#include <qpdf/QTC.hh>

#include <array>
#include <utility>

using namespace std::literals;
using namespace qpdf;

static const QPDFObjectHandle null_oh = QPDFObjectHandle::newNull();

static size_t
to_s(int n)
{
    return static_cast<size_t>(n);
}

static int
to_i(size_t n)
{
    return static_cast<int>(n);
}

inline void
Array::checkOwnership(QPDFObjectHandle const& item) const
{
    if (!item) {
        throw std::logic_error("Attempting to add an uninitialized object to a QPDF_Array.");
    }
    if (qpdf() && item.qpdf() && qpdf() != item.qpdf()) {
        throw std::logic_error(
            "Attempting to add an object from a different QPDF. Use "
            "QPDF::copyForeignObject to add objects from another file.");
    }
}

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle>&& v, bool sparse)
{
    if (sparse) {
        sp = std::make_unique<Sparse>();
        for (auto& item: v) {
            if (item.raw_type_code() != ::ot_null || item.indirect()) {
                sp->elements[sp->size] = std::move(item);
            }
            ++sp->size;
        }
    } else {
        elements = std::move(v);
    }
}

QPDF_Array*
Array::array() const
{
    if (auto a = as<QPDF_Array>()) {
        return a;
    }

    throw std::runtime_error("Expected an array but found a non-array object");
    return nullptr; // unreachable
}

Array::Array(bool empty) :
    BaseHandle(empty ? QPDFObject::create<QPDF_Array>() : nullptr)
{
}

Array::Array(std::vector<QPDFObjectHandle> const& items) :
    BaseHandle(QPDFObject::create<QPDF_Array>(items))
{
}

Array::Array(std::vector<QPDFObjectHandle>&& items) :
    BaseHandle(QPDFObject::create<QPDF_Array>(std::move(items)))
{
}

Array::iterator
Array::begin()
{
    if (auto a = as<QPDF_Array>()) {
        if (!a->sp) {
            return a->elements.begin();
        }
        if (!sp_elements) {
            sp_elements = std::make_unique<std::vector<QPDFObjectHandle>>(getAsVector());
        }
        return sp_elements->begin();
    }
    return {};
}

Array::iterator
Array::end()
{
    if (auto a = as<QPDF_Array>()) {
        if (!a->sp) {
            return a->elements.end();
        }
        if (!sp_elements) {
            sp_elements = std::make_unique<std::vector<QPDFObjectHandle>>(getAsVector());
        }
        return sp_elements->end();
    }
    return {};
}

Array::const_iterator
Array::cbegin()
{
    if (auto a = as<QPDF_Array>()) {
        if (!a->sp) {
            return a->elements.cbegin();
        }
        if (!sp_elements) {
            sp_elements = std::make_unique<std::vector<QPDFObjectHandle>>(getAsVector());
        }
        return sp_elements->cbegin();
    }
    return {};
}

Array::const_iterator
Array::cend()
{
    if (auto a = as<QPDF_Array>()) {
        if (!a->sp) {
            return a->elements.cend();
        }
        if (!sp_elements) {
            sp_elements = std::make_unique<std::vector<QPDFObjectHandle>>(getAsVector());
        }
        return sp_elements->cend();
    }
    return {};
}

Array::const_reverse_iterator
Array::crbegin()
{
    if (auto a = as<QPDF_Array>()) {
        if (!a->sp) {
            return a->elements.crbegin();
        }
        if (!sp_elements) {
            sp_elements = std::make_unique<std::vector<QPDFObjectHandle>>(getAsVector());
        }
        return sp_elements->crbegin();
    }
    return {};
}

Array::const_reverse_iterator
Array::crend()
{
    if (auto a = as<QPDF_Array>()) {
        if (!a->sp) {
            return a->elements.crend();
        }
        if (!sp_elements) {
            sp_elements = std::make_unique<std::vector<QPDFObjectHandle>>(getAsVector());
        }
        return sp_elements->crend();
    }
    return {};
}

QPDFObjectHandle
Array::null() const
{
    return null_oh;
}

size_t
Array::size() const
{
    if (auto a = as<QPDF_Array>()) {
        return a->sp ? a->sp->size : a->elements.size();
    }
    return 0;
}

QPDFObjectHandle const&
Array::operator[](size_t n) const
{
    static const QPDFObjectHandle null_obj;
    auto a = as<QPDF_Array>();
    if (!a) {
        return null_obj;
    }
    if (a->sp) {
        auto const& iter = a->sp->elements.find(n);
        return iter == a->sp->elements.end() ? null_obj : iter->second;
    }
    return n >= a->elements.size() ? null_obj : a->elements[n];
}

QPDFObjectHandle const&
Array::operator[](int n) const
{
    static const QPDFObjectHandle null_obj;
    if (n < 0) {
        return null_obj;
    }
    return (*this)[static_cast<size_t>(n)];
}

QPDFObjectHandle
Array::get(size_t n) const
{
    if (n >= size()) {
        return {};
    }
    auto a = array();
    if (!a->sp) {
        return a->elements[n];
    }
    auto const& iter = a->sp->elements.find(n);
    return iter == a->sp->elements.end() ? null() : iter->second;
}

QPDFObjectHandle
Array::get(int n) const
{
    if (n < 0) {
        return {};
    }
    return get(to_s(n));
}

std::vector<QPDFObjectHandle>
Array::getAsVector() const
{
    auto a = array();
    if (a->sp) {
        std::vector<QPDFObjectHandle> v;
        v.reserve(size());
        for (auto const& item: a->sp->elements) {
            v.resize(item.first, null_oh);
            v.emplace_back(item.second);
        }
        v.resize(size(), null_oh);
        return v;
    } else {
        return a->elements;
    }
}

bool
Array::set(size_t at, QPDFObjectHandle const& oh)
{
    if (at >= size()) {
        return false;
    }
    auto a = array();
    checkOwnership(oh);
    if (a->sp) {
        a->sp->elements[at] = oh;
    } else {
        a->elements[at] = oh;
    }
    return true;
}

bool
Array::set(int at, QPDFObjectHandle const& oh)
{
    if (at < 0) {
        return false;
    }
    return set(to_s(at), oh);
}

void
Array::setFromVector(std::vector<QPDFObjectHandle> const& v)
{
    auto a = array();
    a->elements.resize(0);
    a->elements.reserve(v.size());
    for (auto const& item: v) {
        checkOwnership(item);
        a->elements.emplace_back(item);
    }
}

bool
Array::insert(size_t at, QPDFObjectHandle const& item)
{
    auto a = array();
    size_t sz = size();
    if (at > sz) {
        return false;
    }
    checkOwnership(item);
    if (at == sz) {
        // As special case, also allow insert beyond the end
        push_back(item);
        return true;
    }
    if (!a->sp) {
        a->elements.insert(a->elements.cbegin() + to_i(at), item);
        return true;
    }
    auto iter = a->sp->elements.crbegin();
    while (iter != a->sp->elements.crend()) {
        auto key = (iter++)->first;
        if (key >= at) {
            auto nh = a->sp->elements.extract(key);
            ++nh.key();
            a->sp->elements.insert(std::move(nh));
        } else {
            break;
        }
    }
    a->sp->elements[at] = item;
    ++a->sp->size;
    return true;
}

bool
Array::insert(int at_i, QPDFObjectHandle const& item)
{
    if (at_i < 0) {
        return false;
    }
    return insert(to_s(at_i), item);
}

void
Array::push_back(QPDFObjectHandle const& item)
{
    auto a = array();
    checkOwnership(item);
    if (a->sp) {
        a->sp->elements[(a->sp->size)++] = item;
    } else {
        a->elements.emplace_back(item);
    }
}

bool
Array::erase(size_t at)
{
    auto a = array();
    if (at >= size()) {
        return false;
    }
    if (!a->sp) {
        a->elements.erase(a->elements.cbegin() + to_i(at));
        return true;
    }
    auto end = a->sp->elements.end();
    if (auto iter = a->sp->elements.lower_bound(at); iter != end) {
        if (iter->first == at) {
            iter++;
            a->sp->elements.erase(at);
        }

        while (iter != end) {
            auto nh = a->sp->elements.extract(iter++);
            --nh.key();
            a->sp->elements.insert(std::move(nh));
        }
    }
    --(a->sp->size);
    return true;
}

bool
Array::erase(int at_i)
{
    if (at_i < 0) {
        return false;
    }
    return erase(to_s(at_i));
}

QPDFObjectHandle
QPDFObjectHandle::newArray()
{
    return {Array()};
}

QPDFObjectHandle
QPDFObjectHandle::newArray(std::vector<QPDFObjectHandle> const& items)
{
    return {Array(items)};
}

int
QPDFObjectHandle::getArrayNItems() const
{
    auto s = size();
    if (s > 1 || isArray()) {
        return to_i(s);
    }
    typeWarning("array", "treating as empty");
    return 0;
}

QPDFObjectHandle
QPDFObjectHandle::getArrayItem(int n) const
{
    if (auto array = Array(*this)) {
        if (auto result = array[n]) {
            return result;
        }
        if (n >= 0 && std::cmp_less(n, array.size())) {
            // sparse array null
            return newNull();
        }
        objectWarning("returning null for out of bounds array access");

    } else {
        typeWarning("array", "returning null");
    }
    static auto constexpr msg = " -> null returned from invalid array access"sv;
    return QPDF_Null::create(obj, msg, "");
}

bool
QPDFObjectHandle::isRectangle() const
{
    Array array(*this);
    for (auto const& oh: array) {
        if (!oh.isNumber()) {
            return false;
        }
    }
    return array.size() == 4;
}

bool
QPDFObjectHandle::isMatrix() const
{
    Array array(*this);
    for (auto const& oh: array) {
        if (!oh.isNumber()) {
            return false;
        }
    }
    return array.size() == 6;
}

QPDFObjectHandle::Rectangle
QPDFObjectHandle::getArrayAsRectangle() const
{
    Array array(*this);
    if (array.size() != 4) {
        return {};
    }
    std::array<double, 4> items;
    for (size_t i = 0; i < 4; ++i) {
        if (!array[i].getValueAsNumber(items[i])) {
            return {};
        }
    }
    return {
        std::min(items[0], items[2]),
        std::min(items[1], items[3]),
        std::max(items[0], items[2]),
        std::max(items[1], items[3])};
}

QPDFObjectHandle::Matrix
QPDFObjectHandle::getArrayAsMatrix() const
{
    Array array(*this);
    if (array.size() != 6) {
        return {};
    }
    std::array<double, 6> items;
    for (size_t i = 0; i < 6; ++i) {
        if (!array[i].getValueAsNumber(items[i])) {
            return {};
        }
    }
    return {items[0], items[1], items[2], items[3], items[4], items[5]};
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getArrayAsVector() const
{
    if (auto array = as_array(strict)) {
        return array.getAsVector();
    }
    typeWarning("array", "treating as empty");
    QTC::TC("qpdf", "QPDFObjectHandle array treating as empty vector");
    return {};
}

void
QPDFObjectHandle::setArrayItem(int n, QPDFObjectHandle const& item)
{
    if (auto array = as_array(strict)) {
        if (!array.set(n, item)) {
            objectWarning("ignoring attempt to set out of bounds array item");
        }
    } else {
        typeWarning("array", "ignoring attempt to set item");
    }
}
void
QPDFObjectHandle::setArrayFromVector(std::vector<QPDFObjectHandle> const& items)
{
    if (auto array = as_array(strict)) {
        array.setFromVector(items);
    } else {
        typeWarning("array", "ignoring attempt to replace items");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring replace items");
    }
}

void
QPDFObjectHandle::insertItem(int at, QPDFObjectHandle const& item)
{
    if (auto array = as_array(strict)) {
        if (!array.insert(at, item)) {
            objectWarning("ignoring attempt to insert out of bounds array item");
            QTC::TC("qpdf", "QPDFObjectHandle insert array bounds");
        }
    } else {
        typeWarning("array", "ignoring attempt to insert item");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring insert item");
    }
}

QPDFObjectHandle
QPDFObjectHandle::insertItemAndGetNew(int at, QPDFObjectHandle const& item)
{
    insertItem(at, item);
    return item;
}

void
QPDFObjectHandle::appendItem(QPDFObjectHandle const& item)
{
    if (auto array = as_array(strict)) {
        array.push_back(item);
    } else {
        typeWarning("array", "ignoring attempt to append item");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring append item");
    }
}

QPDFObjectHandle
QPDFObjectHandle::appendItemAndGetNew(QPDFObjectHandle const& item)
{
    appendItem(item);
    return item;
}

void
QPDFObjectHandle::eraseItem(int at)
{
    if (auto array = as_array(strict)) {
        if (!array.erase(at)) {
            objectWarning("ignoring attempt to erase out of bounds array item");
            QTC::TC("qpdf", "QPDFObjectHandle erase array bounds");
        }
    } else {
        typeWarning("array", "ignoring attempt to erase item");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring erase item");
    }
}

QPDFObjectHandle
QPDFObjectHandle::eraseItemAndGetOld(int at)
{
    auto result = Array(*this)[at];
    eraseItem(at);
    return result ? result : newNull();
}

size_t
BaseHandle::size() const
{
    switch (resolved_type_code()) {
    case ::ot_array:
        return as<QPDF_Array>()->size();
    case ::ot_uninitialized:
    case ::ot_reserved:
    case ::ot_null:
    case ::ot_destroyed:
    case ::ot_unresolved:
    case ::ot_reference:
        return 0;
    case ::ot_boolean:
    case ::ot_integer:
    case ::ot_real:
    case ::ot_string:
    case ::ot_name:
    case ::ot_dictionary:
    case ::ot_stream:
    case ::ot_inlineimage:
    case ::ot_operator:
        return 1;
    default:
        throw std::logic_error("Unexpected type code in size"); // unreachable
        return 0;                                               // unreachable
    }
}

QPDFObjectHandle
BaseHandle::operator[](size_t n) const
{
    if (resolved_type_code() == ::ot_array) {
        return Array(obj)[n];
    }
    if (n < size()) {
        return *this;
    }
    return {};
}

QPDFObjectHandle
BaseHandle::operator[](int n) const
{
    if (n < 0) {
        return {};
    }
    return (*this)[static_cast<size_t>(n)];
}
