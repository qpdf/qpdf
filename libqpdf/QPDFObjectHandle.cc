#include <qpdf/QPDFObjectHandle_private.hh>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/JSON_writer.hh>
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

#include <qpdf/QIntC.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using namespace std::literals;
using namespace qpdf;

BaseHandle::
operator QPDFObjGen() const
{
    return obj ? obj->getObjGen() : QPDFObjGen();
}

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

QPDFObjectHandle::StreamDataProvider::~StreamDataProvider() // NOLINT (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
}

void
QPDFObjectHandle::StreamDataProvider::provideStreamData(QPDFObjGen const& og, Pipeline* pipeline)
{
    return provideStreamData(og.getObj(), og.getGen(), pipeline);
}

bool
QPDFObjectHandle::StreamDataProvider::provideStreamData(
    QPDFObjGen const& og, Pipeline* pipeline, bool suppress_warnings, bool will_retry)
{
    return provideStreamData(og.getObj(), og.getGen(), pipeline, suppress_warnings, will_retry);
}

void
QPDFObjectHandle::StreamDataProvider::provideStreamData(
    int objid, int generation, Pipeline* pipeline)
{
    throw std::logic_error("you must override provideStreamData -- see QPDFObjectHandle.hh");
}

bool
QPDFObjectHandle::StreamDataProvider::provideStreamData(
    int objid, int generation, Pipeline* pipeline, bool suppress_warnings, bool will_retry)
{
    throw std::logic_error("you must override provideStreamData -- see QPDFObjectHandle.hh");
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
        CoalesceProvider(QPDFObjectHandle containing_page, QPDFObjectHandle old_contents) :
            containing_page(containing_page),
            old_contents(old_contents)
        {
        }
        ~CoalesceProvider() override = default;
        void provideStreamData(QPDFObjGen const&, Pipeline* pipeline) override;

      private:
        QPDFObjectHandle containing_page;
        QPDFObjectHandle old_contents;
    };
} // namespace

void
CoalesceProvider::provideStreamData(QPDFObjGen const&, Pipeline* p)
{
    QTC::TC("qpdf", "QPDFObjectHandle coalesce provide stream data");
    std::string description = "page object " + containing_page.getObjGen().unparse(' ');
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
    std::string const& value = token.getRawValue();
    write(value.c_str(), value.length());
}

void
QPDFObjectHandle::ParserCallbacks::handleObject(QPDFObjectHandle)
{
    throw std::logic_error("You must override one of the handleObject methods in ParserCallbacks");
}

void
QPDFObjectHandle::ParserCallbacks::handleObject(QPDFObjectHandle oh, size_t, size_t)
{
    // This version of handleObject was added in qpdf 9. If the developer did not override it, fall
    // back to the older interface.
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
    class LastChar final: public Pipeline
    {
      public:
        LastChar(Pipeline& next);
        ~LastChar() final = default;
        void write(unsigned char const* data, size_t len) final;
        void finish() final;
        unsigned char getLastChar();

      private:
        unsigned char last_char{0};
    };
} // namespace

LastChar::LastChar(Pipeline& next) :
    Pipeline("lastchar", &next)
{
}

void
LastChar::write(unsigned char const* data, size_t len)
{
    if (len > 0) {
        this->last_char = data[len - 1];
    }
    next()->write(data, len);
}

void
LastChar::finish()
{
    next()->finish();
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
    // Recursively remove association with any QPDF object. This method may only be called during
    // final destruction. QPDF::~QPDF() calls it for indirect objects using the object pointer
    // itself, so we don't do that here. Other objects call it through this method.
    if (obj && !isIndirect()) {
        this->obj->disconnect();
    }
}

qpdf_object_type_e
QPDFObjectHandle::getTypeCode() const
{
    return obj ? obj->getResolvedTypeCode() : ::ot_uninitialized;
}

char const*
QPDFObjectHandle::getTypeName() const
{
    static constexpr std::array<char const*, 15> tn{
        "uninitialized",
        "reserved",
        "null",
        "boolean",
        "integer",
        "real",
        "string",
        "name",
        "array",
        "dictionary",
        "stream",
        "operator",
        "inline-image",
        "unresolved",
        "destroyed"};
    return obj ? tn[getTypeCode()] : "uninitialized";
}

QPDF_Array*
QPDFObjectHandle::asArray() const
{
    return obj ? obj->as<QPDF_Array>() : nullptr;
}

QPDF_Bool*
QPDFObjectHandle::asBool() const
{
    return obj ? obj->as<QPDF_Bool>() : nullptr;
}

QPDF_Dictionary*
QPDFObjectHandle::asDictionary() const
{
    return obj ? obj->as<QPDF_Dictionary>() : nullptr;
}

QPDF_InlineImage*
QPDFObjectHandle::asInlineImage() const
{
    return obj ? obj->as<QPDF_InlineImage>() : nullptr;
}

QPDF_Integer*
QPDFObjectHandle::asInteger() const
{
    return obj ? obj->as<QPDF_Integer>() : nullptr;
}

QPDF_Name*
QPDFObjectHandle::asName() const
{
    return obj ? obj->as<QPDF_Name>() : nullptr;
}

QPDF_Null*
QPDFObjectHandle::asNull() const
{
    return obj ? obj->as<QPDF_Null>() : nullptr;
}

QPDF_Operator*
QPDFObjectHandle::asOperator() const
{
    return obj ? obj->as<QPDF_Operator>() : nullptr;
}

QPDF_Real*
QPDFObjectHandle::asReal() const
{
    return obj ? obj->as<QPDF_Real>() : nullptr;
}

QPDF_Reserved*
QPDFObjectHandle::asReserved() const
{
    return obj ? obj->as<QPDF_Reserved>() : nullptr;
}

QPDF_Stream*
QPDFObjectHandle::asStream() const
{
    return obj ? obj->as<QPDF_Stream>() : nullptr;
}

QPDF_Stream*
QPDFObjectHandle::asStreamWithAssert() const
{
    auto stream = asStream();
    assertType("stream", stream);
    return stream;
}

QPDF_String*
QPDFObjectHandle::asString() const
{
    return obj ? obj->as<QPDF_String>() : nullptr;
}

bool
QPDFObjectHandle::isDestroyed() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_destroyed;
}

bool
QPDFObjectHandle::isBool() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_boolean;
}

bool
QPDFObjectHandle::isDirectNull() const
{
    // Don't call dereference() -- this is a const method, and we know
    // objid == 0, so there's nothing to resolve.
    return (obj && getObjectID() == 0 && obj->getTypeCode() == ::ot_null);
}

bool
QPDFObjectHandle::isNull() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_null;
}

bool
QPDFObjectHandle::isInteger() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_integer;
}

bool
QPDFObjectHandle::isReal() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_real;
}

bool
QPDFObjectHandle::isNumber() const
{
    return (isInteger() || isReal());
}

double
QPDFObjectHandle::getNumericValue() const
{
    if (isInteger()) {
        return static_cast<double>(getIntValue());
    } else if (isReal()) {
        return atof(getRealValue().c_str());
    } else {
        typeWarning("number", "returning 0");
        QTC::TC("qpdf", "QPDFObjectHandle numeric non-numeric");
        return 0;
    }
}

bool
QPDFObjectHandle::getValueAsNumber(double& value) const
{
    if (!isNumber()) {
        return false;
    }
    value = getNumericValue();
    return true;
}

bool
QPDFObjectHandle::isName() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_name;
}

bool
QPDFObjectHandle::isString() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_string;
}

bool
QPDFObjectHandle::isOperator() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_operator;
}

bool
QPDFObjectHandle::isInlineImage() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_inlineimage;
}

bool
QPDFObjectHandle::isArray() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_array;
}

bool
QPDFObjectHandle::isDictionary() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_dictionary;
}

bool
QPDFObjectHandle::isStream() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_stream;
}

bool
QPDFObjectHandle::isReserved() const
{
    return obj && obj->getResolvedTypeCode() == ::ot_reserved;
}

bool
QPDFObjectHandle::isScalar() const
{
    return isBool() || isInteger() || isName() || isNull() || isReal() || isString();
}

bool
QPDFObjectHandle::isNameAndEquals(std::string const& name) const
{
    return isName() && (getName() == name);
}

bool
QPDFObjectHandle::isDictionaryOfType(std::string const& type, std::string const& subtype) const
{
    return isDictionary() && (type.empty() || getKey("/Type").isNameAndEquals(type)) &&
        (subtype.empty() || getKey("/Subtype").isNameAndEquals(subtype));
}

bool
QPDFObjectHandle::isStreamOfType(std::string const& type, std::string const& subtype) const
{
    return isStream() && getDict().isDictionaryOfType(type, subtype);
}

// Bool accessors

bool
QPDFObjectHandle::getBoolValue() const
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
QPDFObjectHandle::getValueAsBool(bool& value) const
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
QPDFObjectHandle::getIntValue() const
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
QPDFObjectHandle::getValueAsInt(long long& value) const
{
    auto integer = asInteger();
    if (integer == nullptr) {
        return false;
    }
    value = integer->getVal();
    return true;
}

int
QPDFObjectHandle::getIntValueAsInt() const
{
    int result = 0;
    long long v = getIntValue();
    if (v < INT_MIN) {
        QTC::TC("qpdf", "QPDFObjectHandle int returning INT_MIN");
        warnIfPossible("requested value of integer is too small; returning INT_MIN");
        result = INT_MIN;
    } else if (v > INT_MAX) {
        QTC::TC("qpdf", "QPDFObjectHandle int returning INT_MAX");
        warnIfPossible("requested value of integer is too big; returning INT_MAX");
        result = INT_MAX;
    } else {
        result = static_cast<int>(v);
    }
    return result;
}

bool
QPDFObjectHandle::getValueAsInt(int& value) const
{
    if (!isInteger()) {
        return false;
    }
    value = getIntValueAsInt();
    return true;
}

unsigned long long
QPDFObjectHandle::getUIntValue() const
{
    long long v = getIntValue();
    if (v < 0) {
        QTC::TC("qpdf", "QPDFObjectHandle uint returning 0");
        warnIfPossible("unsigned value request for negative number; returning 0");
        return 0;
    } else {
        return static_cast<unsigned long long>(v);
    }
}

bool
QPDFObjectHandle::getValueAsUInt(unsigned long long& value) const
{
    if (!isInteger()) {
        return false;
    }
    value = getUIntValue();
    return true;
}

unsigned int
QPDFObjectHandle::getUIntValueAsUInt() const
{
    long long v = getIntValue();
    if (v < 0) {
        QTC::TC("qpdf", "QPDFObjectHandle uint uint returning 0");
        warnIfPossible("unsigned integer value request for negative number; returning 0");
        return 0;
    } else if (v > UINT_MAX) {
        QTC::TC("qpdf", "QPDFObjectHandle uint returning UINT_MAX");
        warnIfPossible("requested value of unsigned integer is too big; returning UINT_MAX");
        return UINT_MAX;
    } else {
        return static_cast<unsigned int>(v);
    }
}

bool
QPDFObjectHandle::getValueAsUInt(unsigned int& value) const
{
    if (!isInteger()) {
        return false;
    }
    value = getUIntValueAsUInt();
    return true;
}

// Real accessors

std::string
QPDFObjectHandle::getRealValue() const
{
    if (isReal()) {
        return obj->getStringValue();
    } else {
        typeWarning("real", "returning 0.0");
        QTC::TC("qpdf", "QPDFObjectHandle real returning 0.0");
        return "0.0";
    }
}

bool
QPDFObjectHandle::getValueAsReal(std::string& value) const
{
    if (!isReal()) {
        return false;
    }
    value = obj->getStringValue();
    return true;
}

// Name accessors

std::string
QPDFObjectHandle::getName() const
{
    if (isName()) {
        return obj->getStringValue();
    } else {
        typeWarning("name", "returning dummy name");
        QTC::TC("qpdf", "QPDFObjectHandle name returning dummy name");
        return "/QPDFFakeName";
    }
}

bool
QPDFObjectHandle::getValueAsName(std::string& value) const
{
    if (!isName()) {
        return false;
    }
    value = obj->getStringValue();
    return true;
}

// String accessors

std::string
QPDFObjectHandle::getStringValue() const
{
    if (isString()) {
        return obj->getStringValue();
    } else {
        typeWarning("string", "returning empty string");
        QTC::TC("qpdf", "QPDFObjectHandle string returning empty string");
        return "";
    }
}

bool
QPDFObjectHandle::getValueAsString(std::string& value) const
{
    if (!isString()) {
        return false;
    }
    value = obj->getStringValue();
    return true;
}

std::string
QPDFObjectHandle::getUTF8Value() const
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
QPDFObjectHandle::getValueAsUTF8(std::string& value) const
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
QPDFObjectHandle::getOperatorValue() const
{
    if (isOperator()) {
        return obj->getStringValue();
    } else {
        typeWarning("operator", "returning fake value");
        QTC::TC("qpdf", "QPDFObjectHandle operator returning fake value");
        return "QPDFFAKE";
    }
}

bool
QPDFObjectHandle::getValueAsOperator(std::string& value) const
{
    if (!isOperator()) {
        return false;
    }
    value = obj->getStringValue();
    return true;
}

std::string
QPDFObjectHandle::getInlineImageValue() const
{
    if (isInlineImage()) {
        return obj->getStringValue();
    } else {
        typeWarning("inlineimage", "returning empty data");
        QTC::TC("qpdf", "QPDFObjectHandle inlineimage returning empty data");
        return "";
    }
}

bool
QPDFObjectHandle::getValueAsInlineImage(std::string& value) const
{
    if (!isInlineImage()) {
        return false;
    }
    value = obj->getStringValue();
    return true;
}

// Array accessors and mutators are in QPDF_Array.cc

QPDFObjectHandle::QPDFArrayItems
QPDFObjectHandle::aitems()
{
    return *this;
}

// Dictionary accessors are in QPDF_Dictionary.cc

QPDFObjectHandle::QPDFDictItems
QPDFObjectHandle::ditems()
{
    return {*this};
}

// Array and Name accessors

bool
QPDFObjectHandle::isOrHasName(std::string const& value) const
{
    if (isNameAndEquals(value)) {
        return true;
    } else if (isArray()) {
        for (auto& item: getArrayAsVector()) {
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
        for (auto const& i2: sub.ditems()) {
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
    QPDFObjectHandle other, std::map<std::string, std::map<std::string, std::string>>* conflicts)
{
    if (!(isDictionary() && other.isDictionary())) {
        QTC::TC("qpdf", "QPDFObjectHandle merge top type mismatch");
        return;
    }

    auto make_og_to_name = [](QPDFObjectHandle& dict,
                              std::map<QPDFObjGen, std::string>& og_to_name) {
        for (auto const& i: dict.ditems()) {
            if (i.second.isIndirect()) {
                og_to_name[i.second.getObjGen()] = i.first;
            }
        }
    };

    // This algorithm is described in comments in QPDFObjectHandle.hh
    // above the declaration of mergeResources.
    for (auto const& o_top: other.ditems()) {
        std::string const& rtype = o_top.first;
        QPDFObjectHandle other_val = o_top.second;
        if (hasKey(rtype)) {
            QPDFObjectHandle this_val = getKey(rtype);
            if (this_val.isDictionary() && other_val.isDictionary()) {
                if (this_val.isIndirect()) {
                    // Do this even if there are no keys. Various places in the code call
                    // mergeResources with resource dictionaries that contain empty subdictionaries
                    // just to get this shallow copy functionality.
                    QTC::TC("qpdf", "QPDFObjectHandle replace with copy");
                    this_val = replaceKeyAndGetNew(rtype, this_val.shallowCopy());
                }
                std::map<QPDFObjGen, std::string> og_to_name;
                std::set<std::string> rnames;
                int min_suffix = 1;
                bool initialized_maps = false;
                for (auto const& ov_iter: other_val.ditems()) {
                    std::string const& key = ov_iter.first;
                    QPDFObjectHandle rval = ov_iter.second;
                    if (!this_val.hasKey(key)) {
                        if (!rval.isIndirect()) {
                            QTC::TC("qpdf", "QPDFObjectHandle merge shallow copy");
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
                            std::string new_key =
                                getUniqueResourceName(key + "_", min_suffix, &rnames);
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
QPDFObjectHandle::getResourceNames() const
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
    std::string const& prefix, int& min_suffix, std::set<std::string>* namesp) const
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
    throw std::logic_error(
        "unable to find unconflicting name in"
        " QPDFObjectHandle::getUniqueResourceName");
}

// Dictionary mutators are in QPDF_Dictionary.cc

// Stream accessors are in QPDF_Stream.cc

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
    if (auto array = as_array(strict)) {
        int n_items = array.size();
        for (int i = 0; i < n_items; ++i) {
            QPDFObjectHandle item = array.at(i).second;
            if (item.isStream()) {
                result.emplace_back(item);
            } else {
                QTC::TC("qpdf", "QPDFObjectHandle non-stream in stream array");
                warn(
                    item.getOwningQPDF(),
                    QPDFExc(
                        qpdf_e_damaged_pdf,
                        "",
                        description + ": item index " + std::to_string(i) + " (from 0)",
                        0,
                        "ignoring non-stream in an array of streams"));
            }
        }
    } else if (isStream()) {
        result.emplace_back(*this);
    } else if (!isNull()) {
        warn(
            getOwningQPDF(),
            QPDFExc(
                qpdf_e_damaged_pdf,
                "",
                description,
                0,
                " object is supposed to be a stream or an array of streams but is neither"));
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
    return this->getKey("/Contents").arrayOrStreamToStreamArray(description, all_description);
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
        throw std::runtime_error(
            "QPDF::rotatePage called with an angle that is not a multiple of 90");
    }
    int new_angle = angle;
    if (relative) {
        int old_angle = 0;
        QPDFObjectHandle cur_obj = *this;
        QPDFObjGen::set visited;
        while (visited.add(cur_obj)) {
            // Don't get stuck in an infinite loop
            if (cur_obj.getKey("/Rotate").getValueAsInt(old_angle)) {
                break;
            } else if (cur_obj.getKey("/Parent").isDictionary()) {
                cur_obj = cur_obj.getKey("/Parent");
            } else {
                break;
            }
        }
        QTC::TC("qpdf", "QPDFObjectHandle found old angle", visited.size() > 1 ? 0 : 1);
        if ((old_angle % 90) != 0) {
            old_angle = 0;
        }
        new_angle += old_angle;
    }
    new_angle = (new_angle + 360) % 360;
    // Make this explicit even with new_angle == 0 since /Rotate can be inherited.
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
        // /Contents is optional for pages, and some very damaged files may have pages that are
        // invalid in other ways.
        return;
    }
    // Should not be possible for a page object to not have an owning PDF unless it was manually
    // constructed in some incorrect way. However, it can happen in a PDF file whose page structure
    // is direct, which is against spec but still possible to hand construct, as in fuzz issue
    // 27393.
    QPDF& qpdf = getQPDF("coalesceContentStreams called on object  with no associated PDF file");

    QPDFObjectHandle new_contents = newStream(&qpdf);
    this->replaceKey("/Contents", new_contents);

    auto provider = std::shared_ptr<StreamDataProvider>(new CoalesceProvider(*this, contents));
    new_contents.replaceStreamData(provider, newNull(), newNull());
}

std::string
QPDFObjectHandle::unparse() const
{
    if (this->isIndirect()) {
        return getObjGen().unparse(' ') + " R";
    } else {
        return unparseResolved();
    }
}

std::string
QPDFObjectHandle::unparseResolved() const
{
    if (!obj) {
        throw std::logic_error("attempted to dereference an uninitialized QPDFObjectHandle");
    }
    return obj->unparse();
}

std::string
QPDFObjectHandle::unparseBinary() const
{
    if (auto str = asString()) {
        return str->unparse(true);
    } else {
        return unparse();
    }
}

JSON
QPDFObjectHandle::getJSON(int json_version, bool dereference_indirect) const
{
    if ((!dereference_indirect) && isIndirect()) {
        return JSON::makeString(unparse());
    } else if (!obj) {
        throw std::logic_error("attempted to dereference an uninitialized QPDFObjectHandle");
    } else {
        Pl_Buffer p{"json"};
        JSON::Writer jw{&p, 0};
        writeJSON(json_version, jw, dereference_indirect);
        p.finish();
        return JSON::parse(p.getString());
    }
}

void
QPDFObjectHandle::writeJSON(int json_version, JSON::Writer& p, bool dereference_indirect) const
{
    if (!dereference_indirect && isIndirect()) {
        p << "\"" << getObjGen().unparse(' ') << " R\"";
    } else if (!obj) {
        throw std::logic_error("attempted to dereference an uninitialized QPDFObjectHandle");
    } else {
        obj->writeJSON(json_version, p);
    }
}

void
QPDFObjectHandle::writeJSON(
    int json_version, Pipeline* p, bool dereference_indirect, size_t depth) const
{
    JSON::Writer jw{p, depth};
    writeJSON(json_version, jw, dereference_indirect);
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
QPDFObjectHandle::parse(std::string const& object_str, std::string const& object_description)
{
    return parse(nullptr, object_str, object_description);
}

QPDFObjectHandle
QPDFObjectHandle::parse(
    QPDF* context, std::string const& object_str, std::string const& object_description)
{
    auto input = std::shared_ptr<InputSource>(new BufferInputSource("parsed object", object_str));
    QPDFTokenizer tokenizer;
    bool empty = false;
    QPDFObjectHandle result = parse(input, object_description, tokenizer, empty, nullptr, context);
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
    this->getKey("/Contents").pipeContentStreams(p, description, all_description);
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
        LastChar lc(buf);
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
    p->writeString(buf.getString());
    p->finish();
}

void
QPDFObjectHandle::parsePageContents(ParserCallbacks* callbacks)
{
    std::string description = "page object " + getObjGen().unparse(' ');
    this->getKey("/Contents").parseContentStream_internal(description, callbacks);
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
    auto description = "token filter for page object " + getObjGen().unparse(' ');
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
QPDFObjectHandle::parseContentStream(QPDFObjectHandle stream_or_array, ParserCallbacks* callbacks)
{
    stream_or_array.parseContentStream_internal("content stream objects", callbacks);
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
        parseContentStream_data(stream_data, all_description, callbacks, getOwningQPDF());
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
    auto input =
        std::shared_ptr<InputSource>(new BufferInputSource(description, stream_data.get()));
    QPDFTokenizer tokenizer;
    tokenizer.allowEOF();
    bool empty = false;
    while (QIntC::to_size(input->tell()) < stream_length) {
        // Read a token and seek to the beginning. The offset we get from this process is the
        // beginning of the next non-ignorable (space, comment) token. This way, the offset and
        // don't including ignorable content.
        tokenizer.readToken(input, "content", true);
        qpdf_offset_t offset = input->getLastOffset();
        input->seek(offset, SEEK_SET);
        auto obj =
            QPDFParser(*input, "content", tokenizer, nullptr, context, false).parse(empty, true);
        if (!obj) {
            // EOF
            break;
        }
        size_t length = QIntC::to_size(input->tell() - offset);

        callbacks->handleObject(obj, QIntC::to_size(offset), length);
        if (obj.isOperator() && (obj.getOperatorValue() == "ID")) {
            // Discard next character; it is the space after ID that terminated the token.  Read
            // until end of inline image.
            char ch;
            input->read(&ch, 1);
            tokenizer.expectInlineImage(input);
            QPDFTokenizer::Token t = tokenizer.readToken(input, description, true);
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
                    QPDFObjectHandle::newInlineImage(inline_image), QIntC::to_size(offset), length);
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
    return as_stream(error).addTokenFilter(filter);
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
    return QPDFParser(*input, object_description, tokenizer, decrypter, context, false)
        .parse(empty, false);
}

qpdf_offset_t
QPDFObjectHandle::getParsedOffset() const
{
    return obj ? obj->getParsedOffset() : -1;
}

QPDFObjectHandle
QPDFObjectHandle::newBool(bool value)
{
    return {QPDF_Bool::create(value)};
}

QPDFObjectHandle
QPDFObjectHandle::newNull()
{
    return {QPDF_Null::create()};
}

QPDFObjectHandle
QPDFObjectHandle::newInteger(long long value)
{
    return {QPDF_Integer::create(value)};
}

QPDFObjectHandle
QPDFObjectHandle::newReal(std::string const& value)
{
    return {QPDF_Real::create(value)};
}

QPDFObjectHandle
QPDFObjectHandle::newReal(double value, int decimal_places, bool trim_trailing_zeroes)
{
    return {QPDF_Real::create(value, decimal_places, trim_trailing_zeroes)};
}

QPDFObjectHandle
QPDFObjectHandle::newName(std::string const& name)
{
    return {QPDF_Name::create(name)};
}

QPDFObjectHandle
QPDFObjectHandle::newString(std::string const& str)
{
    return {QPDF_String::create(str)};
}

QPDFObjectHandle
QPDFObjectHandle::newUnicodeString(std::string const& utf8_str)
{
    return {QPDF_String::create_utf16(utf8_str)};
}

QPDFObjectHandle
QPDFObjectHandle::newOperator(std::string const& value)
{
    return {QPDF_Operator::create(value)};
}

QPDFObjectHandle
QPDFObjectHandle::newInlineImage(std::string const& value)
{
    return {QPDF_InlineImage::create(value)};
}

QPDFObjectHandle
QPDFObjectHandle::newArray()
{
    return newArray(std::vector<QPDFObjectHandle>());
}

QPDFObjectHandle
QPDFObjectHandle::newArray(std::vector<QPDFObjectHandle> const& items)
{
    return {QPDF_Array::create(items)};
}

QPDFObjectHandle
QPDFObjectHandle::newArray(Rectangle const& rect)
{
    return newArray({newReal(rect.llx), newReal(rect.lly), newReal(rect.urx), newReal(rect.ury)});
}

QPDFObjectHandle
QPDFObjectHandle::newArray(Matrix const& matrix)
{
    return newArray(
        {newReal(matrix.a),
         newReal(matrix.b),
         newReal(matrix.c),
         newReal(matrix.d),
         newReal(matrix.e),
         newReal(matrix.f)});
}

QPDFObjectHandle
QPDFObjectHandle::newArray(QPDFMatrix const& matrix)
{
    return newArray(
        {newReal(matrix.a),
         newReal(matrix.b),
         newReal(matrix.c),
         newReal(matrix.d),
         newReal(matrix.e),
         newReal(matrix.f)});
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
QPDFObjectHandle::newDictionary(std::map<std::string, QPDFObjectHandle> const& items)
{
    return {QPDF_Dictionary::create(items)};
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf)
{
    if (qpdf == nullptr) {
        throw std::runtime_error("attempt to create stream in null qpdf object");
    }
    QTC::TC("qpdf", "QPDFObjectHandle newStream");
    return qpdf->newStream();
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, std::shared_ptr<Buffer> data)
{
    if (qpdf == nullptr) {
        throw std::runtime_error("attempt to create stream in null qpdf object");
    }
    QTC::TC("qpdf", "QPDFObjectHandle newStream with data");
    return qpdf->newStream(data);
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, std::string const& data)
{
    if (qpdf == nullptr) {
        throw std::runtime_error("attempt to create stream in null qpdf object");
    }
    QTC::TC("qpdf", "QPDFObjectHandle newStream with string");
    return qpdf->newStream(data);
}

QPDFObjectHandle
QPDFObjectHandle::newReserved(QPDF* qpdf)
{
    if (qpdf == nullptr) {
        throw std::runtime_error("attempt to create reserved object in null qpdf object");
    }
    return qpdf->newReserved();
}

void
QPDFObjectHandle::setObjectDescription(QPDF* owning_qpdf, std::string const& object_description)
{
    if (obj) {
        auto descr = std::make_shared<QPDFValue::Description>(object_description);
        obj->setDescription(owning_qpdf, descr);
    }
}

bool
QPDFObjectHandle::hasObjectDescription() const
{
    return obj && obj->hasDescription();
}

QPDFObjectHandle
QPDFObjectHandle::shallowCopy()
{
    if (!obj) {
        throw std::logic_error("operation attempted on uninitialized QPDFObjectHandle");
    }
    return {obj->copy()};
}

QPDFObjectHandle
QPDFObjectHandle::unsafeShallowCopy()
{
    if (!obj) {
        throw std::logic_error("operation attempted on uninitialized QPDFObjectHandle");
    }
    return {obj->copy(true)};
}

void
QPDFObjectHandle::makeDirect(QPDFObjGen::set& visited, bool stop_at_streams)
{
    assertInitialized();

    auto cur_og = getObjGen();
    if (!visited.add(cur_og)) {
        QTC::TC("qpdf", "QPDFObjectHandle makeDirect loop");
        throw std::runtime_error("loop detected while converting object from indirect to direct");
    }

    if (isBool() || isInteger() || isName() || isNull() || isReal() || isString()) {
        this->obj = obj->copy(true);
    } else if (isArray()) {
        std::vector<QPDFObjectHandle> items;
        auto array = as_array(strict);
        int n = array.size();
        for (int i = 0; i < n; ++i) {
            items.emplace_back(array.at(i).second);
            items.back().makeDirect(visited, stop_at_streams);
        }
        this->obj = QPDF_Array::create(items);
    } else if (isDictionary()) {
        std::map<std::string, QPDFObjectHandle> items;
        auto dict = as_dictionary(strict);
        for (auto const& key: getKeys()) {
            items[key] = dict.getKey(key);
            items[key].makeDirect(visited, stop_at_streams);
        }
        this->obj = QPDF_Dictionary::create(items);
    } else if (isStream()) {
        QTC::TC("qpdf", "QPDFObjectHandle copy stream", stop_at_streams ? 0 : 1);
        if (!stop_at_streams) {
            throw std::runtime_error("attempt to make a stream into a direct object");
        }
    } else if (isReserved()) {
        throw std::logic_error(
            "QPDFObjectHandle: attempting to make a reserved object handle direct");
    } else {
        throw std::logic_error("QPDFObjectHandle::makeDirectInternal: unknown object type");
    }

    visited.erase(cur_og);
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
    QPDFObjGen::set visited;
    makeDirect(visited, allow_streams);
}

void
QPDFObjectHandle::assertInitialized() const
{
    if (!obj) {
        throw std::logic_error("operation attempted on uninitialized QPDFObjectHandle");
    }
}

void
QPDFObjectHandle::typeWarning(char const* expected_type, std::string const& warning) const
{
    QPDF* context = nullptr;
    std::string description;
    // Type checks above guarantee that the object has been dereferenced. Nevertheless, dereference
    // throws exceptions in the test suite
    if (!obj) {
        throw std::logic_error("attempted to dereference an uninitialized QPDFObjectHandle");
    }
    obj->getDescription(context, description);
    // Null context handled by warn
    warn(
        context,
        QPDFExc(
            qpdf_e_object,
            "",
            description,
            0,
            std::string("operation for ") + expected_type + " attempted on object of type " +
                QPDFObjectHandle(*this).getTypeName() + ": " + warning));
}

void
QPDFObjectHandle::warnIfPossible(std::string const& warning) const
{
    QPDF* context = nullptr;
    std::string description;
    if (obj && obj->getDescription(context, description)) {
        warn(context, QPDFExc(qpdf_e_damaged_pdf, "", description, 0, warning));
    } else {
        *QPDFLogger::defaultLogger()->getError() << warning << "\n";
    }
}

void
QPDFObjectHandle::objectWarning(std::string const& warning) const
{
    QPDF* context = nullptr;
    std::string description;
    // Type checks above guarantee that the object is initialized.
    obj->getDescription(context, description);
    // Null context handled by warn
    warn(context, QPDFExc(qpdf_e_object, "", description, 0, warning));
}

void
QPDFObjectHandle::assertType(char const* type_name, bool istype) const
{
    if (!istype) {
        throw std::runtime_error(
            std::string("operation for ") + type_name + " attempted on object of type " +
            QPDFObjectHandle(*this).getTypeName());
    }
}

void
QPDFObjectHandle::assertNull() const
{
    assertType("null", isNull());
}

void
QPDFObjectHandle::assertBool() const
{
    assertType("boolean", isBool());
}

void
QPDFObjectHandle::assertInteger() const
{
    assertType("integer", isInteger());
}

void
QPDFObjectHandle::assertReal() const
{
    assertType("real", isReal());
}

void
QPDFObjectHandle::assertName() const
{
    assertType("name", isName());
}

void
QPDFObjectHandle::assertString() const
{
    assertType("string", isString());
}

void
QPDFObjectHandle::assertOperator() const
{
    assertType("operator", isOperator());
}

void
QPDFObjectHandle::assertInlineImage() const
{
    assertType("inlineimage", isInlineImage());
}

void
QPDFObjectHandle::assertArray() const
{
    assertType("array", isArray());
}

void
QPDFObjectHandle::assertDictionary() const
{
    assertType("dictionary", isDictionary());
}

void
QPDFObjectHandle::assertStream() const
{
    assertType("stream", isStream());
}

void
QPDFObjectHandle::assertReserved() const
{
    assertType("reserved", isReserved());
}

void
QPDFObjectHandle::assertIndirect() const
{
    if (!isIndirect()) {
        throw std::logic_error("operation for indirect object attempted on direct object");
    }
}

void
QPDFObjectHandle::assertScalar() const
{
    assertType("scalar", isScalar());
}

void
QPDFObjectHandle::assertNumber() const
{
    assertType("number", isNumber());
}

bool
QPDFObjectHandle::isPageObject() const
{
    // See comments in QPDFObjectHandle.hh.
    if (getOwningQPDF() == nullptr) {
        return false;
    }
    // getAllPages repairs /Type when traversing the page tree.
    getOwningQPDF()->getAllPages();
    return isDictionaryOfType("/Page");
}

bool
QPDFObjectHandle::isPagesObject() const
{
    if (getOwningQPDF() == nullptr) {
        return false;
    }
    // getAllPages repairs /Type when traversing the page tree.
    getOwningQPDF()->getAllPages();
    return isDictionaryOfType("/Pages");
}

bool
QPDFObjectHandle::isFormXObject() const
{
    return isStreamOfType("", "/Form");
}

bool
QPDFObjectHandle::isImage(bool exclude_imagemask) const
{
    return (
        isStreamOfType("", "/Image") &&
        ((!exclude_imagemask) ||
         (!(getDict().getKey("/ImageMask").isBool() &&
            getDict().getKey("/ImageMask").getBoolValue()))));
}

void
QPDFObjectHandle::assertPageObject() const
{
    if (!isPageObject()) {
        throw std::runtime_error("page operation called on non-Page object");
    }
}

void
QPDFObjectHandle::warn(QPDF* qpdf, QPDFExc const& e)
{
    // If parsing on behalf of a QPDF object and want to give a warning, we can warn through the
    // object. If parsing for some other reason, such as an explicit creation of an object from a
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
    ++m->iter;
    updateIValue();
    return *this;
}

QPDFObjectHandle::QPDFDictItems::iterator&
QPDFObjectHandle::QPDFDictItems::iterator::operator--()
{
    --m->iter;
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
QPDFObjectHandle::QPDFDictItems::iterator::operator==(iterator const& other) const
{
    if (m->is_end && other.m->is_end) {
        return true;
    }
    if (m->is_end || other.m->is_end) {
        return false;
    }
    return (this->ivalue.first == other.ivalue.first);
}

QPDFObjectHandle::QPDFDictItems::iterator::iterator(QPDFObjectHandle& oh, bool for_begin) :
    m(new Members(oh, for_begin))
{
    updateIValue();
}

void
QPDFObjectHandle::QPDFDictItems::iterator::updateIValue()
{
    m->is_end = (m->iter == m->keys.end());
    if (m->is_end) {
        this->ivalue.first = "";
        this->ivalue.second = QPDFObjectHandle();
    } else {
        this->ivalue.first = *(m->iter);
        this->ivalue.second = m->oh.getKey(this->ivalue.first);
    }
}

QPDFObjectHandle::QPDFDictItems::iterator::Members::Members(QPDFObjectHandle& oh, bool for_begin) :
    oh(oh)
{
    this->keys = oh.getKeys();
    this->iter = for_begin ? this->keys.begin() : this->keys.end();
}

QPDFObjectHandle::QPDFDictItems::iterator
QPDFObjectHandle::QPDFDictItems::begin()
{
    return {oh, true};
}

QPDFObjectHandle::QPDFDictItems::iterator
QPDFObjectHandle::QPDFDictItems::end()
{
    return {oh, false};
}

QPDFObjectHandle::QPDFArrayItems::QPDFArrayItems(QPDFObjectHandle const& oh) :
    oh(oh)
{
}

QPDFObjectHandle::QPDFArrayItems::iterator&
QPDFObjectHandle::QPDFArrayItems::iterator::operator++()
{
    if (!m->is_end) {
        ++m->item_number;
        updateIValue();
    }
    return *this;
}

QPDFObjectHandle::QPDFArrayItems::iterator&
QPDFObjectHandle::QPDFArrayItems::iterator::operator--()
{
    if (m->item_number > 0) {
        --m->item_number;
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
QPDFObjectHandle::QPDFArrayItems::iterator::operator==(iterator const& other) const
{
    return (m->item_number == other.m->item_number);
}

QPDFObjectHandle::QPDFArrayItems::iterator::iterator(QPDFObjectHandle& oh, bool for_begin) :
    m(new Members(oh, for_begin))
{
    updateIValue();
}

void
QPDFObjectHandle::QPDFArrayItems::iterator::updateIValue()
{
    m->is_end = (m->item_number >= m->oh.getArrayNItems());
    if (m->is_end) {
        this->ivalue = QPDFObjectHandle();
    } else {
        this->ivalue = m->oh.getArrayItem(m->item_number);
    }
}

QPDFObjectHandle::QPDFArrayItems::iterator::Members::Members(QPDFObjectHandle& oh, bool for_begin) :
    oh(oh)
{
    this->item_number = for_begin ? 0 : oh.getArrayNItems();
}

QPDFObjectHandle::QPDFArrayItems::iterator
QPDFObjectHandle::QPDFArrayItems::begin()
{
    return {oh, true};
}

QPDFObjectHandle::QPDFArrayItems::iterator
QPDFObjectHandle::QPDFArrayItems::end()
{
    return {oh, false};
}

QPDFObjGen
QPDFObjectHandle::getObjGen() const
{
    return obj ? obj->getObjGen() : QPDFObjGen();
}

// Indirect object accessors
QPDF*
QPDFObjectHandle::getOwningQPDF() const
{
    return obj ? obj->getQPDF() : nullptr;
}

QPDF&
QPDFObjectHandle::getQPDF(std::string const& error_msg) const
{
    if (auto result = obj ? obj->getQPDF() : nullptr) {
        return *result;
    }
    throw std::runtime_error(error_msg.empty() ? "attempt to use a null qpdf object" : error_msg);
}

void
QPDFObjectHandle::setParsedOffset(qpdf_offset_t offset)
{
    if (obj) {
        obj->setParsedOffset(offset);
    }
}

QPDFObjectHandle
operator""_qpdf(char const* v, size_t len)
{
    return QPDFObjectHandle::parse(std::string(v, len), "QPDFObjectHandle literal");
}
