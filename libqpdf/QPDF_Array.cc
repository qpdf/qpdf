#include <qpdf/QPDFObjectHandle_private.hh>

#include <qpdf/QTC.hh>

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
    auto a = array();
    return a->sp ? a->sp->size : a->elements.size();
}

std::pair<bool, QPDFObjectHandle>
Array::at(int n) const
{
    auto a = array();
    if (n < 0 || std::cmp_greater_equal(n, size())) {
        return {false, {}};
    }
    if (!a->sp) {
        return {true, a->elements[to_s(n)]};
    }
    auto const& iter = a->sp->elements.find(to_s(n));
    return {true, iter == a->sp->elements.end() ? null() : iter->second};
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
Array::setAt(int at, QPDFObjectHandle const& oh)
{
    if (at < 0 || std::cmp_greater_equal(at, size())) {
        return false;
    }
    auto a = array();
    checkOwnership(oh);
    if (a->sp) {
        a->sp->elements[to_s(at)] = oh;
    } else {
        a->elements[to_s(at)] = oh;
    }
    return true;
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
Array::insert(int at_i, QPDFObjectHandle const& item)
{
    auto a = array();
    size_t sz = size();
    if (at_i < 0) {
        return false;
    }
    size_t at = to_s(at_i);
    if (at > sz) {
        return false;
    }
    if (at == sz) {
        // As special case, also allow insert beyond the end
        push_back(item);
        return true;
    }
    checkOwnership(item);
    if (a->sp) {
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
        a->sp->elements[at] = item.getObj();
        ++a->sp->size;
    } else {
        a->elements.insert(a->elements.cbegin() + at_i, item.getObj());
    }
    return true;
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
Array::erase(int at_i)
{
    auto a = array();
    if (at_i < 0) {
        return false;
    }
    size_t at = to_s(at_i);
    if (at >= size()) {
        return false;
    }
    if (a->sp) {
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
    } else {
        a->elements.erase(a->elements.cbegin() + at_i);
    }
    return true;
}

int
QPDFObjectHandle::getArrayNItems() const
{
    if (auto array = as_array(strict)) {
        return to_i(array.size());
    }
    typeWarning("array", "treating as empty");
    QTC::TC("qpdf", "QPDFObjectHandle array treating as empty");
    return 0;
}

QPDFObjectHandle
QPDFObjectHandle::getArrayItem(int n) const
{
    if (auto array = as_array(strict)) {
        if (auto const [success, oh] = array.at(n); success) {
            return oh;
        } else {
            objectWarning("returning null for out of bounds array access");
            QTC::TC("qpdf", "QPDFObjectHandle array bounds");
        }
    } else {
        typeWarning("array", "returning null");
        QTC::TC("qpdf", "QPDFObjectHandle array null for non-array");
    }
    static auto constexpr msg = " -> null returned from invalid array access"sv;
    return QPDF_Null::create(obj, msg, "");
}

bool
QPDFObjectHandle::isRectangle() const
{
    if (auto array = as_array(strict)) {
        for (int i = 0; i < 4; ++i) {
            if (auto item = array.at(i).second; !item.isNumber()) {
                return false;
            }
        }
        return array.size() == 4;
    }
    return false;
}

bool
QPDFObjectHandle::isMatrix() const
{
    if (auto array = as_array(strict)) {
        for (int i = 0; i < 6; ++i) {
            if (auto item = array.at(i).second; !item.isNumber()) {
                return false;
            }
        }
        return array.size() == 6;
    }
    return false;
}

QPDFObjectHandle::Rectangle
QPDFObjectHandle::getArrayAsRectangle() const
{
    if (auto array = as_array(strict)) {
        if (array.size() != 4) {
            return {};
        }
        double items[4];
        for (int i = 0; i < 4; ++i) {
            if (auto item = array.at(i).second; !item.getValueAsNumber(items[i])) {
                return {};
            }
        }
        return {
            std::min(items[0], items[2]),
            std::min(items[1], items[3]),
            std::max(items[0], items[2]),
            std::max(items[1], items[3])};
    }
    return {};
}

QPDFObjectHandle::Matrix
QPDFObjectHandle::getArrayAsMatrix() const
{
    if (auto array = as_array(strict)) {
        if (array.size() != 6) {
            return {};
        }
        double items[6];
        for (int i = 0; i < 6; ++i) {
            if (auto item = array.at(i).second; !item.getValueAsNumber(items[i])) {
                return {};
            }
        }
        return {items[0], items[1], items[2], items[3], items[4], items[5]};
    }
    return {};
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
        if (!array.setAt(n, item)) {
            objectWarning("ignoring attempt to set out of bounds array item");
            QTC::TC("qpdf", "QPDFObjectHandle set array bounds");
        }
    } else {
        typeWarning("array", "ignoring attempt to set item");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring set item");
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
    auto array = as_array(strict);
    auto result =
        (array && std::cmp_less(at, array.size()) && at >= 0) ? array.at(at).second : newNull();
    eraseItem(at);
    return result;
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
