#include <qpdf/QPDF_Array.hh>

#include <qpdf/JSON_writer.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QTC.hh>

using namespace std::literals;
using namespace qpdf;

static const QPDFObjectHandle null_oh = QPDFObjectHandle::newNull();

inline void
QPDF_Array::checkOwnership(QPDFObjectHandle const& item) const
{
    // This is only called from QPDF_Array::setFromVector, which in turn is only called from create.
    // At his point qpdf is a nullptr and therefore the ownership check reduces to an uninitialized
    // check
    if (!item.getObjectPtr()) {
        throw std::logic_error("Attempting to add an uninitialized object to a QPDF_Array.");
    }
}

inline void
Array::checkOwnership(QPDFObjectHandle const& item) const
{
    if (auto o = item.getObjectPtr()) {
        if (auto pdf = obj->getQPDF()) {
            if (auto item_qpdf = o->getQPDF()) {
                if (pdf != item_qpdf) {
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
    QPDFValue(::ot_array)
{
}

QPDF_Array::QPDF_Array(QPDF_Array const& other) :
    QPDFValue(::ot_array),
    sp(other.sp ? std::make_unique<Sparse>(*other.sp) : nullptr)
{
}

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle> const& v) :
    QPDFValue(::ot_array)
{
    setFromVector(v);
}

QPDF_Array::QPDF_Array(std::vector<std::shared_ptr<QPDFObject>>&& v, bool sparse) :
    QPDFValue(::ot_array)
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
            auto og = item.second->resolved_object()->getObjGen();
            result += og.isIndirect() ? og.unparse(' ') + " R " : item.second->unparse() + " ";
            next = ++key;
        }
        for (int j = next; j < sp->size; ++j) {
            result += "null ";
        }
    } else {
        for (auto const& item: elements) {
            auto og = item->resolved_object()->getObjGen();
            result += og.isIndirect() ? og.unparse(' ') + " R " : item->unparse() + " ";
        }
    }
    result += "]";
    return result;
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

QPDF_Array*
Array::array() const
{
    if (obj) {
        if (auto a = obj->as<QPDF_Array>()) {
            return a;
        }
    }
    throw std::runtime_error("Expected an array but found a non-array object");
    return nullptr; // unreachable
}

QPDFObjectHandle
Array::null() const
{
    return null_oh;
}

int
Array::size() const
{
    auto a = array();
    return a->sp ? a->sp->size : int(a->elements.size());
}

std::pair<bool, QPDFObjectHandle>
Array::at(int n) const
{
    auto a = array();
    if (n < 0 || n >= size()) {
        return {false, {}};
    }
    if (!a->sp) {
        return {true, a->elements[size_t(n)]};
    }
    auto const& iter = a->sp->elements.find(n);
    return {true, iter == a->sp->elements.end() ? null() : iter->second};
}

std::vector<QPDFObjectHandle>
Array::getAsVector() const
{
    auto a = array();
    if (a->sp) {
        std::vector<QPDFObjectHandle> v;
        v.reserve(size_t(size()));
        for (auto const& item: a->sp->elements) {
            v.resize(size_t(item.first), null_oh);
            v.emplace_back(item.second);
        }
        v.resize(size_t(size()), null_oh);
        return v;
    } else {
        return {a->elements.cbegin(), a->elements.cend()};
    }
}

bool
Array::setAt(int at, QPDFObjectHandle const& oh)
{
    if (at < 0 || at >= size()) {
        return false;
    }
    auto a = array();
    checkOwnership(oh);
    if (a->sp) {
        a->sp->elements[at] = oh.getObj();
    } else {
        a->elements[size_t(at)] = oh.getObj();
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

void
Array::setFromVector(std::vector<QPDFObjectHandle> const& v)
{
    auto a = array();
    a->elements.resize(0);
    a->elements.reserve(v.size());
    for (auto const& item: v) {
        checkOwnership(item);
        a->elements.push_back(item.getObj());
    }
}

bool
Array::insert(int at, QPDFObjectHandle const& item)
{
    auto a = array();
    int sz = size();
    if (at < 0 || at > sz) {
        // As special case, also allow insert beyond the end
        return false;
    } else if (at == sz) {
        push_back(item);
    } else {
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
            a->elements.insert(a->elements.cbegin() + at, item.getObj());
        }
    }
    return true;
}

void
Array::push_back(QPDFObjectHandle const& item)
{
    auto a = array();
    checkOwnership(item);
    if (a->sp) {
        a->sp->elements[(a->sp->size)++] = item.getObj();
    } else {
        a->elements.push_back(item.getObj());
    }
}

bool
Array::erase(int at)
{
    auto a = array();
    if (at < 0 || at >= size()) {
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
        a->elements.erase(a->elements.cbegin() + at);
    }
    return true;
}

int
QPDFObjectHandle::getArrayNItems() const
{
    if (auto array = as_array(strict)) {
        return array.size();
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
    auto result = (array && at < array.size() && at >= 0) ? array.at(at).second : newNull();
    eraseItem(at);
    return result;
}
