#include <qpdf/QPDFObjectHandle.hh>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFMatrix.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFParser.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Bool.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_InlineImage.hh>
#include <qpdf/QPDF_Integer.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Operator.hh>
#include <qpdf/QPDF_Real.hh>
#include <qpdf/QPDF_Reserved.hh>
#include <qpdf/QPDF_Stream.hh>
#include <qpdf/QPDF_String.hh>
#include <qpdf/QPDF_Unresolved.hh>
#include <qpdf/SparseOHArray.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <algorithm>
#include <cstring>
#include <ctype.h>
#include <limits.h>
#include <stdexcept>
#include <stdlib.h>

namespace
{
    class TerminateParsing
    {
    };
} // namespace

QPDFObjectHandle::StreamDataProvider::StreamDataProvider(bool supports_retry) :
    supports_retry(supports_retry)
{
}

QPDFObjectHandle::StreamDataProvider::~StreamDataProvider()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
}

void
QPDFObjectHandle::StreamDataProvider::provideStreamData(
    QPDFObjGen const& og, Pipeline* pipeline)
{
    return provideStreamData(og.getObj(), og.getGen(), pipeline);
}

bool
QPDFObjectHandle::StreamDataProvider::provideStreamData(
    QPDFObjGen const& og,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    return provideStreamData(
        og.getObj(), og.getGen(), pipeline, suppress_warnings, will_retry);
}

void
QPDFObjectHandle::StreamDataProvider::provideStreamData(
    int objid, int generation, Pipeline* pipeline)
{
    throw std::logic_error(
        "you must override provideStreamData -- see QPDFObjectHandle.hh");
}

bool
QPDFObjectHandle::StreamDataProvider::provideStreamData(
    int objid,
    int generation,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    throw std::logic_error(
        "you must override provideStreamData -- see QPDFObjectHandle.hh");
    return false;
}

bool
QPDFObjectHandle::StreamDataProvider::supportsRetry()
{
    return this->supports_retry;
}

namespace
{
    class CoalesceProvider: public QPDFObjectHandle::StreamDataProvider
    {
      public:
        CoalesceProvider(
            QPDFObjectHandle containing_page, QPDFObjectHandle old_contents) :
            containing_page(containing_page),
            old_contents(old_contents)
        {
        }
        virtual ~CoalesceProvider() = default;
        virtual void provideStreamData(QPDFObjGen const&, Pipeline* pipeline);

      private:
        QPDFObjectHandle containing_page;
        QPDFObjectHandle old_contents;
    };
} // namespace

void
CoalesceProvider::provideStreamData(QPDFObjGen const&, Pipeline* p)
{
    QTC::TC("qpdf", "QPDFObjectHandle coalesce provide stream data");
    std::string description =
        "page object " + containing_page.getObjGen().unparse(' ');
    std::string all_description;
    old_contents.pipeContentStreams(p, description, all_description);
}

void
QPDFObjectHandle::TokenFilter::handleEOF()
{
}

void
QPDFObjectHandle::TokenFilter::setPipeline(Pipeline* p)
{
    this->pipeline = p;
}

void
QPDFObjectHandle::TokenFilter::write(char const* data, size_t len)
{
    if (!this->pipeline) {
        return;
    }
    if (len) {
        this->pipeline->write(data, len);
    }
}

void
QPDFObjectHandle::TokenFilter::write(std::string const& str)
{
    write(str.c_str(), str.length());
}

void
QPDFObjectHandle::TokenFilter::writeToken(QPDFTokenizer::Token const& token)
{
    std::string value = token.getRawValue();
    write(value.c_str(), value.length());
}

void
QPDFObjectHandle::ParserCallbacks::handleObject(QPDFObjectHandle)
{
    throw std::logic_error("You must override one of the"
                           " handleObject methods in ParserCallbacks");
}

void
QPDFObjectHandle::ParserCallbacks::handleObject(
    QPDFObjectHandle oh, size_t, size_t)
{
    // This version of handleObject was added in qpdf 9. If the
    // developer did not override it, fall back to the older
    // interface.
    handleObject(oh);
}

void
QPDFObjectHandle::ParserCallbacks::contentSize(size_t)
{
    // Ignore by default; overriding this is optional.
}

void
QPDFObjectHandle::ParserCallbacks::terminateParsing()
{
    throw TerminateParsing();
}

namespace
{
    class LastChar: public Pipeline
    {
      public:
        LastChar(Pipeline* next);
        virtual ~LastChar() = default;
        virtual void write(unsigned char const* data, size_t len);
        virtual void finish();
        unsigned char getLastChar();

      private:
        unsigned char last_char;
    };
} // namespace

LastChar::LastChar(Pipeline* next) :
    Pipeline("lastchar", next),
    last_char(0)
{
}

void
LastChar::write(unsigned char const* data, size_t len)
{
    if (len > 0) {
        this->last_char = data[len - 1];
    }
    getNext()->write(data, len);
}

void
LastChar::finish()
{
    getNext()->finish();
}

unsigned char
LastChar::getLastChar()
{
    return this->last_char;
}

bool
QPDFObjectHandle::isSameObjectAs(QPDFObjectHandle const& rhs) const
{
    return this->obj == rhs.obj;
}

void
QPDFObjectHandle::disconnect()
{
    // Recursively remove association with any QPDF object. This
    // method may only be called during final destruction.
    // QPDF::~QPDF() calls it for indirect objects using the object
    // pointer itself, so we don't do that here. Other objects call it
    // through this method.
    if (!isIndirect()) {
        this->obj->disconnect();
    }
}

qpdf_object_type_e
QPDFObjectHandle::getTypeCode()
{
    return dereference() ? this->obj->getTypeCode() : ::ot_uninitialized;
}

char const*
QPDFObjectHandle::getTypeName()
{
    return dereference() ? this->obj->getTypeName() : "uninitialized";
}

QPDF_Array*
QPDFObjectHandle::asArray()
{
    return dereference() ? obj->as<QPDF_Array>() : nullptr;
}

QPDF_Bool*
QPDFObjectHandle::asBool()
{
    return dereference() ? obj->as<QPDF_Bool>() : nullptr;
}

QPDF_Dictionary*
QPDFObjectHandle::asDictionary()
{
    return dereference() ? obj->as<QPDF_Dictionary>() : nullptr;
}

QPDF_InlineImage*
QPDFObjectHandle::asInlineImage()
{
    return dereference() ? obj->as<QPDF_InlineImage>() : nullptr;
}

QPDF_Integer*
QPDFObjectHandle::asInteger()
{
    return dereference() ? obj->as<QPDF_Integer>() : nullptr;
}

QPDF_Name*
QPDFObjectHandle::asName()
{
    return dereference() ? obj->as<QPDF_Name>() : nullptr;
}

QPDF_Null*
QPDFObjectHandle::asNull()
{
    return dereference() ? obj->as<QPDF_Null>() : nullptr;
}

QPDF_Operator*
QPDFObjectHandle::asOperator()
{
    return dereference() ? obj->as<QPDF_Operator>() : nullptr;
}

QPDF_Real*
QPDFObjectHandle::asReal()
{
    return dereference() ? obj->as<QPDF_Real>() : nullptr;
}

QPDF_Reserved*
QPDFObjectHandle::asReserved()
{
    return dereference() ? obj->as<QPDF_Reserved>() : nullptr;
}

QPDF_Stream*
QPDFObjectHandle::asStream()
{
    return dereference() ? obj->as<QPDF_Stream>() : nullptr;
}

QPDF_Stream*
QPDFObjectHandle::asStreamWithAssert()
{
    auto stream = asStream();
    assertType("stream", stream);
    return stream;
}

QPDF_String*
QPDFObjectHandle::asString()
{
    return dereference() ? obj->as<QPDF_String>() : nullptr;
}

bool
QPDFObjectHandle::isDestroyed()
{
    return dereference() && (obj->getTypeCode() == ::ot_destroyed);
}

bool
QPDFObjectHandle::isBool()
{
    return dereference() && (obj->getTypeCode() == ::ot_boolean);
}

bool
QPDFObjectHandle::isDirectNull() const
{
    // Don't call dereference() -- this is a const method, and we know
    // objid == 0, so there's nothing to resolve.
    return (
        isInitialized() && (getObjectID() == 0) &&
        (obj->getTypeCode() == ::ot_null));
}

bool
QPDFObjectHandle::isNull()
{
    return dereference() && (obj->getTypeCode() == ::ot_null);
}

bool
QPDFObjectHandle::isInteger()
{
    return dereference() && (obj->getTypeCode() == ::ot_integer);
}

bool
QPDFObjectHandle::isReal()
{
    return dereference() && (obj->getTypeCode() == ::ot_real);
}

bool
QPDFObjectHandle::isNumber()
{
    return (isInteger() || isReal());
}

double
QPDFObjectHandle::getNumericValue()
{
    double result = 0.0;
    if (isInteger()) {
        result = static_cast<double>(getIntValue());
    } else if (isReal()) {
        result = atof(getRealValue().c_str());
    } else {
        typeWarning("number", "returning 0");
        QTC::TC("qpdf", "QPDFObjectHandle numeric non-numeric");
    }
    return result;
}

bool
QPDFObjectHandle::getValueAsNumber(double& value)
{
    if (!isNumber()) {
        return false;
    }
    value = getNumericValue();
    return true;
}

bool
QPDFObjectHandle::isName()
{
    return dereference() && (obj->getTypeCode() == ::ot_name);
}

bool
QPDFObjectHandle::isString()
{
    return dereference() && (obj->getTypeCode() == ::ot_string);
}

bool
QPDFObjectHandle::isOperator()
{
    return dereference() && (obj->getTypeCode() == ::ot_operator);
}

bool
QPDFObjectHandle::isInlineImage()
{
    return dereference() && (obj->getTypeCode() == ::ot_inlineimage);
}

bool
QPDFObjectHandle::isArray()
{
    return dereference() && (obj->getTypeCode() == ::ot_array);
}

bool
QPDFObjectHandle::isDictionary()
{
    return dereference() && (obj->getTypeCode() == ::ot_dictionary);
}

bool
QPDFObjectHandle::isStream()
{
    return dereference() && (obj->getTypeCode() == ::ot_stream);
}

bool
QPDFObjectHandle::isReserved()
{
    return dereference() && (obj->getTypeCode() == ::ot_reserved);
}

bool
QPDFObjectHandle::isScalar()
{
    return (
        !(isArray() || isDictionary() || isStream() || isOperator() ||
          isInlineImage()));
}

bool
QPDFObjectHandle::isNameAndEquals(std::string const& name)
{
    return isName() && (getName() == name);
}

bool
QPDFObjectHandle::isDictionaryOfType(
    std::string const& type, std::string const& subtype)
{
    return isDictionary() &&
        (type.empty() || getKey("/Type").isNameAndEquals(type)) &&
        (subtype.empty() || getKey("/Subtype").isNameAndEquals(subtype));
}

bool
QPDFObjectHandle::isStreamOfType(
    std::string const& type, std::string const& subtype)
{
    return isStream() && getDict().isDictionaryOfType(type, subtype);
}

// Bool accessors

bool
QPDFObjectHandle::getBoolValue()
{
    auto boolean = asBool();
    if (boolean) {
        return boolean->getVal();
    } else {
        typeWarning("boolean", "returning false");
        QTC::TC("qpdf", "QPDFObjectHandle boolean returning false");
        return false;
    }
}

bool
QPDFObjectHandle::getValueAsBool(bool& value)
{
    auto boolean = asBool();
    if (boolean == nullptr) {
        return false;
    }
    value = boolean->getVal();
    return true;
}

// Integer accessors

long long
QPDFObjectHandle::getIntValue()
{
    auto integer = asInteger();
    if (integer) {
        return integer->getVal();
    } else {
        typeWarning("integer", "returning 0");
        QTC::TC("qpdf", "QPDFObjectHandle integer returning 0");
        return 0;
    }
}

bool
QPDFObjectHandle::getValueAsInt(long long& value)
{
    auto integer = asInteger();
    if (integer == nullptr) {
        return false;
    }
    value = integer->getVal();
    return true;
}

int
QPDFObjectHandle::getIntValueAsInt()
{
    int result = 0;
    long long v = getIntValue();
    if (v < INT_MIN) {
        QTC::TC("qpdf", "QPDFObjectHandle int returning INT_MIN");
        warnIfPossible(
            "requested value of integer is too small; returning INT_MIN");
        result = INT_MIN;
    } else if (v > INT_MAX) {
        QTC::TC("qpdf", "QPDFObjectHandle int returning INT_MAX");
        warnIfPossible(
            "requested value of integer is too big; returning INT_MAX");
        result = INT_MAX;
    } else {
        result = static_cast<int>(v);
    }
    return result;
}

bool
QPDFObjectHandle::getValueAsInt(int& value)
{
    if (!isInteger()) {
        return false;
    }
    value = getIntValueAsInt();
    return true;
}

unsigned long long
QPDFObjectHandle::getUIntValue()
{
    unsigned long long result = 0;
    long long v = getIntValue();
    if (v < 0) {
        QTC::TC("qpdf", "QPDFObjectHandle uint returning 0");
        warnIfPossible(
            "unsigned value request for negative number; returning 0");
    } else {
        result = static_cast<unsigned long long>(v);
    }
    return result;
}

bool
QPDFObjectHandle::getValueAsUInt(unsigned long long& value)
{
    if (!isInteger()) {
        return false;
    }
    value = getUIntValue();
    return true;
}

unsigned int
QPDFObjectHandle::getUIntValueAsUInt()
{
    unsigned int result = 0;
    long long v = getIntValue();
    if (v < 0) {
        QTC::TC("qpdf", "QPDFObjectHandle uint uint returning 0");
        warnIfPossible(
            "unsigned integer value request for negative number; returning 0");
        result = 0;
    } else if (v > UINT_MAX) {
        QTC::TC("qpdf", "QPDFObjectHandle uint returning UINT_MAX");
        warnIfPossible("requested value of unsigned integer is too big;"
                       " returning UINT_MAX");
        result = UINT_MAX;
    } else {
        result = static_cast<unsigned int>(v);
    }
    return result;
}

bool
QPDFObjectHandle::getValueAsUInt(unsigned int& value)
{
    if (!isInteger()) {
        return false;
    }
    value = getUIntValueAsUInt();
    return true;
}

// Real accessors

std::string
QPDFObjectHandle::getRealValue()
{
    auto real = asReal();
    if (real) {
        return real->getVal();
    } else {
        typeWarning("real", "returning 0.0");
        QTC::TC("qpdf", "QPDFObjectHandle real returning 0.0");
        return "0.0";
    }
}

bool
QPDFObjectHandle::getValueAsReal(std::string& value)
{
    auto real = asReal();
    if (real == nullptr) {
        return false;
    }
    value = real->getVal();
    return true;
}

// Name accessors

std::string
QPDFObjectHandle::getName()
{
    auto name = asName();
    if (name) {
        return name->getName();
    } else {
        typeWarning("name", "returning dummy name");
        QTC::TC("qpdf", "QPDFObjectHandle name returning dummy name");
        return "/QPDFFakeName";
    }
}

bool
QPDFObjectHandle::getValueAsName(std::string& value)
{
    auto name = asName();
    if (name == nullptr) {
        return false;
    }
    value = name->getName();
    return true;
}

// String accessors

std::string
QPDFObjectHandle::getStringValue()
{
    auto str = asString();
    if (str) {
        return str->getVal();
    } else {
        typeWarning("string", "returning empty string");
        QTC::TC("qpdf", "QPDFObjectHandle string returning empty string");
        return "";
    }
}

bool
QPDFObjectHandle::getValueAsString(std::string& value)
{
    auto str = asString();
    if (str == nullptr) {
        return false;
    }
    value = str->getVal();
    return true;
}

std::string
QPDFObjectHandle::getUTF8Value()
{
    auto str = asString();
    if (str) {
        return str->getUTF8Val();
    } else {
        typeWarning("string", "returning empty string");
        QTC::TC("qpdf", "QPDFObjectHandle string returning empty utf8");
        return "";
    }
}

bool
QPDFObjectHandle::getValueAsUTF8(std::string& value)
{
    auto str = asString();
    if (str == nullptr) {
        return false;
    }
    value = str->getUTF8Val();
    return true;
}

// Operator and Inline Image accessors

std::string
QPDFObjectHandle::getOperatorValue()
{
    auto op = asOperator();
    if (op) {
        return op->getVal();
    } else {
        typeWarning("operator", "returning fake value");
        QTC::TC("qpdf", "QPDFObjectHandle operator returning fake value");
        return "QPDFFAKE";
    }
}

bool
QPDFObjectHandle::getValueAsOperator(std::string& value)
{
    auto op = asOperator();
    if (op == nullptr) {
        return false;
    }
    value = op->getVal();
    return true;
}

std::string
QPDFObjectHandle::getInlineImageValue()
{
    auto image = asInlineImage();
    if (image) {
        return image->getVal();
    } else {
        typeWarning("inlineimage", "returning empty data");
        QTC::TC("qpdf", "QPDFObjectHandle inlineimage returning empty data");
        return "";
    }
}

bool
QPDFObjectHandle::getValueAsInlineImage(std::string& value)
{
    auto image = asInlineImage();
    if (image == nullptr) {
        return false;
    }
    value = image->getVal();
    return true;
}

// Array accessors

QPDFObjectHandle::QPDFArrayItems
QPDFObjectHandle::aitems()
{
    return QPDFArrayItems(*this);
}

int
QPDFObjectHandle::getArrayNItems()
{
    auto array = asArray();
    if (array) {
        return array->getNItems();
    } else {
        typeWarning("array", "treating as empty");
        QTC::TC("qpdf", "QPDFObjectHandle array treating as empty");
        return 0;
    }
}

QPDFObjectHandle
QPDFObjectHandle::getArrayItem(int n)
{
    QPDFObjectHandle result;
    auto array = asArray();
    if (array && (n < array->getNItems()) && (n >= 0)) {
        result = array->getItem(n);
    } else {
        result = newNull();
        if (array) {
            objectWarning("returning null for out of bounds array access");
            QTC::TC("qpdf", "QPDFObjectHandle array bounds");
        } else {
            typeWarning("array", "returning null");
            QTC::TC("qpdf", "QPDFObjectHandle array null for non-array");
        }
        QPDF* context = nullptr;
        std::string description;
        if (obj->getDescription(context, description)) {
            result.setObjectDescription(
                context,
                description + " -> null returned from invalid array access");
        }
    }
    return result;
}

bool
QPDFObjectHandle::isRectangle()
{
    auto array = asArray();
    if ((array == nullptr) || (array->getNItems() != 4)) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        if (!array->getItem(i).isNumber()) {
            return false;
        }
    }
    return true;
}

bool
QPDFObjectHandle::isMatrix()
{
    auto array = asArray();
    if ((array == nullptr) || (array->getNItems() != 6)) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        if (!array->getItem(i).isNumber()) {
            return false;
        }
    }
    return true;
}

QPDFObjectHandle::Rectangle
QPDFObjectHandle::getArrayAsRectangle()
{
    Rectangle result;
    if (isRectangle()) {
        auto array = asArray();
        // Rectangle coordinates are always supposed to be llx, lly,
        // urx, ury, but files have been found in the wild where
        // llx > urx or lly > ury.
        double i0 = array->getItem(0).getNumericValue();
        double i1 = array->getItem(1).getNumericValue();
        double i2 = array->getItem(2).getNumericValue();
        double i3 = array->getItem(3).getNumericValue();
        result = Rectangle(
            std::min(i0, i2),
            std::min(i1, i3),
            std::max(i0, i2),
            std::max(i1, i3));
    }
    return result;
}

QPDFObjectHandle::Matrix
QPDFObjectHandle::getArrayAsMatrix()
{
    Matrix result;
    if (isMatrix()) {
        auto array = asArray();
        result = Matrix(
            array->getItem(0).getNumericValue(),
            array->getItem(1).getNumericValue(),
            array->getItem(2).getNumericValue(),
            array->getItem(3).getNumericValue(),
            array->getItem(4).getNumericValue(),
            array->getItem(5).getNumericValue());
    }
    return result;
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getArrayAsVector()
{
    std::vector<QPDFObjectHandle> result;
    auto array = asArray();
    if (array) {
        array->getAsVector(result);
    } else {
        typeWarning("array", "treating as empty");
        QTC::TC("qpdf", "QPDFObjectHandle array treating as empty vector");
    }
    return result;
}

// Array mutators

void
QPDFObjectHandle::setArrayItem(int n, QPDFObjectHandle const& item)
{
    auto array = asArray();
    if (array) {
        checkOwnership(item);
        array->setItem(n, item);
    } else {
        typeWarning("array", "ignoring attempt to set item");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring set item");
    }
}

void
QPDFObjectHandle::setArrayFromVector(std::vector<QPDFObjectHandle> const& items)
{
    auto array = asArray();
    if (array) {
        for (auto const& item: items) {
            checkOwnership(item);
        }
        array->setFromVector(items);
    } else {
        typeWarning("array", "ignoring attempt to replace items");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring replace items");
    }
}

void
QPDFObjectHandle::insertItem(int at, QPDFObjectHandle const& item)
{
    auto array = asArray();
    if (array) {
        array->insertItem(at, item);
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
    auto array = asArray();
    if (array) {
        checkOwnership(item);
        array->appendItem(item);
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
    auto array = asArray();
    if (array && (at < array->getNItems()) && (at >= 0)) {
        array->eraseItem(at);
    } else {
        if (array) {
            objectWarning("ignoring attempt to erase out of bounds array item");
            QTC::TC("qpdf", "QPDFObjectHandle erase array bounds");
        } else {
            typeWarning("array", "ignoring attempt to erase item");
            QTC::TC("qpdf", "QPDFObjectHandle array ignoring erase item");
        }
    }
}

QPDFObjectHandle
QPDFObjectHandle::eraseItemAndGetOld(int at)
{
    auto result = QPDFObjectHandle::newNull();
    auto array = asArray();
    if (array && (at < array->getNItems()) && (at >= 0)) {
        result = array->getItem(at);
    }
    eraseItem(at);
    return result;
}

// Dictionary accessors

QPDFObjectHandle::QPDFDictItems
QPDFObjectHandle::ditems()
{
    return QPDFDictItems(*this);
}

bool
QPDFObjectHandle::hasKey(std::string const& key)
{
    auto dict = asDictionary();
    if (dict) {
        return dict->hasKey(key);
    } else {
        typeWarning(
            "dictionary", "returning false for a key containment request");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary false for hasKey");
        return false;
    }
}

QPDFObjectHandle
QPDFObjectHandle::getKey(std::string const& key)
{
    QPDFObjectHandle result;
    auto dict = asDictionary();
    if (dict) {
        result = dict->getKey(key);
    } else {
        typeWarning("dictionary", "returning null for attempted key retrieval");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary null for getKey");
        result = newNull();
        QPDF* qpdf = nullptr;
        std::string description;
        if (obj->getDescription(qpdf, description)) {
            result.setObjectDescription(
                qpdf,
                (description + " -> null returned from getting key " + key +
                 " from non-Dictionary"));
        }
    }
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::getKeyIfDict(std::string const& key)
{
    return isNull() ? newNull() : getKey(key);
}

std::set<std::string>
QPDFObjectHandle::getKeys()
{
    std::set<std::string> result;
    auto dict = asDictionary();
    if (dict) {
        result = dict->getKeys();
    } else {
        typeWarning("dictionary", "treating as empty");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary empty set for getKeys");
    }
    return result;
}

std::map<std::string, QPDFObjectHandle>
QPDFObjectHandle::getDictAsMap()
{
    std::map<std::string, QPDFObjectHandle> result;
    auto dict = asDictionary();
    if (dict) {
        result = dict->getAsMap();
    } else {
        typeWarning("dictionary", "treating as empty");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary empty map for asMap");
    }
    return result;
}

// Array and Name accessors
bool
QPDFObjectHandle::isOrHasName(std::string const& value)
{
    if (isNameAndEquals(value)) {
        return true;
    } else if (isArray()) {
        for (auto& item: aitems()) {
            if (item.isNameAndEquals(value)) {
                return true;
            }
        }
    }
    return false;
}

void
QPDFObjectHandle::makeResourcesIndirect(QPDF& owning_qpdf)
{
    if (!isDictionary()) {
        return;
    }
    for (auto const& i1: ditems()) {
        QPDFObjectHandle sub = i1.second;
        if (!sub.isDictionary()) {
            continue;
        }
        for (auto i2: sub.ditems()) {
            std::string const& key = i2.first;
            QPDFObjectHandle val = i2.second;
            if (!val.isIndirect()) {
                sub.replaceKey(key, owning_qpdf.makeIndirectObject(val));
            }
        }
    }
}

void
QPDFObjectHandle::mergeResources(
    QPDFObjectHandle other,
    std::map<std::string, std::map<std::string, std::string>>* conflicts)
{
    if (!(isDictionary() && other.isDictionary())) {
        QTC::TC("qpdf", "QPDFObjectHandle merge top type mismatch");
        return;
    }

    auto make_og_to_name = [](QPDFObjectHandle& dict,
                              std::map<QPDFObjGen, std::string>& og_to_name) {
        for (auto i: dict.ditems()) {
            if (i.second.isIndirect()) {
                og_to_name[i.second.getObjGen()] = i.first;
            }
        }
    };

    // This algorithm is described in comments in QPDFObjectHandle.hh
    // above the declaration of mergeResources.
    for (auto o_top: other.ditems()) {
        std::string const& rtype = o_top.first;
        QPDFObjectHandle other_val = o_top.second;
        if (hasKey(rtype)) {
            QPDFObjectHandle this_val = getKey(rtype);
            if (this_val.isDictionary() && other_val.isDictionary()) {
                if (this_val.isIndirect()) {
                    // Do this even if there are no keys. Various
                    // places in the code call mergeResources with
                    // resource dictionaries that contain empty
                    // subdictionaries just to get this shallow copy
                    // functionality.
                    QTC::TC("qpdf", "QPDFObjectHandle replace with copy");
                    this_val =
                        replaceKeyAndGetNew(rtype, this_val.shallowCopy());
                }
                std::map<QPDFObjGen, std::string> og_to_name;
                std::set<std::string> rnames;
                int min_suffix = 1;
                bool initialized_maps = false;
                for (auto ov_iter: other_val.ditems()) {
                    std::string const& key = ov_iter.first;
                    QPDFObjectHandle rval = ov_iter.second;
                    if (!this_val.hasKey(key)) {
                        if (!rval.isIndirect()) {
                            QTC::TC(
                                "qpdf", "QPDFObjectHandle merge shallow copy");
                            rval = rval.shallowCopy();
                        }
                        this_val.replaceKey(key, rval);
                    } else if (conflicts) {
                        if (!initialized_maps) {
                            make_og_to_name(this_val, og_to_name);
                            rnames = this_val.getResourceNames();
                            initialized_maps = true;
                        }
                        auto rval_og = rval.getObjGen();
                        if (rval.isIndirect() && og_to_name.count(rval_og)) {
                            QTC::TC("qpdf", "QPDFObjectHandle merge reuse");
                            auto new_key = og_to_name[rval_og];
                            if (new_key != key) {
                                (*conflicts)[rtype][key] = new_key;
                            }
                        } else {
                            QTC::TC("qpdf", "QPDFObjectHandle merge generate");
                            std::string new_key = getUniqueResourceName(
                                key + "_", min_suffix, &rnames);
                            (*conflicts)[rtype][key] = new_key;
                            this_val.replaceKey(new_key, rval);
                        }
                    }
                }
            } else if (this_val.isArray() && other_val.isArray()) {
                std::set<std::string> scalars;
                for (auto this_item: this_val.aitems()) {
                    if (this_item.isScalar()) {
                        scalars.insert(this_item.unparse());
                    }
                }
                for (auto other_item: other_val.aitems()) {
                    if (other_item.isScalar()) {
                        if (scalars.count(other_item.unparse()) == 0) {
                            QTC::TC("qpdf", "QPDFObjectHandle merge array");
                            this_val.appendItem(other_item);
                        } else {
                            QTC::TC("qpdf", "QPDFObjectHandle merge array dup");
                        }
                    }
                }
            }
        } else {
            QTC::TC("qpdf", "QPDFObjectHandle merge copy from other");
            replaceKey(rtype, other_val.shallowCopy());
        }
    }
}

std::set<std::string>
QPDFObjectHandle::getResourceNames()
{
    // Return second-level dictionary keys
    std::set<std::string> result;
    if (!isDictionary()) {
        return result;
    }
    for (auto const& key: getKeys()) {
        QPDFObjectHandle val = getKey(key);
        if (val.isDictionary()) {
            for (auto const& val_key: val.getKeys()) {
                result.insert(val_key);
            }
        }
    }
    return result;
}

std::string
QPDFObjectHandle::getUniqueResourceName(
    std::string const& prefix, int& min_suffix, std::set<std::string>* namesp)

{
    std::set<std::string> names = (namesp ? *namesp : getResourceNames());
    int max_suffix = min_suffix + QIntC::to_int(names.size());
    while (min_suffix <= max_suffix) {
        std::string candidate = prefix + std::to_string(min_suffix);
        if (names.count(candidate) == 0) {
            return candidate;
        }
        // Increment after return; min_suffix should be the value
        // used, not the next value.
        ++min_suffix;
    }
    // This could only happen if there is a coding error.
    // The number of candidates we test is more than the
    // number of keys we're checking against.
    throw std::logic_error("unable to find unconflicting name in"
                           " QPDFObjectHandle::getUniqueResourceName");
}

// Dictionary mutators

void
QPDFObjectHandle::replaceKey(
    std::string const& key, QPDFObjectHandle const& value)
{
    auto dict = asDictionary();
    if (dict) {
        checkOwnership(value);
        dict->replaceKey(key, value);
    } else {
        typeWarning("dictionary", "ignoring key replacement request");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary ignoring replaceKey");
    }
}

QPDFObjectHandle
QPDFObjectHandle::replaceKeyAndGetNew(
    std::string const& key, QPDFObjectHandle const& value)
{
    replaceKey(key, value);
    return value;
}

QPDFObjectHandle
QPDFObjectHandle::replaceKeyAndGetOld(
    std::string const& key, QPDFObjectHandle const& value)
{
    QPDFObjectHandle old = removeKeyAndGetOld(key);
    replaceKey(key, value);
    return old;
}

void
QPDFObjectHandle::removeKey(std::string const& key)
{
    auto dict = asDictionary();
    if (dict) {
        dict->removeKey(key);
    } else {
        typeWarning("dictionary", "ignoring key removal request");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary ignoring removeKey");
    }
}

QPDFObjectHandle
QPDFObjectHandle::removeKeyAndGetOld(std::string const& key)
{
    auto result = QPDFObjectHandle::newNull();
    auto dict = asDictionary();
    if (dict) {
        result = dict->getKey(key);
    }
    removeKey(key);
    return result;
}

void
QPDFObjectHandle::replaceOrRemoveKey(
    std::string const& key, QPDFObjectHandle const& value)
{
    replaceKey(key, value);
}

// Stream accessors
QPDFObjectHandle
QPDFObjectHandle::getDict()
{
    return asStreamWithAssert()->getDict();
}

void
QPDFObjectHandle::setFilterOnWrite(bool val)
{
    asStreamWithAssert()->setFilterOnWrite(val);
}

bool
QPDFObjectHandle::getFilterOnWrite()
{
    return asStreamWithAssert()->getFilterOnWrite();
}

bool
QPDFObjectHandle::isDataModified()
{
    return asStreamWithAssert()->isDataModified();
}

void
QPDFObjectHandle::replaceDict(QPDFObjectHandle const& new_dict)
{
    asStreamWithAssert()->replaceDict(new_dict);
}

std::shared_ptr<Buffer>
QPDFObjectHandle::getStreamData(qpdf_stream_decode_level_e level)
{
    return asStreamWithAssert()->getStreamData(level);
}

std::shared_ptr<Buffer>
QPDFObjectHandle::getRawStreamData()
{
    return asStreamWithAssert()->getRawStreamData();
}

bool
QPDFObjectHandle::pipeStreamData(
    Pipeline* p,
    bool* filtering_attempted,
    int encode_flags,
    qpdf_stream_decode_level_e decode_level,
    bool suppress_warnings,
    bool will_retry)
{
    return asStreamWithAssert()->pipeStreamData(
        p,
        filtering_attempted,
        encode_flags,
        decode_level,
        suppress_warnings,
        will_retry);
}

bool
QPDFObjectHandle::pipeStreamData(
    Pipeline* p,
    int encode_flags,
    qpdf_stream_decode_level_e decode_level,
    bool suppress_warnings,
    bool will_retry)
{
    bool filtering_attempted;
    asStreamWithAssert()->pipeStreamData(
        p,
        &filtering_attempted,
        encode_flags,
        decode_level,
        suppress_warnings,
        will_retry);
    return filtering_attempted;
}

bool
QPDFObjectHandle::pipeStreamData(
    Pipeline* p, bool filter, bool normalize, bool compress)
{
    int encode_flags = 0;
    qpdf_stream_decode_level_e decode_level = qpdf_dl_none;
    if (filter) {
        decode_level = qpdf_dl_generalized;
        if (normalize) {
            encode_flags |= qpdf_ef_normalize;
        }
        if (compress) {
            encode_flags |= qpdf_ef_compress;
        }
    }
    return pipeStreamData(p, encode_flags, decode_level, false);
}

void
QPDFObjectHandle::replaceStreamData(
    std::shared_ptr<Buffer> data,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    asStreamWithAssert()->replaceStreamData(data, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(
    std::string const& data,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    auto b = std::make_shared<Buffer>(data.length());
    unsigned char* bp = b->getBuffer();
    if (bp) {
        memcpy(bp, data.c_str(), data.length());
    }
    asStreamWithAssert()->replaceStreamData(b, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(
    std::shared_ptr<StreamDataProvider> provider,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    asStreamWithAssert()->replaceStreamData(provider, filter, decode_parms);
}

namespace
{
    class FunctionProvider: public QPDFObjectHandle::StreamDataProvider
    {
      public:
        FunctionProvider(std::function<void(Pipeline*)> provider) :
            StreamDataProvider(false),
            p1(provider),
            p2(nullptr)
        {
        }
        FunctionProvider(std::function<bool(Pipeline*, bool, bool)> provider) :
            StreamDataProvider(true),
            p1(nullptr),
            p2(provider)
        {
        }

        virtual void
        provideStreamData(QPDFObjGen const&, Pipeline* pipeline) override
        {
            p1(pipeline);
        }

        virtual bool
        provideStreamData(
            QPDFObjGen const&,
            Pipeline* pipeline,
            bool suppress_warnings,
            bool will_retry) override
        {
            return p2(pipeline, suppress_warnings, will_retry);
        }

      private:
        std::function<void(Pipeline*)> p1;
        std::function<bool(Pipeline*, bool, bool)> p2;
    };
} // namespace

void
QPDFObjectHandle::replaceStreamData(
    std::function<void(Pipeline*)> provider,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    auto sdp =
        std::shared_ptr<StreamDataProvider>(new FunctionProvider(provider));
    asStreamWithAssert()->replaceStreamData(sdp, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(
    std::function<bool(Pipeline*, bool, bool)> provider,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    auto sdp =
        std::shared_ptr<StreamDataProvider>(new FunctionProvider(provider));
    asStreamWithAssert()->replaceStreamData(sdp, filter, decode_parms);
}

std::map<std::string, QPDFObjectHandle>
QPDFObjectHandle::getPageImages()
{
    return QPDFPageObjectHelper(*this).getImages();
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::arrayOrStreamToStreamArray(
    std::string const& description, std::string& all_description)
{
    all_description = description;
    std::vector<QPDFObjectHandle> result;
    auto array = asArray();
    if (array) {
        int n_items = array->getNItems();
        for (int i = 0; i < n_items; ++i) {
            QPDFObjectHandle item = array->getItem(i);
            if (item.isStream()) {
                result.push_back(item);
            } else {
                QTC::TC("qpdf", "QPDFObjectHandle non-stream in stream array");
                warn(
                    item.getOwningQPDF(),
                    QPDFExc(
                        qpdf_e_damaged_pdf,
                        "",
                        description + ": item index " + std::to_string(i) +
                            " (from 0)",
                        0,
                        "ignoring non-stream in an array of streams"));
            }
        }
    } else if (isStream()) {
        result.push_back(*this);
    } else if (!isNull()) {
        warn(
            getOwningQPDF(),
            QPDFExc(
                qpdf_e_damaged_pdf,
                "",
                description,
                0,
                " object is supposed to be a stream or an"
                " array of streams but is neither"));
    }

    bool first = true;
    for (auto const& item: result) {
        if (first) {
            first = false;
        } else {
            all_description += ",";
        }
        all_description += " stream " + item.getObjGen().unparse(' ');
    }

    return result;
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getPageContents()
{
    std::string description = "page object " + getObjGen().unparse(' ');
    std::string all_description;
    return this->getKey("/Contents")
        .arrayOrStreamToStreamArray(description, all_description);
}

void
QPDFObjectHandle::addPageContents(QPDFObjectHandle new_contents, bool first)
{
    new_contents.assertStream();

    std::vector<QPDFObjectHandle> content_streams;
    if (first) {
        QTC::TC("qpdf", "QPDFObjectHandle prepend page contents");
        content_streams.push_back(new_contents);
    }
    for (auto const& iter: getPageContents()) {
        QTC::TC("qpdf", "QPDFObjectHandle append page contents");
        content_streams.push_back(iter);
    }
    if (!first) {
        content_streams.push_back(new_contents);
    }

    this->replaceKey("/Contents", newArray(content_streams));
}

void
QPDFObjectHandle::rotatePage(int angle, bool relative)
{
    if ((angle % 90) != 0) {
        throw std::runtime_error("QPDF::rotatePage called with an"
                                 " angle that is not a multiple of 90");
    }
    int new_angle = angle;
    if (relative) {
        int old_angle = 0;
        bool found_rotate = false;
        QPDFObjectHandle cur_obj = *this;
        bool searched_parent = false;
        std::set<QPDFObjGen> visited;
        while (!found_rotate) {
            if (visited.count(cur_obj.getObjGen())) {
                // Don't get stuck in an infinite loop
                break;
            }
            if (!visited.empty()) {
                searched_parent = true;
            }
            visited.insert(cur_obj.getObjGen());
            if (cur_obj.getKey("/Rotate").isInteger()) {
                found_rotate = true;
                old_angle = cur_obj.getKey("/Rotate").getIntValueAsInt();
            } else if (cur_obj.getKey("/Parent").isDictionary()) {
                cur_obj = cur_obj.getKey("/Parent");
            } else {
                break;
            }
        }
        QTC::TC(
            "qpdf",
            "QPDFObjectHandle found old angle",
            searched_parent ? 0 : 1);
        if ((old_angle % 90) != 0) {
            old_angle = 0;
        }
        new_angle += old_angle;
    }
    new_angle = (new_angle + 360) % 360;
    // Make this explicit even with new_angle == 0 since /Rotate can
    // be inherited.
    replaceKey("/Rotate", QPDFObjectHandle::newInteger(new_angle));
}

void
QPDFObjectHandle::coalesceContentStreams()
{
    QPDFObjectHandle contents = this->getKey("/Contents");
    if (contents.isStream()) {
        QTC::TC("qpdf", "QPDFObjectHandle coalesce called on stream");
        return;
    } else if (!contents.isArray()) {
        // /Contents is optional for pages, and some very damaged
        // files may have pages that are invalid in other ways.
        return;
    }
    // Should not be possible for a page object to not have an
    // owning PDF unless it was manually constructed in some
    // incorrect way. However, it can happen in a PDF file whose
    // page structure is direct, which is against spec but still
    // possible to hand construct, as in fuzz issue 27393.
    QPDF& qpdf = getQPDF(
        "coalesceContentStreams called on object  with no associated PDF file");

    QPDFObjectHandle new_contents = newStream(&qpdf);
    this->replaceKey("/Contents", new_contents);

    auto provider = std::shared_ptr<StreamDataProvider>(
        new CoalesceProvider(*this, contents));
    new_contents.replaceStreamData(provider, newNull(), newNull());
}

std::string
QPDFObjectHandle::unparse()
{
    std::string result;
    if (this->isIndirect()) {
        result = getObjGen().unparse(' ') + " R";
    } else {
        result = unparseResolved();
    }
    return result;
}

std::string
QPDFObjectHandle::unparseResolved()
{
    if (!dereference()) {
        throw std::logic_error(
            "attempted to dereference an uninitialized QPDFObjectHandle");
    }
    return obj->unparse();
}

std::string
QPDFObjectHandle::unparseBinary()
{
    auto str = asString();
    if (str) {
        return str->unparse(true);
    } else {
        return unparse();
    }
}

// Deprecated versionless getJSON to be removed in qpdf 12
JSON
QPDFObjectHandle::getJSON(bool dereference_indirect)
{
    return getJSON(1, dereference_indirect);
}

JSON
QPDFObjectHandle::getJSON(int json_version, bool dereference_indirect)
{
    if ((!dereference_indirect) && isIndirect()) {
        return JSON::makeString(unparse());
    } else if (!dereference()) {
        throw std::logic_error(
            "attempted to dereference an uninitialized QPDFObjectHandle");
    } else {
        return obj->getJSON(json_version);
    }
}

JSON
QPDFObjectHandle::getStreamJSON(
    int json_version,
    qpdf_json_stream_data_e json_data,
    qpdf_stream_decode_level_e decode_level,
    Pipeline* p,
    std::string const& data_filename)
{
    return asStreamWithAssert()->getStreamJSON(
        json_version, json_data, decode_level, p, data_filename);
}

QPDFObjectHandle
QPDFObjectHandle::wrapInArray()
{
    if (isArray()) {
        return *this;
    }
    QPDFObjectHandle result = QPDFObjectHandle::newArray();
    result.appendItem(*this);
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::parse(
    std::string const& object_str, std::string const& object_description)
{
    return parse(nullptr, object_str, object_description);
}

QPDFObjectHandle
QPDFObjectHandle::parse(
    QPDF* context,
    std::string const& object_str,
    std::string const& object_description)
{
    auto input = std::shared_ptr<InputSource>(
        new BufferInputSource("parsed object", object_str));
    QPDFTokenizer tokenizer;
    bool empty = false;
    QPDFObjectHandle result =
        parse(input, object_description, tokenizer, empty, nullptr, context);
    size_t offset = QIntC::to_size(input->tell());
    while (offset < object_str.length()) {
        if (!isspace(object_str.at(offset))) {
            QTC::TC("qpdf", "QPDFObjectHandle trailing data in parse");
            throw QPDFExc(
                qpdf_e_damaged_pdf,
                input->getName(),
                object_description,
                input->getLastOffset(),
                "trailing data found parsing object from string");
        }
        ++offset;
    }
    return result;
}

void
QPDFObjectHandle::pipePageContents(Pipeline* p)
{
    std::string description = "page object " + getObjGen().unparse(' ');
    std::string all_description;
    this->getKey("/Contents")
        .pipeContentStreams(p, description, all_description);
}

void
QPDFObjectHandle::pipeContentStreams(
    Pipeline* p, std::string const& description, std::string& all_description)
{
    std::vector<QPDFObjectHandle> streams =
        arrayOrStreamToStreamArray(description, all_description);
    bool need_newline = false;
    Pl_Buffer buf("concatenated content stream buffer");
    for (auto stream: streams) {
        if (need_newline) {
            buf.writeCStr("\n");
        }
        LastChar lc(&buf);
        if (!stream.pipeStreamData(&lc, 0, qpdf_dl_specialized)) {
            QTC::TC("qpdf", "QPDFObjectHandle errors in parsecontent");
            throw QPDFExc(
                qpdf_e_damaged_pdf,
                "content stream",
                "content stream object " + stream.getObjGen().unparse(' '),
                0,
                "errors while decoding content stream");
        }
        lc.finish();
        need_newline = (lc.getLastChar() != static_cast<unsigned char>('\n'));
        QTC::TC("qpdf", "QPDFObjectHandle need_newline", need_newline ? 0 : 1);
    }
    std::unique_ptr<Buffer> b(buf.getBuffer());
    p->write(b->getBuffer(), b->getSize());
    p->finish();
}

void
QPDFObjectHandle::parsePageContents(ParserCallbacks* callbacks)
{
    std::string description = "page object " + getObjGen().unparse(' ');
    this->getKey("/Contents")
        .parseContentStream_internal(description, callbacks);
}

void
QPDFObjectHandle::parseAsContents(ParserCallbacks* callbacks)
{
    std::string description = "object " + getObjGen().unparse(' ');
    this->parseContentStream_internal(description, callbacks);
}

void
QPDFObjectHandle::filterPageContents(TokenFilter* filter, Pipeline* next)
{
    auto description =
        "token filter for page object " + getObjGen().unparse(' ');
    Pl_QPDFTokenizer token_pipeline(description.c_str(), filter, next);
    this->pipePageContents(&token_pipeline);
}

void
QPDFObjectHandle::filterAsContents(TokenFilter* filter, Pipeline* next)
{
    auto description = "token filter for object " + getObjGen().unparse(' ');
    Pl_QPDFTokenizer token_pipeline(description.c_str(), filter, next);
    this->pipeStreamData(&token_pipeline, 0, qpdf_dl_specialized);
}

void
QPDFObjectHandle::parseContentStream(
    QPDFObjectHandle stream_or_array, ParserCallbacks* callbacks)
{
    stream_or_array.parseContentStream_internal(
        "content stream objects", callbacks);
}

void
QPDFObjectHandle::parseContentStream_internal(
    std::string const& description, ParserCallbacks* callbacks)
{
    Pl_Buffer buf("concatenated stream data buffer");
    std::string all_description;
    pipeContentStreams(&buf, description, all_description);
    auto stream_data = buf.getBufferSharedPointer();
    callbacks->contentSize(stream_data->getSize());
    try {
        parseContentStream_data(
            stream_data, all_description, callbacks, getOwningQPDF());
    } catch (TerminateParsing&) {
        return;
    }
    callbacks->handleEOF();
}

void
QPDFObjectHandle::parseContentStream_data(
    std::shared_ptr<Buffer> stream_data,
    std::string const& description,
    ParserCallbacks* callbacks,
    QPDF* context)
{
    size_t stream_length = stream_data->getSize();
    auto input = std::shared_ptr<InputSource>(
        new BufferInputSource(description, stream_data.get()));
    QPDFTokenizer tokenizer;
    tokenizer.allowEOF();
    bool empty = false;
    while (QIntC::to_size(input->tell()) < stream_length) {
        // Read a token and seek to the beginning. The offset we get
        // from this process is the beginning of the next
        // non-ignorable (space, comment) token. This way, the offset
        // and don't including ignorable content.
        tokenizer.readToken(input, "content", true);
        qpdf_offset_t offset = input->getLastOffset();
        input->seek(offset, SEEK_SET);
        auto obj = QPDFParser(input, "content", tokenizer, nullptr, context)
                       .parse(empty, true);
        if (!obj.isInitialized()) {
            // EOF
            break;
        }
        size_t length = QIntC::to_size(input->tell() - offset);

        callbacks->handleObject(obj, QIntC::to_size(offset), length);
        if (obj.isOperator() && (obj.getOperatorValue() == "ID")) {
            // Discard next character; it is the space after ID that
            // terminated the token.  Read until end of inline image.
            char ch;
            input->read(&ch, 1);
            tokenizer.expectInlineImage(input);
            QPDFTokenizer::Token t =
                tokenizer.readToken(input, description, true);
            offset = input->getLastOffset();
            length = QIntC::to_size(input->tell() - offset);
            if (t.getType() == QPDFTokenizer::tt_bad) {
                QTC::TC("qpdf", "QPDFObjectHandle EOF in inline image");
                warn(
                    context,
                    QPDFExc(
                        qpdf_e_damaged_pdf,
                        input->getName(),
                        "stream data",
                        input->tell(),
                        "EOF found while reading inline image"));
            } else {
                std::string inline_image = t.getValue();
                QTC::TC("qpdf", "QPDFObjectHandle inline image token");
                callbacks->handleObject(
                    QPDFObjectHandle::newInlineImage(inline_image),
                    QIntC::to_size(offset),
                    length);
            }
        }
    }
}

void
QPDFObjectHandle::addContentTokenFilter(std::shared_ptr<TokenFilter> filter)
{
    coalesceContentStreams();
    this->getKey("/Contents").addTokenFilter(filter);
}

void
QPDFObjectHandle::addTokenFilter(std::shared_ptr<TokenFilter> filter)
{
    return asStreamWithAssert()->addTokenFilter(filter);
}

QPDFObjectHandle
QPDFObjectHandle::parse(
    std::shared_ptr<InputSource> input,
    std::string const& object_description,
    QPDFTokenizer& tokenizer,
    bool& empty,
    StringDecrypter* decrypter,
    QPDF* context)
{
    return QPDFParser(input, object_description, tokenizer, decrypter, context)
        .parse(empty, false);
}

qpdf_offset_t
QPDFObjectHandle::getParsedOffset()
{
    if (dereference()) {
        return this->obj->getParsedOffset();
    } else {
        return -1;
    }
}

QPDFObjectHandle
QPDFObjectHandle::newBool(bool value)
{
    return QPDFObjectHandle(QPDF_Bool::create(value));
}

QPDFObjectHandle
QPDFObjectHandle::newNull()
{
    return QPDFObjectHandle(QPDF_Null::create());
}

QPDFObjectHandle
QPDFObjectHandle::newInteger(long long value)
{
    return QPDFObjectHandle(QPDF_Integer::create(value));
}

QPDFObjectHandle
QPDFObjectHandle::newReal(std::string const& value)
{
    return QPDFObjectHandle(QPDF_Real::create(value));
}

QPDFObjectHandle
QPDFObjectHandle::newReal(
    double value, int decimal_places, bool trim_trailing_zeroes)
{
    return QPDFObjectHandle(
        QPDF_Real::create(value, decimal_places, trim_trailing_zeroes));
}

QPDFObjectHandle
QPDFObjectHandle::newName(std::string const& name)
{
    return QPDFObjectHandle(QPDF_Name::create(name));
}

QPDFObjectHandle
QPDFObjectHandle::newString(std::string const& str)
{
    return QPDFObjectHandle(QPDF_String::create(str));
}

QPDFObjectHandle
QPDFObjectHandle::newUnicodeString(std::string const& utf8_str)
{
    return QPDFObjectHandle(QPDF_String::create_utf16(utf8_str));
}

QPDFObjectHandle
QPDFObjectHandle::newOperator(std::string const& value)
{
    return QPDFObjectHandle(QPDF_Operator::create(value));
}

QPDFObjectHandle
QPDFObjectHandle::newInlineImage(std::string const& value)
{
    return QPDFObjectHandle(QPDF_InlineImage::create(value));
}

QPDFObjectHandle
QPDFObjectHandle::newArray()
{
    return newArray(std::vector<QPDFObjectHandle>());
}

QPDFObjectHandle
QPDFObjectHandle::newArray(std::vector<QPDFObjectHandle> const& items)
{
    return QPDFObjectHandle(QPDF_Array::create(items));
}

QPDFObjectHandle
QPDFObjectHandle::newArray(Rectangle const& rect)
{
    std::vector<QPDFObjectHandle> items;
    items.push_back(newReal(rect.llx));
    items.push_back(newReal(rect.lly));
    items.push_back(newReal(rect.urx));
    items.push_back(newReal(rect.ury));
    return newArray(items);
}

QPDFObjectHandle
QPDFObjectHandle::newArray(Matrix const& matrix)
{
    std::vector<QPDFObjectHandle> items;
    items.push_back(newReal(matrix.a));
    items.push_back(newReal(matrix.b));
    items.push_back(newReal(matrix.c));
    items.push_back(newReal(matrix.d));
    items.push_back(newReal(matrix.e));
    items.push_back(newReal(matrix.f));
    return newArray(items);
}

QPDFObjectHandle
QPDFObjectHandle::newArray(QPDFMatrix const& matrix)
{
    std::vector<QPDFObjectHandle> items;
    items.push_back(newReal(matrix.a));
    items.push_back(newReal(matrix.b));
    items.push_back(newReal(matrix.c));
    items.push_back(newReal(matrix.d));
    items.push_back(newReal(matrix.e));
    items.push_back(newReal(matrix.f));
    return newArray(items);
}

QPDFObjectHandle
QPDFObjectHandle::newFromRectangle(Rectangle const& rect)
{
    return newArray(rect);
}

QPDFObjectHandle
QPDFObjectHandle::newFromMatrix(Matrix const& m)
{
    return newArray(m);
}

QPDFObjectHandle
QPDFObjectHandle::newFromMatrix(QPDFMatrix const& m)
{
    return newArray(m);
}

QPDFObjectHandle
QPDFObjectHandle::newDictionary()
{
    return newDictionary(std::map<std::string, QPDFObjectHandle>());
}

QPDFObjectHandle
QPDFObjectHandle::newDictionary(
    std::map<std::string, QPDFObjectHandle> const& items)
{
    return QPDFObjectHandle(QPDF_Dictionary::create(items));
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf)
{
    if (qpdf == nullptr) {
        throw std::runtime_error(
            "attempt to create stream in null qpdf object");
    }
    QTC::TC("qpdf", "QPDFObjectHandle newStream");
    return qpdf->newStream();
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, std::shared_ptr<Buffer> data)
{
    if (qpdf == nullptr) {
        throw std::runtime_error(
            "attempt to create stream in null qpdf object");
    }
    QTC::TC("qpdf", "QPDFObjectHandle newStream with data");
    return qpdf->newStream(data);
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, std::string const& data)
{
    if (qpdf == nullptr) {
        throw std::runtime_error(
            "attempt to create stream in null qpdf object");
    }
    QTC::TC("qpdf", "QPDFObjectHandle newStream with string");
    return qpdf->newStream(data);
}

QPDFObjectHandle
QPDFObjectHandle::newReserved(QPDF* qpdf)
{
    return qpdf->makeIndirectObject(QPDFObjectHandle(QPDF_Reserved::create()));
}

void
QPDFObjectHandle::setObjectDescription(
    QPDF* owning_qpdf, std::string const& object_description)
{
    // This is called during parsing on newly created direct objects,
    // so we can't call dereference() here.
    if (isInitialized() && obj.get()) {
        obj->setDescription(owning_qpdf, object_description);
    }
}

bool
QPDFObjectHandle::hasObjectDescription()
{
    return dereference() && obj.get() && obj->hasDescription();
}

QPDFObjectHandle
QPDFObjectHandle::shallowCopy()
{
    QPDFObjectHandle result;
    shallowCopyInternal(result, false);
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::unsafeShallowCopy()
{
    QPDFObjectHandle result;
    shallowCopyInternal(result, true);
    return result;
}

void
QPDFObjectHandle::shallowCopyInternal(
    QPDFObjectHandle& new_obj, bool first_level_only)
{
    assertInitialized();

    if (isStream()) {
        QTC::TC("qpdf", "QPDFObjectHandle ERR shallow copy stream");
        throw std::runtime_error("attempt to make a shallow copy of a stream");
    }
    new_obj = QPDFObjectHandle(obj->shallowCopy());

    std::set<QPDFObjGen> visited;
    new_obj.copyObject(visited, false, first_level_only, false);
}

void
QPDFObjectHandle::copyObject(
    std::set<QPDFObjGen>& visited,
    bool cross_indirect,
    bool first_level_only,
    bool stop_at_streams)
{
    assertInitialized();

    if (isStream()) {
        QTC::TC(
            "qpdf", "QPDFObjectHandle copy stream", stop_at_streams ? 0 : 1);
        if (stop_at_streams) {
            return;
        }
        throw std::runtime_error(
            "attempt to make a stream into a direct object");
    }

    auto cur_og = getObjGen();
    if (cur_og.getObj() != 0) {
        if (visited.count(cur_og)) {
            QTC::TC("qpdf", "QPDFObjectHandle makeDirect loop");
            throw std::runtime_error(
                "loop detected while converting object from "
                "indirect to direct");
        }
        visited.insert(cur_og);
    }

    if (isReserved()) {
        throw std::logic_error("QPDFObjectHandle: attempting to make a"
                               " reserved object handle direct");
    }

    std::shared_ptr<QPDFObject> new_obj;

    if (isBool() || isInteger() || isName() || isNull() || isReal() ||
        isString()) {
        new_obj = obj->shallowCopy();
    } else if (isArray()) {
        std::vector<QPDFObjectHandle> items;
        auto array = asArray();
        int n = array->getNItems();
        for (int i = 0; i < n; ++i) {
            items.push_back(array->getItem(i));
            if ((!first_level_only) &&
                (cross_indirect || (!items.back().isIndirect()))) {
                items.back().copyObject(
                    visited, cross_indirect, first_level_only, stop_at_streams);
            }
        }
        new_obj = QPDF_Array::create(items);
    } else if (isDictionary()) {
        std::map<std::string, QPDFObjectHandle> items;
        auto dict = asDictionary();
        for (auto const& key: getKeys()) {
            items[key] = dict->getKey(key);
            if ((!first_level_only) &&
                (cross_indirect || (!items[key].isIndirect()))) {
                items[key].copyObject(
                    visited, cross_indirect, first_level_only, stop_at_streams);
            }
        }
        new_obj = QPDF_Dictionary::create(items);
    } else {
        throw std::logic_error("QPDFObjectHandle::makeDirectInternal: "
                               "unknown object type");
    }

    this->obj = new_obj;

    if (cur_og.getObj()) {
        visited.erase(cur_og);
    }
}

QPDFObjectHandle
QPDFObjectHandle::copyStream()
{
    assertStream();
    QPDFObjectHandle result = newStream(this->getOwningQPDF());
    QPDFObjectHandle dict = result.getDict();
    QPDFObjectHandle old_dict = getDict();
    for (auto& iter: QPDFDictItems(old_dict)) {
        if (iter.second.isIndirect()) {
            dict.replaceKey(iter.first, iter.second);
        } else {
            dict.replaceKey(iter.first, iter.second.shallowCopy());
        }
    }
    QPDF::StreamCopier::copyStreamData(getOwningQPDF(), result, *this);
    return result;
}

void
QPDFObjectHandle::makeDirect(bool allow_streams)
{
    std::set<QPDFObjGen> visited;
    copyObject(visited, true, false, allow_streams);
}

void
QPDFObjectHandle::assertInitialized() const
{
    if (!isInitialized()) {
        throw std::logic_error("operation attempted on uninitialized "
                               "QPDFObjectHandle");
    }
}

void
QPDFObjectHandle::typeWarning(
    char const* expected_type, std::string const& warning)
{
    QPDF* context = nullptr;
    std::string description;
    // Type checks above guarantee that the object has been dereferenced.
    // Nevertheless, dereference throws exceptions in the test suite
    if (!dereference()) {
        throw std::logic_error(
            "attempted to dereference an uninitialized QPDFObjectHandle");
    }
    this->obj->getDescription(context, description);
    // Null context handled by warn
    warn(
        context,
        QPDFExc(
            qpdf_e_object,
            "",
            description,
            0,
            std::string("operation for ") + expected_type +
                " attempted on object of type " + getTypeName() + ": " +
                warning));
}

void
QPDFObjectHandle::warnIfPossible(std::string const& warning)
{
    QPDF* context = nullptr;
    std::string description;
    if (dereference() && obj->getDescription(context, description)) {
        warn(context, QPDFExc(qpdf_e_damaged_pdf, "", description, 0, warning));
    } else {
        *QPDFLogger::defaultLogger()->getError() << warning << "\n";
    }
}

void
QPDFObjectHandle::objectWarning(std::string const& warning)
{
    QPDF* context = nullptr;
    std::string description;
    // Type checks above guarantee that the object has been dereferenced.
    this->obj->getDescription(context, description);
    // Null context handled by warn
    warn(context, QPDFExc(qpdf_e_object, "", description, 0, warning));
}

void
QPDFObjectHandle::assertType(char const* type_name, bool istype)
{
    if (!istype) {
        throw std::runtime_error(
            std::string("operation for ") + type_name +
            " attempted on object of type " + getTypeName());
    }
}

void
QPDFObjectHandle::assertNull()
{
    assertType("null", isNull());
}

void
QPDFObjectHandle::assertBool()
{
    assertType("boolean", isBool());
}

void
QPDFObjectHandle::assertInteger()
{
    assertType("integer", isInteger());
}

void
QPDFObjectHandle::assertReal()
{
    assertType("real", isReal());
}

void
QPDFObjectHandle::assertName()
{
    assertType("name", isName());
}

void
QPDFObjectHandle::assertString()
{
    assertType("string", isString());
}

void
QPDFObjectHandle::assertOperator()
{
    assertType("operator", isOperator());
}

void
QPDFObjectHandle::assertInlineImage()
{
    assertType("inlineimage", isInlineImage());
}

void
QPDFObjectHandle::assertArray()
{
    assertType("array", isArray());
}

void
QPDFObjectHandle::assertDictionary()
{
    assertType("dictionary", isDictionary());
}

void
QPDFObjectHandle::assertStream()
{
    assertType("stream", isStream());
}

void
QPDFObjectHandle::assertReserved()
{
    assertType("reserved", isReserved());
}

void
QPDFObjectHandle::assertIndirect()
{
    if (!isIndirect()) {
        throw std::logic_error(
            "operation for indirect object attempted on direct object");
    }
}

void
QPDFObjectHandle::assertScalar()
{
    assertType("scalar", isScalar());
}

void
QPDFObjectHandle::assertNumber()
{
    assertType("number", isNumber());
}

bool
QPDFObjectHandle::isPageObject()
{
    // See comments in QPDFObjectHandle.hh.
    if (getOwningQPDF() == nullptr) {
        return false;
    }
    // getAllPages repairs /Type when traversing the page tree.
    getOwningQPDF()->getAllPages();
    if (!this->isDictionary()) {
        return false;
    }
    if (this->hasKey("/Type")) {
        QPDFObjectHandle type = this->getKey("/Type");
        if (type.isNameAndEquals("/Page")) {
            return true;
        }
        // Files have been seen in the wild that have /Type (Page)
        else if (type.isString() && (type.getStringValue() == "Page")) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

bool
QPDFObjectHandle::isPagesObject()
{
    if (getOwningQPDF() == nullptr) {
        return false;
    }
    // getAllPages repairs /Type when traversing the page tree.
    getOwningQPDF()->getAllPages();
    return isDictionaryOfType("/Pages");
}

bool
QPDFObjectHandle::isFormXObject()
{
    return isStreamOfType("", "/Form");
}

bool
QPDFObjectHandle::isImage(bool exclude_imagemask)
{
    return (
        isStreamOfType("", "/Image") &&
        ((!exclude_imagemask) ||
         (!(getDict().getKey("/ImageMask").isBool() &&
            getDict().getKey("/ImageMask").getBoolValue()))));
}

void
QPDFObjectHandle::checkOwnership(QPDFObjectHandle const& item) const
{
    auto qpdf = getOwningQPDF();
    auto item_qpdf = item.getOwningQPDF();
    if ((qpdf != nullptr) && (item_qpdf != nullptr) && (qpdf != item_qpdf)) {
        QTC::TC("qpdf", "QPDFObjectHandle check ownership");
        throw std::logic_error(
            "Attempting to add an object from a different QPDF."
            " Use QPDF::copyForeignObject to add objects from another file.");
    }
}

void
QPDFObjectHandle::assertPageObject()
{
    if (!isPageObject()) {
        throw std::runtime_error("page operation called on non-Page object");
    }
}

bool
QPDFObjectHandle::dereference()
{
    if (!isInitialized()) {
        return false;
    }
    this->obj->resolve();
    return true;
}

void
QPDFObjectHandle::warn(QPDF* qpdf, QPDFExc const& e)
{
    // If parsing on behalf of a QPDF object and want to give a
    // warning, we can warn through the object. If parsing for some
    // other reason, such as an explicit creation of an object from a
    // string, then just throw the exception.
    if (qpdf) {
        qpdf->warn(e);
    } else {
        throw e;
    }
}

QPDFObjectHandle::QPDFDictItems::QPDFDictItems(QPDFObjectHandle const& oh) :
    oh(oh)
{
}

QPDFObjectHandle::QPDFDictItems::iterator&
QPDFObjectHandle::QPDFDictItems::iterator::operator++()
{
    ++this->m->iter;
    updateIValue();
    return *this;
}

QPDFObjectHandle::QPDFDictItems::iterator&
QPDFObjectHandle::QPDFDictItems::iterator::operator--()
{
    --this->m->iter;
    updateIValue();
    return *this;
}

QPDFObjectHandle::QPDFDictItems::iterator::reference
QPDFObjectHandle::QPDFDictItems::iterator::operator*()
{
    updateIValue();
    return this->ivalue;
}

QPDFObjectHandle::QPDFDictItems::iterator::pointer
QPDFObjectHandle::QPDFDictItems::iterator::operator->()
{
    updateIValue();
    return &this->ivalue;
}

bool
QPDFObjectHandle::QPDFDictItems::iterator::operator==(
    iterator const& other) const
{
    if (this->m->is_end && other.m->is_end) {
        return true;
    }
    if (this->m->is_end || other.m->is_end) {
        return false;
    }
    return (this->ivalue.first == other.ivalue.first);
}

QPDFObjectHandle::QPDFDictItems::iterator::iterator(
    QPDFObjectHandle& oh, bool for_begin) :
    m(new Members(oh, for_begin))
{
    updateIValue();
}

void
QPDFObjectHandle::QPDFDictItems::iterator::updateIValue()
{
    this->m->is_end = (this->m->iter == this->m->keys.end());
    if (this->m->is_end) {
        this->ivalue.first = "";
        this->ivalue.second = QPDFObjectHandle();
    } else {
        this->ivalue.first = *(this->m->iter);
        this->ivalue.second = this->m->oh.getKey(this->ivalue.first);
    }
}

QPDFObjectHandle::QPDFDictItems::iterator::Members::Members(
    QPDFObjectHandle& oh, bool for_begin) :
    oh(oh)
{
    this->keys = oh.getKeys();
    this->iter = for_begin ? this->keys.begin() : this->keys.end();
}

QPDFObjectHandle::QPDFDictItems::iterator
QPDFObjectHandle::QPDFDictItems::begin()
{
    return iterator(oh, true);
}

QPDFObjectHandle::QPDFDictItems::iterator
QPDFObjectHandle::QPDFDictItems::end()
{
    return iterator(oh, false);
}

QPDFObjectHandle::QPDFArrayItems::QPDFArrayItems(QPDFObjectHandle const& oh) :
    oh(oh)
{
}

QPDFObjectHandle::QPDFArrayItems::iterator&
QPDFObjectHandle::QPDFArrayItems::iterator::operator++()
{
    if (!this->m->is_end) {
        ++this->m->item_number;
        updateIValue();
    }
    return *this;
}

QPDFObjectHandle::QPDFArrayItems::iterator&
QPDFObjectHandle::QPDFArrayItems::iterator::operator--()
{
    if (this->m->item_number > 0) {
        --this->m->item_number;
        updateIValue();
    }
    return *this;
}

QPDFObjectHandle::QPDFArrayItems::iterator::reference
QPDFObjectHandle::QPDFArrayItems::iterator::operator*()
{
    updateIValue();
    return this->ivalue;
}

QPDFObjectHandle::QPDFArrayItems::iterator::pointer
QPDFObjectHandle::QPDFArrayItems::iterator::operator->()
{
    updateIValue();
    return &this->ivalue;
}

bool
QPDFObjectHandle::QPDFArrayItems::iterator::operator==(
    iterator const& other) const
{
    return (this->m->item_number == other.m->item_number);
}

QPDFObjectHandle::QPDFArrayItems::iterator::iterator(
    QPDFObjectHandle& oh, bool for_begin) :
    m(new Members(oh, for_begin))
{
    updateIValue();
}

void
QPDFObjectHandle::QPDFArrayItems::iterator::updateIValue()
{
    this->m->is_end = (this->m->item_number >= this->m->oh.getArrayNItems());
    if (this->m->is_end) {
        this->ivalue = QPDFObjectHandle();
    } else {
        this->ivalue = this->m->oh.getArrayItem(this->m->item_number);
    }
}

QPDFObjectHandle::QPDFArrayItems::iterator::Members::Members(
    QPDFObjectHandle& oh, bool for_begin) :
    oh(oh)
{
    this->item_number = for_begin ? 0 : oh.getArrayNItems();
}

QPDFObjectHandle::QPDFArrayItems::iterator
QPDFObjectHandle::QPDFArrayItems::begin()
{
    return iterator(oh, true);
}

QPDFObjectHandle::QPDFArrayItems::iterator
QPDFObjectHandle::QPDFArrayItems::end()
{
    return iterator(oh, false);
}

QPDFObjGen
QPDFObjectHandle::getObjGen() const
{
    return isInitialized() ? obj->getObjGen() : QPDFObjGen();
}

// Indirect object accessors
QPDF*
QPDFObjectHandle::getOwningQPDF() const
{
    return isInitialized() ? this->obj->getQPDF() : nullptr;
}

QPDF&
QPDFObjectHandle::getQPDF(std::string const& error_msg) const
{
    auto result = isInitialized() ? this->obj->getQPDF() : nullptr;
    if (result == nullptr) {
        throw std::runtime_error(
            error_msg == "" ? "attempt to use a null qpdf object" : error_msg);
    }
    return *result;
}

void
QPDFObjectHandle::setParsedOffset(qpdf_offset_t offset)
{
    // This is called during parsing on newly created direct objects,
    // so we can't call dereference() here.
    if (isInitialized()) {
        this->obj->setParsedOffset(offset);
    }
}

QPDFObjectHandle
operator""_qpdf(char const* v, size_t len)
{
    return QPDFObjectHandle::parse(
        std::string(v, len), "QPDFObjectHandle literal");
}
