#include <qpdf/QPDFObjectHandle.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDF_Bool.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Integer.hh>
#include <qpdf/QPDF_Real.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QPDF_String.hh>
#include <qpdf/QPDF_Operator.hh>
#include <qpdf/QPDF_InlineImage.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_Stream.hh>
#include <qpdf/QPDF_Reserved.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Concatenate.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/SparseOHArray.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QIntC.hh>

#include <stdexcept>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <cstring>
#include <algorithm>

class TerminateParsing
{
};

QPDFObjectHandle::StreamDataProvider::StreamDataProvider(
    bool supports_retry) :
    supports_retry(supports_retry)
{
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
    int objid, int generation, Pipeline* pipeline,
    bool suppress_warnings, bool will_retry)
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

class CoalesceProvider: public QPDFObjectHandle::StreamDataProvider
{
  public:
    CoalesceProvider(QPDFObjectHandle containing_page,
                     QPDFObjectHandle old_contents) :
        containing_page(containing_page),
        old_contents(old_contents)
    {
    }
    virtual ~CoalesceProvider()
    {
    }
    virtual void provideStreamData(int objid, int generation,
                                   Pipeline* pipeline);

  private:
    QPDFObjectHandle containing_page;
    QPDFObjectHandle old_contents;
};

void
CoalesceProvider::provideStreamData(int, int, Pipeline* p)
{
    QTC::TC("qpdf", "QPDFObjectHandle coalesce provide stream data");
    Pl_Concatenate concat("concatenate", p);
    std::string description = "page object " +
        QUtil::int_to_string(containing_page.getObjectID()) + " " +
        QUtil::int_to_string(containing_page.getGeneration());
    std::string all_description;
    old_contents.pipeContentStreams(&concat, description, all_description);
    concat.manualFinish();
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
    if (! this->pipeline)
    {
        return;
    }
    if (len)
    {
	this->pipeline->write(QUtil::unsigned_char_pointer(data), len);
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

class LastChar: public Pipeline
{
  public:
    LastChar(Pipeline* next);
    virtual ~LastChar() = default;
    virtual void write(unsigned char* data, size_t len);
    virtual void finish();
    unsigned char getLastChar();

  private:
    unsigned char last_char;
};

LastChar::LastChar(Pipeline* next) :
    Pipeline("lastchar", next),
    last_char(0)
{
}

void
LastChar::write(unsigned char* data, size_t len)
{
    if (len > 0)
    {
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

QPDFObjectHandle::QPDFObjectHandle() :
    initialized(false),
    qpdf(0),
    objid(0),
    generation(0),
    reserved(false)
{
}

QPDFObjectHandle::QPDFObjectHandle(QPDF* qpdf, int objid, int generation) :
    initialized(true),
    qpdf(qpdf),
    objid(objid),
    generation(generation),
    reserved(false)
{
}

QPDFObjectHandle::QPDFObjectHandle(QPDFObject* data) :
    initialized(true),
    qpdf(0),
    objid(0),
    generation(0),
    obj(data),
    reserved(false)
{
}

void
QPDFObjectHandle::releaseResolved()
{
    // Recursively break any resolved references to indirect objects.
    // Do not cross over indirect object boundaries to avoid an
    // infinite loop.  This method may only be called during final
    // destruction.  See comments in QPDF::~QPDF().
    if (isIndirect())
    {
	if (this->obj.getPointer())
	{
	    this->obj = 0;
	}
    }
    else
    {
	QPDFObject::ObjAccessor::releaseResolved(this->obj.getPointer());
    }
}

void
QPDFObjectHandle::setObjectDescriptionFromInput(
    QPDFObjectHandle object, QPDF* context,
    std::string const& description, PointerHolder<InputSource> input,
    qpdf_offset_t offset)
{
    object.setObjectDescription(
        context,
        input->getName() + ", " + description +
        " at offset " + QUtil::int_to_string(offset));
}

bool
QPDFObjectHandle::isInitialized() const
{
    return this->initialized;
}

QPDFObject::object_type_e
QPDFObjectHandle::getTypeCode()
{
    if (this->initialized)
    {
        dereference();
        return this->obj->getTypeCode();
    }
    else
    {
        return QPDFObject::ot_uninitialized;
    }
}

char const*
QPDFObjectHandle::getTypeName()
{
    if (this->initialized)
    {
        dereference();
        return this->obj->getTypeName();
    }
    else
    {
        return "uninitialized";
    }
}

template <class T>
class QPDFObjectTypeAccessor
{
  public:
    static bool check(QPDFObject* o)
    {
	return (o && dynamic_cast<T*>(o));
    }
    static bool check(QPDFObject const* o)
    {
	return (o && dynamic_cast<T const*>(o));
    }
};

bool
QPDFObjectHandle::isBool()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Bool>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isDirectNull() const
{
    return (this->initialized && (this->objid == 0) &&
            QPDFObjectTypeAccessor<QPDF_Null>::check(obj.getPointer()));
}

bool
QPDFObjectHandle::isNull()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Null>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isInteger()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Integer>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isReal()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Real>::check(obj.getPointer());
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
    if (isInteger())
    {
	result = static_cast<double>(getIntValue());
    }
    else if (isReal())
    {
	result = atof(getRealValue().c_str());
    }
    else
    {
        typeWarning("number", "returning 0");
        QTC::TC("qpdf", "QPDFObjectHandle numeric non-numeric");
    }
    return result;
}

bool
QPDFObjectHandle::isName()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Name>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isString()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_String>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isOperator()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Operator>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isInlineImage()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_InlineImage>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isArray()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Array>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isDictionary()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Dictionary>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isStream()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Stream>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isReserved()
{
    // dereference will clear reserved if this has been replaced
    dereference();
    return this->reserved;
}

bool
QPDFObjectHandle::isIndirect()
{
    assertInitialized();
    return (this->objid != 0);
}

bool
QPDFObjectHandle::isScalar()
{
    return (! (isArray() || isDictionary() || isStream() ||
               isOperator() || isInlineImage()));
}

// Bool accessors

bool
QPDFObjectHandle::getBoolValue()
{
    if (isBool())
    {
        return dynamic_cast<QPDF_Bool*>(obj.getPointer())->getVal();
    }
    else
    {
        typeWarning("boolean", "returning false");
        QTC::TC("qpdf", "QPDFObjectHandle boolean returning false");
        return false;
    }
}

// Integer accessors

long long
QPDFObjectHandle::getIntValue()
{
    if (isInteger())
    {
        return dynamic_cast<QPDF_Integer*>(obj.getPointer())->getVal();
    }
    else
    {
        typeWarning("integer", "returning 0");
        QTC::TC("qpdf", "QPDFObjectHandle integer returning 0");
        return 0;
    }
}

int
QPDFObjectHandle::getIntValueAsInt()
{
    int result = 0;
    long long v = getIntValue();
    if (v < INT_MIN)
    {
        QTC::TC("qpdf", "QPDFObjectHandle int returning INT_MIN");
        warnIfPossible(
            "requested value of integer is too small; returning INT_MIN",
            false);
        result = INT_MIN;
    }
    else if (v > INT_MAX)
    {
        QTC::TC("qpdf", "QPDFObjectHandle int returning INT_MAX");
        warnIfPossible(
            "requested value of integer is too big; returning INT_MAX",
            false);
        result = INT_MAX;
    }
    else
    {
        result = static_cast<int>(v);
    }
    return result;
}

unsigned long long
QPDFObjectHandle::getUIntValue()
{
    unsigned long long result = 0;
    long long v = getIntValue();
    if (v < 0)
    {
        QTC::TC("qpdf", "QPDFObjectHandle uint returning 0");
        warnIfPossible(
            "unsigned value request for negative number; returning 0",
            false);
    }
    else
    {
        result = static_cast<unsigned long long>(v);
    }
    return result;
}

unsigned int
QPDFObjectHandle::getUIntValueAsUInt()
{
    unsigned int result = 0;
    long long v = getIntValue();
    if (v < 0)
    {
        QTC::TC("qpdf", "QPDFObjectHandle uint uint returning 0");
        warnIfPossible(
            "unsigned integer value request for negative number; returning 0",
            false);
        result = 0;
    }
    else if (v > UINT_MAX)
    {
        QTC::TC("qpdf", "QPDFObjectHandle uint returning UINT_MAX");
        warnIfPossible(
            "requested value of unsigned integer is too big;"
            " returning UINT_MAX",
            false);
        result = UINT_MAX;
    }
    else
    {
        result = static_cast<unsigned int>(v);
    }
    return result;
}

// Real accessors

std::string
QPDFObjectHandle::getRealValue()
{
    if (isReal())
    {
        return dynamic_cast<QPDF_Real*>(obj.getPointer())->getVal();
    }
    else
    {
        typeWarning("real", "returning 0.0");
        QTC::TC("qpdf", "QPDFObjectHandle real returning 0.0");
        return "0.0";
    }
}

// Name accessors

std::string
QPDFObjectHandle::getName()
{
    if (isName())
    {
        return dynamic_cast<QPDF_Name*>(obj.getPointer())->getName();
    }
    else
    {
        typeWarning("name", "returning dummy name");
        QTC::TC("qpdf", "QPDFObjectHandle name returning dummy name");
        return "/QPDFFakeName";
    }
}

// String accessors

std::string
QPDFObjectHandle::getStringValue()
{
    if (isString())
    {
        return dynamic_cast<QPDF_String*>(obj.getPointer())->getVal();
    }
    else
    {
        typeWarning("string", "returning empty string");
        QTC::TC("qpdf", "QPDFObjectHandle string returning empty string");
        return "";
    }
}

std::string
QPDFObjectHandle::getUTF8Value()
{
    if (isString())
    {
        return dynamic_cast<QPDF_String*>(obj.getPointer())->getUTF8Val();
    }
    else
    {
        typeWarning("string", "returning empty string");
        QTC::TC("qpdf", "QPDFObjectHandle string returning empty utf8");
        return "";
    }
}

// Operator and Inline Image accessors

std::string
QPDFObjectHandle::getOperatorValue()
{
    if (isOperator())
    {
        return dynamic_cast<QPDF_Operator*>(obj.getPointer())->getVal();
    }
    else
    {
        typeWarning("operator", "returning fake value");
        QTC::TC("qpdf", "QPDFObjectHandle operator returning fake value");
        return "QPDFFAKE";
    }
}

std::string
QPDFObjectHandle::getInlineImageValue()
{
    if (isInlineImage())
    {
        return dynamic_cast<QPDF_InlineImage*>(obj.getPointer())->getVal();
    }
    else
    {
        typeWarning("inlineimage", "returning empty data");
        QTC::TC("qpdf", "QPDFObjectHandle inlineimage returning empty data");
        return "";
    }
}

// Array accessors

int
QPDFObjectHandle::getArrayNItems()
{
    if (isArray())
    {
        return dynamic_cast<QPDF_Array*>(obj.getPointer())->getNItems();
    }
    else
    {
        typeWarning("array", "treating as empty");
        QTC::TC("qpdf", "QPDFObjectHandle array treating as empty");
        return 0;
    }
}

QPDFObjectHandle
QPDFObjectHandle::getArrayItem(int n)
{
    QPDFObjectHandle result;
    if (isArray() && (n < getArrayNItems()) && (n >= 0))
    {
        result = dynamic_cast<QPDF_Array*>(obj.getPointer())->getItem(n);
    }
    else
    {
        result = newNull();
        if (isArray())
        {
            objectWarning("returning null for out of bounds array access");
            QTC::TC("qpdf", "QPDFObjectHandle array bounds");
        }
        else
        {
            typeWarning("array", "returning null");
            QTC::TC("qpdf", "QPDFObjectHandle array null for non-array");
        }
        QPDF* context = 0;
        std::string description;
        if (this->obj->getDescription(context, description))
        {
            result.setObjectDescription(
                context,
                description +
                " -> null returned from invalid array access");
        }
    }
    return result;
}

bool
QPDFObjectHandle::isRectangle()
{
    if (! isArray())
    {
        return false;
    }
    if (getArrayNItems() != 4)
    {
        return false;
    }
    for (int i = 0; i < 4; ++i)
    {
        if (! getArrayItem(i).isNumber())
        {
            return false;
        }
    }
    return true;
}

bool
QPDFObjectHandle::isMatrix()
{
    if (! isArray())
    {
        return false;
    }
    if (getArrayNItems() != 6)
    {
        return false;
    }
    for (int i = 0; i < 6; ++i)
    {
        if (! getArrayItem(i).isNumber())
        {
            return false;
        }
    }
    return true;
}

QPDFObjectHandle::Rectangle
QPDFObjectHandle::getArrayAsRectangle()
{
    Rectangle result;
    if (isRectangle())
    {
        // Rectangle coordinates are always supposed to be llx, lly,
        // urx, ury, but files have been found in the wild where
        // llx > urx or lly > ury.
        double i0 = getArrayItem(0).getNumericValue();
        double i1 = getArrayItem(1).getNumericValue();
        double i2 = getArrayItem(2).getNumericValue();
        double i3 = getArrayItem(3).getNumericValue();
        result = Rectangle(std::min(i0, i2),
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
    if (isMatrix())
    {
        result = Matrix(getArrayItem(0).getNumericValue(),
                        getArrayItem(1).getNumericValue(),
                        getArrayItem(2).getNumericValue(),
                        getArrayItem(3).getNumericValue(),
                        getArrayItem(4).getNumericValue(),
                        getArrayItem(5).getNumericValue());
    }
    return result;
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getArrayAsVector()
{
    std::vector<QPDFObjectHandle> result;
    if (isArray())
    {
        dynamic_cast<QPDF_Array*>(obj.getPointer())->getAsVector(result);
    }
    else
    {
        typeWarning("array", "treating as empty");
        QTC::TC("qpdf", "QPDFObjectHandle array treating as empty vector");
    }
    return result;
}

// Array mutators

void
QPDFObjectHandle::setArrayItem(int n, QPDFObjectHandle const& item)
{
    if (isArray())
    {
        dynamic_cast<QPDF_Array*>(obj.getPointer())->setItem(n, item);
    }
    else
    {
        typeWarning("array", "ignoring attempt to set item");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring set item");
    }
}

void
QPDFObjectHandle::setArrayFromVector(std::vector<QPDFObjectHandle> const& items)
{
    if (isArray())
    {
        dynamic_cast<QPDF_Array*>(obj.getPointer())->setFromVector(items);
    }
    else
    {
        typeWarning("array", "ignoring attempt to replace items");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring replace items");
    }
}

void
QPDFObjectHandle::insertItem(int at, QPDFObjectHandle const& item)
{
    if (isArray())
    {
        dynamic_cast<QPDF_Array*>(obj.getPointer())->insertItem(at, item);
    }
    else
    {
        typeWarning("array", "ignoring attempt to insert item");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring insert item");
    }
}

void
QPDFObjectHandle::appendItem(QPDFObjectHandle const& item)
{
    if (isArray())
    {
        dynamic_cast<QPDF_Array*>(obj.getPointer())->appendItem(item);
    }
    else
    {
        typeWarning("array", "ignoring attempt to append item");
        QTC::TC("qpdf", "QPDFObjectHandle array ignoring append item");
    }
}

void
QPDFObjectHandle::eraseItem(int at)
{
    if (isArray() && (at < getArrayNItems()) && (at >= 0))
    {
        dynamic_cast<QPDF_Array*>(obj.getPointer())->eraseItem(at);
    }
    else
    {
        if (isArray())
        {
            objectWarning("ignoring attempt to erase out of bounds array item");
            QTC::TC("qpdf", "QPDFObjectHandle erase array bounds");
        }
        else
        {
            typeWarning("array", "ignoring attempt to erase item");
            QTC::TC("qpdf", "QPDFObjectHandle array ignoring erase item");
        }
    }
}

// Dictionary accessors

bool
QPDFObjectHandle::hasKey(std::string const& key)
{
    if (isDictionary())
    {
        return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->hasKey(key);
    }
    else
    {
        typeWarning("dictionary",
                    "returning false for a key containment request");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary false for hasKey");
        return false;
    }
}

QPDFObjectHandle
QPDFObjectHandle::getKey(std::string const& key)
{
    QPDFObjectHandle result;
    if (isDictionary())
    {
        result = dynamic_cast<QPDF_Dictionary*>(
            obj.getPointer())->getKey(key);
    }
    else
    {
        typeWarning(
            "dictionary", "returning null for attempted key retrieval");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary null for getKey");
        result = newNull();
        QPDF* qpdf = 0;
        std::string description;
        if (this->obj->getDescription(qpdf, description))
        {
            result.setObjectDescription(
                qpdf,
                description +
                " -> null returned from getting key " +
                key + " from non-Dictionary");
        }
    }
    return result;
}

std::set<std::string>
QPDFObjectHandle::getKeys()
{
    std::set<std::string> result;
    if (isDictionary())
    {
        result = dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->getKeys();
    }
    else
    {
        typeWarning("dictionary", "treating as empty");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary empty set for getKeys");
    }
    return result;
}

std::map<std::string, QPDFObjectHandle>
QPDFObjectHandle::getDictAsMap()
{
    std::map<std::string, QPDFObjectHandle> result;
    if (isDictionary())
    {
        result = dynamic_cast<QPDF_Dictionary*>(
            obj.getPointer())->getAsMap();
    }
    else
    {
        typeWarning("dictionary", "treating as empty");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary empty map for asMap");
    }
    return result;
}

// Array and Name accessors
bool
QPDFObjectHandle::isOrHasName(std::string const& value)
{
    if (isName() && (getName() == value))
    {
	return true;
    }
    else if (isArray())
    {
	int n = getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    QPDFObjectHandle item = getArrayItem(0);
	    if (item.isName() && (item.getName() == value))
	    {
		return true;
	    }
	}
    }
    return false;
}

void
QPDFObjectHandle::mergeResources(QPDFObjectHandle other)
{
    if (! (isDictionary() && other.isDictionary()))
    {
        QTC::TC("qpdf", "QPDFObjectHandle merge top type mismatch");
        return;
    }
    std::set<std::string> other_keys = other.getKeys();
    for (std::set<std::string>::iterator iter = other_keys.begin();
         iter != other_keys.end(); ++iter)
    {
        std::string const& key = *iter;
        QPDFObjectHandle other_val = other.getKey(key);
        if (hasKey(key))
        {
            QPDFObjectHandle this_val = getKey(key);
            if (this_val.isDictionary() && other_val.isDictionary())
            {
                if (this_val.isIndirect())
                {
                    QTC::TC("qpdf", "QPDFObjectHandle replace with copy");
                    this_val = this_val.shallowCopy();
                    replaceKey(key, this_val);
                }
                std::set<std::string> other_val_keys = other_val.getKeys();
                for (std::set<std::string>::iterator i2 =
                         other_val_keys.begin();
                     i2 != other_val_keys.end(); ++i2)
                {
                    if (! this_val.hasKey(*i2))
                    {
                        QTC::TC("qpdf", "QPDFObjectHandle merge shallow copy");
                        this_val.replaceKey(
                            *i2, other_val.getKey(*i2).shallowCopy());
                    }
                }
            }
            else if (this_val.isArray() && other_val.isArray())
            {
                std::set<std::string> scalars;
                int n = this_val.getArrayNItems();
                for (int i = 0; i < n; ++i)
                {
                    QPDFObjectHandle this_item = this_val.getArrayItem(i);
                    if (this_item.isScalar())
                    {
                        scalars.insert(this_item.unparse());
                    }
                }
                n = other_val.getArrayNItems();
                for (int i = 0; i < n; ++i)
                {
                    QPDFObjectHandle other_item = other_val.getArrayItem(i);
                    if (other_item.isScalar())
                    {
                        if (scalars.count(other_item.unparse()) == 0)
                        {
                            QTC::TC("qpdf", "QPDFObjectHandle merge array");
                            this_val.appendItem(other_item);
                        }
                        else
                        {
                            QTC::TC("qpdf", "QPDFObjectHandle merge array dup");
                        }
                    }
                }
            }
        }
        else
        {
            QTC::TC("qpdf", "QPDFObjectHandle merge copy from other");
            replaceKey(key, other_val.shallowCopy());
        }
    }
}

std::set<std::string>
QPDFObjectHandle::getResourceNames()
{
    // Return second-level dictionary keys
    std::set<std::string> result;
    if (! isDictionary())
    {
        return result;
    }
    std::set<std::string> keys = getKeys();
    for (std::set<std::string>::iterator iter = keys.begin();
         iter != keys.end(); ++iter)
    {
        std::string const& key = *iter;
        QPDFObjectHandle val = getKey(key);
        if (val.isDictionary())
        {
            std::set<std::string> val_keys = val.getKeys();
            for (std::set<std::string>::iterator i2 = val_keys.begin();
                 i2 != val_keys.end(); ++i2)
            {
                result.insert(*i2);
            }
        }
    }
    return result;
}

std::string
QPDFObjectHandle::getUniqueResourceName(std::string const& prefix,
                                        int& min_suffix)
{
    std::set<std::string> names = getResourceNames();
    int max_suffix = min_suffix + QIntC::to_int(names.size());
    while (min_suffix <= max_suffix)
    {
        std::string candidate = prefix + QUtil::int_to_string(min_suffix);
        if (names.count(candidate) == 0)
        {
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

// Indirect object accessors
QPDF*
QPDFObjectHandle::getOwningQPDF()
{
    // Will be null for direct objects
    return this->qpdf;
}

// Dictionary mutators

void
QPDFObjectHandle::replaceKey(std::string const& key,
			    QPDFObjectHandle value)
{
    if (isDictionary())
    {
        dynamic_cast<QPDF_Dictionary*>(
            obj.getPointer())->replaceKey(key, value);
    }
    else
    {
        typeWarning("dictionary", "ignoring key replacement request");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary ignoring replaceKey");
    }
}

void
QPDFObjectHandle::removeKey(std::string const& key)
{
    if (isDictionary())
    {
        dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->removeKey(key);
    }
    else
    {
        typeWarning("dictionary", "ignoring key removal request");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary ignoring removeKey");
    }
}

void
QPDFObjectHandle::replaceOrRemoveKey(std::string const& key,
				     QPDFObjectHandle value)
{
    if (isDictionary())
    {
        dynamic_cast<QPDF_Dictionary*>(
            obj.getPointer())->replaceOrRemoveKey(key, value);
    }
    else
    {
        typeWarning("dictionary", "ignoring key removal/replacement request");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary ignoring removereplace");
    }
}

// Stream accessors
QPDFObjectHandle
QPDFObjectHandle::getDict()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getDict();
}

void
QPDFObjectHandle::setFilterOnWrite(bool val)
{
    assertStream();
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->setFilterOnWrite(val);
}

bool
QPDFObjectHandle::getFilterOnWrite()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getFilterOnWrite();
}

bool
QPDFObjectHandle::isDataModified()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->isDataModified();
}

void
QPDFObjectHandle::replaceDict(QPDFObjectHandle new_dict)
{
    assertStream();
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceDict(new_dict);
}

PointerHolder<Buffer>
QPDFObjectHandle::getStreamData(qpdf_stream_decode_level_e level)
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(
        obj.getPointer())->getStreamData(level);
}

PointerHolder<Buffer>
QPDFObjectHandle::getRawStreamData()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getRawStreamData();
}

bool
QPDFObjectHandle::pipeStreamData(Pipeline* p, bool* filtering_attempted,
                                 int encode_flags,
                                 qpdf_stream_decode_level_e decode_level,
                                 bool suppress_warnings, bool will_retry)
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->pipeStreamData(
	p, filtering_attempted, encode_flags, decode_level,
        suppress_warnings, will_retry);
}

bool
QPDFObjectHandle::pipeStreamData(Pipeline* p,
                                 int encode_flags,
                                 qpdf_stream_decode_level_e decode_level,
                                 bool suppress_warnings, bool will_retry)
{
    assertStream();
    bool filtering_attempted;
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->pipeStreamData(
	p, &filtering_attempted, encode_flags, decode_level,
        suppress_warnings, will_retry);
    return filtering_attempted;
}

bool
QPDFObjectHandle::pipeStreamData(Pipeline* p, bool filter,
				 bool normalize, bool compress)
{
    int encode_flags = 0;
    qpdf_stream_decode_level_e decode_level = qpdf_dl_none;
    if (filter)
    {
        decode_level = qpdf_dl_generalized;
        if (normalize)
        {
            encode_flags |= qpdf_ef_normalize;
        }
        if (compress)
        {
            encode_flags |= qpdf_ef_compress;
        }
    }
    return pipeStreamData(p, encode_flags, decode_level, false);
}

void
QPDFObjectHandle::replaceStreamData(PointerHolder<Buffer> data,
				    QPDFObjectHandle const& filter,
				    QPDFObjectHandle const& decode_parms)
{
    assertStream();
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceStreamData(
	data, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(std::string const& data,
				    QPDFObjectHandle const& filter,
				    QPDFObjectHandle const& decode_parms)
{
    assertStream();
    PointerHolder<Buffer> b = new Buffer(data.length());
    unsigned char* bp = b->getBuffer();
    memcpy(bp, data.c_str(), data.length());
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceStreamData(
	b, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(PointerHolder<StreamDataProvider> provider,
				    QPDFObjectHandle const& filter,
				    QPDFObjectHandle const& decode_parms)
{
    assertStream();
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceStreamData(
	provider, filter, decode_parms);
}

QPDFObjGen
QPDFObjectHandle::getObjGen() const
{
    return QPDFObjGen(this->objid, this->generation);
}

int
QPDFObjectHandle::getObjectID() const
{
    return this->objid;
}

int
QPDFObjectHandle::getGeneration() const
{
    return this->generation;
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
    if (isArray())
    {
	int n_items = getArrayNItems();
	for (int i = 0; i < n_items; ++i)
	{
	    QPDFObjectHandle item = getArrayItem(i);
	    if (item.isStream())
            {
                result.push_back(item);
            }
            else
	    {
                QTC::TC("qpdf", "QPDFObjectHandle non-stream in stream array");
                warn(item.getOwningQPDF(),
                     QPDFExc(qpdf_e_damaged_pdf, description,
                             "item index " + QUtil::int_to_string(i) +
                             " (from 0)", 0,
                             "ignoring non-stream in an array of streams"));
	    }
	}
    }
    else if (isStream())
    {
	result.push_back(*this);
    }
    else if (! isNull())
    {
        warn(getOwningQPDF(),
             QPDFExc(qpdf_e_damaged_pdf, "", description, 0,
                     " object is supposed to be a stream or an"
                     " array of streams but is neither"));
    }

    bool first = true;
    for (std::vector<QPDFObjectHandle>::iterator iter = result.begin();
         iter != result.end(); ++iter)
    {
        QPDFObjectHandle item = *iter;
        std::string og =
            QUtil::int_to_string(item.getObjectID()) + " " +
            QUtil::int_to_string(item.getGeneration());
        if (first)
        {
            first = false;
        }
        else
        {
            all_description += ",";
        }
        all_description += " stream " + og;
    }

    return result;
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getPageContents()
{
    std::string description = "page object " +
        QUtil::int_to_string(this->objid) + " " +
        QUtil::int_to_string(this->generation);
    std::string all_description;
    return this->getKey("/Contents").arrayOrStreamToStreamArray(
        description, all_description);
}

void
QPDFObjectHandle::addPageContents(QPDFObjectHandle new_contents, bool first)
{
    new_contents.assertStream();

    std::vector<QPDFObjectHandle> orig_contents = getPageContents();

    std::vector<QPDFObjectHandle> content_streams;
    if (first)
    {
	QTC::TC("qpdf", "QPDFObjectHandle prepend page contents");
	content_streams.push_back(new_contents);
    }
    for (std::vector<QPDFObjectHandle>::iterator iter = orig_contents.begin();
	 iter != orig_contents.end(); ++iter)
    {
	QTC::TC("qpdf", "QPDFObjectHandle append page contents");
	content_streams.push_back(*iter);
    }
    if (! first)
    {
	content_streams.push_back(new_contents);
    }

    QPDFObjectHandle contents = QPDFObjectHandle::newArray(content_streams);
    this->replaceKey("/Contents", contents);
}

void
QPDFObjectHandle::rotatePage(int angle, bool relative)
{
    if ((angle % 90) != 0)
    {
        throw std::runtime_error(
            "QPDF::rotatePage called with an"
            " angle that is not a multiple of 90");
    }
    int new_angle = angle;
    if (relative)
    {
        int old_angle = 0;
        bool found_rotate = false;
        QPDFObjectHandle cur_obj = *this;
        bool searched_parent = false;
        std::set<QPDFObjGen> visited;
        while (! found_rotate)
        {
            if (visited.count(cur_obj.getObjGen()))
            {
                // Don't get stuck in an infinite loop
                break;
            }
            if (! visited.empty())
            {
                searched_parent = true;
            }
            visited.insert(cur_obj.getObjGen());
            if (cur_obj.getKey("/Rotate").isInteger())
            {
                found_rotate = true;
                old_angle = cur_obj.getKey("/Rotate").getIntValueAsInt();
            }
            else if (cur_obj.getKey("/Parent").isDictionary())
            {
                cur_obj = cur_obj.getKey("/Parent");
            }
            else
            {
                break;
            }
        }
        QTC::TC("qpdf", "QPDFObjectHandle found old angle",
                searched_parent ? 0 : 1);
        if ((old_angle % 90) != 0)
        {
            old_angle = 0;
        }
        new_angle += old_angle;
    }
    new_angle = (new_angle + 360) % 360;
    replaceKey("/Rotate", QPDFObjectHandle::newInteger(new_angle));
}

void
QPDFObjectHandle::coalesceContentStreams()
{
    QPDFObjectHandle contents = this->getKey("/Contents");
    if (contents.isStream())
    {
        QTC::TC("qpdf", "QPDFObjectHandle coalesce called on stream");
        return;
    }
    else if (! contents.isArray())
    {
        // /Contents is optional for pages, and some very damaged
        // files may have pages that are invalid in other ways.
        return;
    }
    QPDF* qpdf = getOwningQPDF();
    if (qpdf == 0)
    {
        // Should not be possible for a page object to not have an
        // owning PDF unless it was manually constructed in some
        // incorrect way. However, it can happen in a PDF file whose
        // page structure is direct, which is against spec but still
        // possible to hand construct, as in fuzz issue 27393.
        throw std::runtime_error("coalesceContentStreams called on object"
                                 " with no associated PDF file");
    }
    QPDFObjectHandle new_contents = newStream(qpdf);
    this->replaceKey("/Contents", new_contents);

    PointerHolder<StreamDataProvider> provider =
        new CoalesceProvider(*this, contents);
    new_contents.replaceStreamData(provider, newNull(), newNull());
}

std::string
QPDFObjectHandle::unparse()
{
    std::string result;
    if (this->isIndirect())
    {
	result = QUtil::int_to_string(this->objid) + " " +
	    QUtil::int_to_string(this->generation) + " R";
    }
    else
    {
	result = unparseResolved();
    }
    return result;
}

std::string
QPDFObjectHandle::unparseResolved()
{
    if (this->reserved)
    {
        throw std::logic_error(
            "QPDFObjectHandle: attempting to unparse a reserved object");
    }
    dereference();
    return this->obj->unparse();
}

std::string
QPDFObjectHandle::unparseBinary()
{
    if (this->isString())
    {
        return dynamic_cast<QPDF_String*>(
            this->obj.getPointer())->unparse(true);
    }
    else
    {
        return unparse();
    }
}

JSON
QPDFObjectHandle::getJSON(bool dereference_indirect)
{
    if ((! dereference_indirect) && this->isIndirect())
    {
        return JSON::makeString(unparse());
    }
    else
    {
        if (this->reserved)
        {
            throw std::logic_error(
                "QPDFObjectHandle: attempting to unparse a reserved object");
        }
        dereference();
        return this->obj->getJSON();
    }
}

QPDFObjectHandle
QPDFObjectHandle::wrapInArray()
{
    if (isArray())
    {
        return *this;
    }
    QPDFObjectHandle result = QPDFObjectHandle::newArray();
    result.appendItem(*this);
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::parse(std::string const& object_str,
                        std::string const& object_description)
{
    PointerHolder<InputSource> input =
        new BufferInputSource("parsed object", object_str);
    QPDFTokenizer tokenizer;
    bool empty = false;
    QPDFObjectHandle result =
        parse(input, object_description, tokenizer, empty, 0, 0);
    size_t offset = QIntC::to_size(input->tell());
    while (offset < object_str.length())
    {
        if (! isspace(object_str.at(offset)))
        {
            QTC::TC("qpdf", "QPDFObjectHandle trailing data in parse");
            throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
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
    std::string description = "page object " +
        QUtil::int_to_string(this->objid) + " " +
        QUtil::int_to_string(this->generation);
    std::string all_description;
    this->getKey("/Contents").pipeContentStreams(
        p, description, all_description);
}

void
QPDFObjectHandle::pipeContentStreams(
    Pipeline* p, std::string const& description, std::string& all_description)
{
    std::vector<QPDFObjectHandle> streams =
        arrayOrStreamToStreamArray(
            description, all_description);
    bool need_newline = false;
    for (std::vector<QPDFObjectHandle>::iterator iter = streams.begin();
         iter != streams.end(); ++iter)
    {
        if (need_newline)
        {
            p->write(QUtil::unsigned_char_pointer("\n"), 1);
        }
        LastChar lc(p);
        QPDFObjectHandle stream = *iter;
        std::string og =
            QUtil::int_to_string(stream.getObjectID()) + " " +
            QUtil::int_to_string(stream.getGeneration());
        std::string w_description = "content stream object " + og;
        if (! stream.pipeStreamData(&lc, 0, qpdf_dl_specialized))
        {
            QTC::TC("qpdf", "QPDFObjectHandle errors in parsecontent");
            throw QPDFExc(qpdf_e_damaged_pdf, "content stream",
                          w_description, 0,
                          "errors while decoding content stream");
        }
        lc.finish();
        need_newline = (lc.getLastChar() != static_cast<unsigned char>('\n'));
        QTC::TC("qpdf", "QPDFObjectHandle need_newline",
                need_newline ? 0 : 1);
    }
}

void
QPDFObjectHandle::parsePageContents(ParserCallbacks* callbacks)
{
    std::string description = "page object " +
        QUtil::int_to_string(this->objid) + " " +
        QUtil::int_to_string(this->generation);
    this->getKey("/Contents").parseContentStream_internal(
        description, callbacks);
}

void
QPDFObjectHandle::filterPageContents(TokenFilter* filter, Pipeline* next)
{
    std::string description = "token filter for page object " +
        QUtil::int_to_string(this->objid) + " " +
        QUtil::int_to_string(this->generation);
    Pl_QPDFTokenizer token_pipeline(description.c_str(), filter, next);
    this->pipePageContents(&token_pipeline);
}

void
QPDFObjectHandle::filterAsContents(TokenFilter* filter, Pipeline* next)
{
    std::string description = "token filter for object " +
        QUtil::int_to_string(this->objid) + " " +
        QUtil::int_to_string(this->generation);
    Pl_QPDFTokenizer token_pipeline(description.c_str(), filter, next);
    this->pipeStreamData(&token_pipeline, 0, qpdf_dl_specialized);
}

void
QPDFObjectHandle::parseContentStream(QPDFObjectHandle stream_or_array,
                                     ParserCallbacks* callbacks)
{
    stream_or_array.parseContentStream_internal(
        "content stream objects", callbacks);
}

void
QPDFObjectHandle::parseContentStream_internal(
    std::string const& description,
    ParserCallbacks* callbacks)
{
    Pl_Buffer buf("concatenated stream data buffer");
    std::string all_description;
    pipeContentStreams(&buf, description, all_description);
    PointerHolder<Buffer> stream_data = buf.getBuffer();
    callbacks->contentSize(stream_data->getSize());
    try
    {
        parseContentStream_data(stream_data, all_description,
                                callbacks, getOwningQPDF());
    }
    catch (TerminateParsing&)
    {
        return;
    }
    callbacks->handleEOF();
}

void
QPDFObjectHandle::parseContentStream_data(
    PointerHolder<Buffer> stream_data,
    std::string const& description,
    ParserCallbacks* callbacks,
    QPDF* context)
{
    size_t stream_length = stream_data->getSize();
    PointerHolder<InputSource> input =
        new BufferInputSource(description, stream_data.getPointer());
    QPDFTokenizer tokenizer;
    tokenizer.allowEOF();
    bool empty = false;
    while (QIntC::to_size(input->tell()) < stream_length)
    {
        // Read a token and seek to the beginning. The offset we get
        // from this process is the beginning of the next
        // non-ignorable (space, comment) token. This way, the offset
        // and don't including ignorable content.
        tokenizer.readToken(input, "content", true);
        qpdf_offset_t offset = input->getLastOffset();
        input->seek(offset, SEEK_SET);
        QPDFObjectHandle obj =
            parseInternal(input, "content", tokenizer,
                          empty, 0, context, true);
        if (! obj.isInitialized())
        {
            // EOF
            break;
        }
        size_t length = QIntC::to_size(input->tell() - offset);

        callbacks->handleObject(obj, QIntC::to_size(offset), length);
        if (obj.isOperator() && (obj.getOperatorValue() == "ID"))
        {
            // Discard next character; it is the space after ID that
            // terminated the token.  Read until end of inline image.
            char ch;
            input->read(&ch, 1);
            tokenizer.expectInlineImage(input);
            QPDFTokenizer::Token t =
                tokenizer.readToken(input, description, true);
            offset = input->getLastOffset();
            length = QIntC::to_size(input->tell() - offset);
            if (t.getType() == QPDFTokenizer::tt_bad)
            {
                QTC::TC("qpdf", "QPDFObjectHandle EOF in inline image");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             "stream data", input->tell(),
                             "EOF found while reading inline image"));
            }
            else
            {
                std::string inline_image = t.getValue();
                QTC::TC("qpdf", "QPDFObjectHandle inline image token");
                callbacks->handleObject(
                    QPDFObjectHandle::newInlineImage(inline_image),
                    QIntC::to_size(offset), length);
            }
        }
    }
}

void
QPDFObjectHandle::addContentTokenFilter(PointerHolder<TokenFilter> filter)
{
    coalesceContentStreams();
    this->getKey("/Contents").addTokenFilter(filter);
}

void
QPDFObjectHandle::addTokenFilter(PointerHolder<TokenFilter> filter)
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(
        obj.getPointer())->addTokenFilter(filter);
}

QPDFObjectHandle
QPDFObjectHandle::parse(PointerHolder<InputSource> input,
                        std::string const& object_description,
                        QPDFTokenizer& tokenizer, bool& empty,
                        StringDecrypter* decrypter, QPDF* context)
{
    return parseInternal(input, object_description, tokenizer, empty,
                         decrypter, context, false);
}

QPDFObjectHandle
QPDFObjectHandle::parseInternal(PointerHolder<InputSource> input,
                                std::string const& object_description,
                                QPDFTokenizer& tokenizer, bool& empty,
                                StringDecrypter* decrypter, QPDF* context,
                                bool content_stream)
{
    // This method must take care not to resolve any objects. Don't
    // check the type of any object without first ensuring that it is
    // a direct object. Otherwise, doing so may have the side effect
    // of reading the object and changing the file pointer. If you do
    // this, it will cause a logic error to be thrown from
    // QPDF::inParse().

    QPDF::ParseGuard pg(context);

    empty = false;

    QPDFObjectHandle object;
    bool set_offset = false;

    std::vector<SparseOHArray> olist_stack;
    olist_stack.push_back(SparseOHArray());
    std::vector<parser_state_e> state_stack;
    state_stack.push_back(st_top);
    std::vector<qpdf_offset_t> offset_stack;
    qpdf_offset_t offset = input->tell();
    offset_stack.push_back(offset);
    bool done = false;
    int bad_count = 0;
    int good_count = 0;
    bool b_contents = false;
    std::vector<std::string> contents_string_stack;
    contents_string_stack.push_back("");
    std::vector<qpdf_offset_t> contents_offset_stack;
    contents_offset_stack.push_back(-1);
    while (! done)
    {
        bool bad = false;
        SparseOHArray& olist = olist_stack.back();
        parser_state_e state = state_stack.back();
        offset = offset_stack.back();
        std::string& contents_string = contents_string_stack.back();
        qpdf_offset_t& contents_offset = contents_offset_stack.back();

	object = QPDFObjectHandle();
	set_offset = false;

	QPDFTokenizer::Token token =
            tokenizer.readToken(input, object_description, true);
        std::string const& token_error_message = token.getErrorMessage();
        if (! token_error_message.empty())
        {
            // Tokens other than tt_bad can still generate warnings.
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         token_error_message));
        }

	switch (token.getType())
	{
          case QPDFTokenizer::tt_eof:
            if (! content_stream)
            {
                QTC::TC("qpdf", "QPDFObjectHandle eof in parseInternal");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "unexpected EOF"));
            }
            bad = true;
            state = st_eof;
            break;

          case QPDFTokenizer::tt_bad:
	    QTC::TC("qpdf", "QPDFObjectHandle bad token in parse");
            bad = true;
            object = newNull();
	    break;

	  case QPDFTokenizer::tt_brace_open:
	  case QPDFTokenizer::tt_brace_close:
	    QTC::TC("qpdf", "QPDFObjectHandle bad brace");
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         "treating unexpected brace token as null"));
            bad = true;
            object = newNull();
	    break;

	  case QPDFTokenizer::tt_array_close:
	    if (state == st_array)
	    {
                state = st_stop;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDFObjectHandle bad array close");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "treating unexpected array close token as null"));
                bad = true;
                object = newNull();
	    }
	    break;

	  case QPDFTokenizer::tt_dict_close:
	    if (state == st_dictionary)
	    {
                state = st_stop;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDFObjectHandle bad dictionary close");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "unexpected dictionary close token"));
                bad = true;
                object = newNull();
	    }
	    break;

	  case QPDFTokenizer::tt_array_open:
	  case QPDFTokenizer::tt_dict_open:
            if (olist_stack.size() > 500)
            {
		QTC::TC("qpdf", "QPDFObjectHandle too deep");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "ignoring excessively deeply nested data structure"));
                bad = true;
                object = newNull();
                state = st_top;
            }
            else
            {
                olist_stack.push_back(SparseOHArray());
                state = st_start;
                offset_stack.push_back(input->tell());
                state_stack.push_back(
                    (token.getType() == QPDFTokenizer::tt_array_open) ?
                    st_array : st_dictionary);
                b_contents = false;
                contents_string_stack.push_back("");
                contents_offset_stack.push_back(-1);
            }
	    break;

	  case QPDFTokenizer::tt_bool:
	    object = newBool((token.getValue() == "true"));
	    break;

	  case QPDFTokenizer::tt_null:
	    object = newNull();
	    break;

	  case QPDFTokenizer::tt_integer:
	    object = newInteger(QUtil::string_to_ll(token.getValue().c_str()));
	    break;

	  case QPDFTokenizer::tt_real:
	    object = newReal(token.getValue());
	    break;

	  case QPDFTokenizer::tt_name:
	    {
		std::string name = token.getValue();
		object = newName(name);

		if (name == "/Contents")
		{
		    b_contents = true;
		}
		else
		{
		    b_contents = false;
		}
	    }
	    break;

	  case QPDFTokenizer::tt_word:
	    {
		std::string const& value = token.getValue();
                if (content_stream)
                {
                    object = QPDFObjectHandle::newOperator(value);
                }
		else if ((value == "R") && (state != st_top) &&
                         (olist.size() >= 2) &&
                         (! olist.at(olist.size() - 1).isIndirect()) &&
                         (olist.at(olist.size() - 1).isInteger()) &&
                         (! olist.at(olist.size() - 2).isIndirect()) &&
                         (olist.at(olist.size() - 2).isInteger()))
		{
                    if (context == 0)
                    {
                        QTC::TC("qpdf", "QPDFObjectHandle indirect without context");
                        throw std::logic_error(
                            "QPDFObjectHandle::parse called without context"
                            " on an object with indirect references");
                    }
		    // Try to resolve indirect objects
		    object = newIndirect(
			context,
			olist.at(olist.size() - 2).getIntValueAsInt(),
			olist.at(olist.size() - 1).getIntValueAsInt());
		    olist.remove_last();
		    olist.remove_last();
		}
		else if ((value == "endobj") && (state == st_top))
		{
		    // We just saw endobj without having read
		    // anything.  Treat this as a null and do not move
		    // the input source's offset.
		    object = newNull();
		    input->seek(input->getLastOffset(), SEEK_SET);
                    empty = true;
		}
		else
		{
                    QTC::TC("qpdf", "QPDFObjectHandle treat word as string");
                    warn(context,
                         QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                 object_description,
                                 input->getLastOffset(),
                                 "unknown token while reading object;"
                                 " treating as string"));
                    bad = true;
                    object = newString(value);
		}
	    }
	    break;

	  case QPDFTokenizer::tt_string:
	    {
		std::string val = token.getValue();
                if (decrypter)
                {
                    if (b_contents)
                    {
                        contents_string = val;
                        contents_offset = input->getLastOffset();
                        b_contents = false;
                    }
                    decrypter->decryptString(val);
                }
		object = QPDFObjectHandle::newString(val);
	    }

	    break;

	  default:
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         "treating unknown token type as null while "
                         "reading object"));
            bad = true;
            object = newNull();
	    break;
	}

        if ((! object.isInitialized()) &&
            (! ((state == st_start) ||
                (state == st_stop) ||
                (state == st_eof))))
        {
            throw std::logic_error(
                "QPDFObjectHandle::parseInternal: "
                "unexpected uninitialized object");
            object = newNull();
        }

        if (bad)
        {
            ++bad_count;
            good_count = 0;
        }
        else
        {
            ++good_count;
            if (good_count > 3)
            {
                bad_count = 0;
            }
        }
        if (bad_count > 5)
        {
            // We had too many consecutive errors without enough
            // intervening successful objects. Give up.
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         "too many errors; giving up on reading object"));
            state = st_top;
            object = newNull();
        }

        switch (state)
        {
          case st_eof:
            if (state_stack.size() > 1)
            {
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "parse error while reading object"));
            }
            done = true;
            // In content stream mode, leave object uninitialized to
            // indicate EOF
            if (! content_stream)
            {
                object = newNull();
            }
            break;

          case st_dictionary:
          case st_array:
            setObjectDescriptionFromInput(
                object, context, object_description, input,
                input->getLastOffset());
            object.setParsedOffset(input->getLastOffset());
            set_offset = true;
            olist.append(object);
            break;

          case st_top:
            done = true;
            break;

          case st_start:
            break;

          case st_stop:
            if ((state_stack.size() < 2) || (olist_stack.size() < 2))
            {
                throw std::logic_error(
                    "QPDFObjectHandle::parseInternal: st_stop encountered"
                    " with insufficient elements in stack");
            }
            parser_state_e old_state = state_stack.back();
            state_stack.pop_back();
            if (old_state == st_array)
            {
                // There's no newArray(SparseOHArray) since
                // SparseOHArray is not part of the public API.
                object = QPDFObjectHandle(new QPDF_Array(olist));
                setObjectDescriptionFromInput(
                    object, context, object_description, input, offset);
                // The `offset` points to the next of "[". Set the
                // rewind offset to point to the beginning of "[".
                // This has been explicitly tested with whitespace
                // surrounding the array start delimiter.
                // getLastOffset points to the array end token and
                // therefore can't be used here.
                object.setParsedOffset(offset - 1);
                set_offset = true;
            }
            else if (old_state == st_dictionary)
            {
                // Convert list to map. Alternating elements are keys.
                // Attempt to recover more or less gracefully from
                // invalid dictionaries.
                std::set<std::string> names;
                size_t n_elements = olist.size();
                for (size_t i = 0; i < n_elements; ++i)
                {
                    QPDFObjectHandle oh = olist.at(i);
                    if ((! oh.isIndirect()) && oh.isName())
                    {
                        names.insert(oh.getName());
                    }
                }

                std::map<std::string, QPDFObjectHandle> dict;
                int next_fake_key = 1;
                for (unsigned int i = 0; i < olist.size(); ++i)
                {
                    QPDFObjectHandle key_obj = olist.at(i);
                    QPDFObjectHandle val;
                    if (key_obj.isIndirect() || (! key_obj.isName()))
                    {
                        bool found_fake = false;
                        std::string candidate;
                        while (! found_fake)
                        {
                            candidate =
                                "/QPDFFake" +
                                QUtil::int_to_string(next_fake_key++);
                            found_fake = (names.count(candidate) == 0);
                            QTC::TC("qpdf", "QPDFObjectHandle found fake",
                                    (found_fake ? 0 : 1));
                        }
                        warn(context,
                             QPDFExc(
                                 qpdf_e_damaged_pdf,
                                 input->getName(), object_description, offset,
                                 "expected dictionary key but found"
                                 " non-name object; inserting key " +
                                 candidate));
                        val = key_obj;
                        key_obj = newName(candidate);
                    }
                    else if (i + 1 >= olist.size())
                    {
                        QTC::TC("qpdf", "QPDFObjectHandle no val for last key");
                        warn(context,
                             QPDFExc(
                                 qpdf_e_damaged_pdf,
                                 input->getName(), object_description, offset,
                                 "dictionary ended prematurely; "
                                 "using null as value for last key"));
                        val = newNull();
                        setObjectDescriptionFromInput(
                            val, context, object_description, input, offset);
                    }
                    else
                    {
                        val = olist.at(++i);
                    }
                    std::string key = key_obj.getName();
                    if (dict.count(key) > 0)
                    {
                        QTC::TC("qpdf", "QPDFObjectHandle duplicate dict key");
                        warn(context,
                             QPDFExc(
                                 qpdf_e_damaged_pdf,
                                 input->getName(), object_description, offset,
                                 "dictionary has duplicated key " + key +
                                 "; last occurrence overrides earlier ones"));
                    }
                    dict[key] = val;
                }
		if (!contents_string.empty() &&
		    dict.count("/Type") &&
		    dict["/Type"].isName() &&
		    dict["/Type"].getName() == "/Sig" &&
		    dict.count("/ByteRange") &&
		    dict.count("/Contents") &&
		    dict["/Contents"].isString())
		{
		    dict["/Contents"]
		      = QPDFObjectHandle::newString(contents_string);
		    dict["/Contents"].setParsedOffset(contents_offset);
		}
                object = newDictionary(dict);
                setObjectDescriptionFromInput(
                    object, context, object_description, input, offset);
                // The `offset` points to the next of "<<". Set the
                // rewind offset to point to the beginning of "<<".
                // This has been explicitly tested with whitespace
                // surrounding the dictionary start delimiter.
                // getLastOffset points to the dictionary end token
                // and therefore can't be used here.
                object.setParsedOffset(offset - 2);
                set_offset = true;
            }
            olist_stack.pop_back();
            offset_stack.pop_back();
            if (state_stack.back() == st_top)
            {
                done = true;
            }
            else
            {
                olist_stack.back().append(object);
            }
            contents_string_stack.pop_back();
            contents_offset_stack.pop_back();
        }
    }

    if (! set_offset)
    {
        setObjectDescriptionFromInput(
            object, context, object_description, input, offset);
        object.setParsedOffset(offset);
    }
    return object;
}

qpdf_offset_t
QPDFObjectHandle::getParsedOffset()
{
    dereference();
    return this->obj->getParsedOffset();
}

void
QPDFObjectHandle::setParsedOffset(qpdf_offset_t offset)
{
    if (this->obj.getPointer())
    {
        this->obj->setParsedOffset(offset);
    }
}

QPDFObjectHandle
QPDFObjectHandle::newIndirect(QPDF* qpdf, int objid, int generation)
{
    if (objid == 0)
    {
        // Special case: QPDF uses objid 0 as a sentinel for direct
        // objects, and the PDF specification doesn't allow for object
        // 0. Treat indirect references to object 0 as null so that we
        // never create an indirect object with objid 0.
        QTC::TC("qpdf", "QPDFObjectHandle indirect with 0 objid");
        return newNull();
    }

    return QPDFObjectHandle(qpdf, objid, generation);
}

QPDFObjectHandle
QPDFObjectHandle::newBool(bool value)
{
    return QPDFObjectHandle(new QPDF_Bool(value));
}

QPDFObjectHandle
QPDFObjectHandle::newNull()
{
    return QPDFObjectHandle(new QPDF_Null());
}

QPDFObjectHandle
QPDFObjectHandle::newInteger(long long value)
{
    return QPDFObjectHandle(new QPDF_Integer(value));
}

QPDFObjectHandle
QPDFObjectHandle::newReal(std::string const& value)
{
    return QPDFObjectHandle(new QPDF_Real(value));
}

QPDFObjectHandle
QPDFObjectHandle::newReal(double value, int decimal_places)
{
    return QPDFObjectHandle(new QPDF_Real(value, decimal_places));
}

QPDFObjectHandle
QPDFObjectHandle::newName(std::string const& name)
{
    return QPDFObjectHandle(new QPDF_Name(name));
}

QPDFObjectHandle
QPDFObjectHandle::newString(std::string const& str)
{
    return QPDFObjectHandle(new QPDF_String(str));
}

QPDFObjectHandle
QPDFObjectHandle::newUnicodeString(std::string const& utf8_str)
{
    return QPDFObjectHandle(QPDF_String::new_utf16(utf8_str));
}

QPDFObjectHandle
QPDFObjectHandle::newOperator(std::string const& value)
{
    return QPDFObjectHandle(new QPDF_Operator(value));
}

QPDFObjectHandle
QPDFObjectHandle::newInlineImage(std::string const& value)
{
    return QPDFObjectHandle(new QPDF_InlineImage(value));
}

QPDFObjectHandle
QPDFObjectHandle::newArray()
{
    return newArray(std::vector<QPDFObjectHandle>());
}

QPDFObjectHandle
QPDFObjectHandle::newArray(std::vector<QPDFObjectHandle> const& items)
{
    return QPDFObjectHandle(new QPDF_Array(items));
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
QPDFObjectHandle::newFromRectangle(Rectangle const& rect)
{
    return newArray(rect);
}

QPDFObjectHandle
QPDFObjectHandle::newFromMatrix(Matrix const& rect)
{
    return newArray(rect);
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
    return QPDFObjectHandle(new QPDF_Dictionary(items));
}


QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, int objid, int generation,
			    QPDFObjectHandle stream_dict,
			    qpdf_offset_t offset, size_t length)
{
    QPDFObjectHandle result = QPDFObjectHandle(new QPDF_Stream(
				qpdf, objid, generation,
				stream_dict, offset, length));
    if (offset)
    {
        result.setParsedOffset(offset);
    }
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf)
{
    if (qpdf == 0)
    {
        throw std::runtime_error(
            "attempt to create stream in null qpdf object");
    }
    QTC::TC("qpdf", "QPDFObjectHandle newStream");
    QPDFObjectHandle stream_dict = newDictionary();
    QPDFObjectHandle result = qpdf->makeIndirectObject(
	QPDFObjectHandle(
	    new QPDF_Stream(qpdf, 0, 0, stream_dict, 0, 0)));
    result.dereference();
    QPDF_Stream* stream =
        dynamic_cast<QPDF_Stream*>(result.obj.getPointer());
    stream->setObjGen(result.getObjectID(), result.getGeneration());
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, PointerHolder<Buffer> data)
{
    QTC::TC("qpdf", "QPDFObjectHandle newStream with data");
    QPDFObjectHandle result = newStream(qpdf);
    result.replaceStreamData(data, newNull(), newNull());
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, std::string const& data)
{
    QTC::TC("qpdf", "QPDFObjectHandle newStream with string");
    QPDFObjectHandle result = newStream(qpdf);
    result.replaceStreamData(data, newNull(), newNull());
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::newReserved(QPDF* qpdf)
{
    // Reserve a spot for this object by assigning it an object
    // number, but then return an unresolved handle to the object.
    QPDFObjectHandle reserved = qpdf->makeIndirectObject(
	QPDFObjectHandle(new QPDF_Reserved()));
    QPDFObjectHandle result =
        newIndirect(qpdf, reserved.objid, reserved.generation);
    result.reserved = true;
    return result;
}

void
QPDFObjectHandle::setObjectDescription(QPDF* owning_qpdf,
                                       std::string const& object_description)
{
    if (isInitialized() && this->obj.getPointer())
    {
        this->obj->setDescription(owning_qpdf, object_description);
    }
}

bool
QPDFObjectHandle::hasObjectDescription()
{
    if (isInitialized() && this->obj.getPointer())
    {
        return this->obj->hasDescription();
    }
    return false;
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
QPDFObjectHandle::shallowCopyInternal(QPDFObjectHandle& new_obj,
                                      bool first_level_only)
{
    assertInitialized();

    if (isStream())
    {
	QTC::TC("qpdf", "QPDFObjectHandle ERR shallow copy stream");
	throw std::runtime_error(
	    "attempt to make a shallow copy of a stream");
    }

    if (isArray())
    {
	QTC::TC("qpdf", "QPDFObjectHandle shallow copy array");
        // No newArray for shallow copying the sparse array
        QPDF_Array* arr = dynamic_cast<QPDF_Array*>(obj.getPointer());
        new_obj = QPDFObjectHandle(
            new QPDF_Array(arr->getElementsForShallowCopy()));
    }
    else if (isDictionary())
    {
	QTC::TC("qpdf", "QPDFObjectHandle shallow copy dictionary");
        new_obj = newDictionary(getDictAsMap());
    }
    else
    {
	QTC::TC("qpdf", "QPDFObjectHandle shallow copy scalar");
        new_obj = *this;
    }

    std::set<QPDFObjGen> visited;
    new_obj.copyObject(visited, false, first_level_only, false);
}

void
QPDFObjectHandle::copyObject(std::set<QPDFObjGen>& visited,
                             bool cross_indirect, bool first_level_only,
                             bool stop_at_streams)
{
    assertInitialized();

    if (isStream())
    {
	QTC::TC("qpdf", "QPDFObjectHandle copy stream",
                stop_at_streams ? 0 : 1);
        if (stop_at_streams)
        {
            return;
        }
	throw std::runtime_error(
	    "attempt to make a stream into a direct object");
    }

    QPDFObjGen cur_og(this->objid, this->generation);
    if (cur_og.getObj() != 0)
    {
	if (visited.count(cur_og))
	{
	    QTC::TC("qpdf", "QPDFObjectHandle makeDirect loop");
	    throw std::runtime_error(
		"loop detected while converting object from "
		"indirect to direct");
	}
	visited.insert(cur_og);
    }

    if (isReserved())
    {
        throw std::logic_error(
            "QPDFObjectHandle: attempting to make a"
            " reserved object handle direct");
    }

    dereference();
    this->qpdf = 0;
    this->objid = 0;
    this->generation = 0;

    PointerHolder<QPDFObject> new_obj;

    if (isBool())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone bool");
	new_obj = new QPDF_Bool(getBoolValue());
    }
    else if (isNull())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone null");
	new_obj = new QPDF_Null();
    }
    else if (isInteger())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone integer");
	new_obj = new QPDF_Integer(getIntValue());
    }
    else if (isReal())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone real");
	new_obj = new QPDF_Real(getRealValue());
    }
    else if (isName())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone name");
	new_obj = new QPDF_Name(getName());
    }
    else if (isString())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone string");
	new_obj = new QPDF_String(getStringValue());
    }
    else if (isArray())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone array");
	std::vector<QPDFObjectHandle> items;
	int n = getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    items.push_back(getArrayItem(i));
            if ((! first_level_only) &&
                (cross_indirect || (! items.back().isIndirect())))
            {
                items.back().copyObject(
                    visited, cross_indirect,
                    first_level_only, stop_at_streams);
            }
	}
	new_obj = new QPDF_Array(items);
    }
    else if (isDictionary())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone dictionary");
	std::set<std::string> keys = getKeys();
	std::map<std::string, QPDFObjectHandle> items;
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    items[*iter] = getKey(*iter);
            if ((! first_level_only) &&
                (cross_indirect || (! items[*iter].isIndirect())))
            {
                items[*iter].copyObject(
                    visited, cross_indirect,
                    first_level_only, stop_at_streams);
            }
	}
	new_obj = new QPDF_Dictionary(items);
    }
    else
    {
	throw std::logic_error("QPDFObjectHandle::makeDirectInternal: "
			       "unknown object type");
    }

    this->obj = new_obj;

    if (cur_og.getObj())
    {
	visited.erase(cur_og);
    }
}

void
QPDFObjectHandle::makeDirect()
{
    makeDirect(false);
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
    if (! this->initialized)
    {
	throw std::logic_error("operation attempted on uninitialized "
			       "QPDFObjectHandle");
    }
}

void
QPDFObjectHandle::typeWarning(char const* expected_type,
                              std::string const& warning)
{
    QPDF* context = 0;
    std::string description;
    dereference();
    if (this->obj->getDescription(context, description))
    {
        warn(context,
             QPDFExc(
                 qpdf_e_damaged_pdf,
                 "", description, 0,
                 std::string("operation for ") + expected_type +
                 " attempted on object of type " +
                 getTypeName() + ": " + warning));
    }
    else
    {
        assertType(expected_type, false);
    }
}

void
QPDFObjectHandle::warnIfPossible(std::string const& warning,
                                 bool throw_if_no_description)
{
    QPDF* context = 0;
    std::string description;
    dereference();
    if (this->obj->getDescription(context, description))
    {
        warn(context,
             QPDFExc(
                 qpdf_e_damaged_pdf,
                 "", description, 0,
                 warning));
    }
    else if (throw_if_no_description)
    {
        throw std::runtime_error(warning);
    }
}

void
QPDFObjectHandle::objectWarning(std::string const& warning)
{
    warnIfPossible(warning, true);
}

void
QPDFObjectHandle::assertType(char const* type_name, bool istype)
{
    if (! istype)
    {
	throw std::runtime_error(std::string("operation for ") + type_name +
                                 " attempted on object of type " +
                                 getTypeName());
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
    if (! isIndirect())
    {
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
    if (! this->isDictionary())
    {
        return false;
    }
    if (this->hasKey("/Type"))
    {
        QPDFObjectHandle type = this->getKey("/Type");
        if (type.isName() && (type.getName() == "/Page"))
        {
            return true;
        }
        // Files have been seen in the wild that have /Type (Page)
        else if (type.isString() && (type.getStringValue() == "Page"))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    return false;
}

bool
QPDFObjectHandle::isPagesObject()
{
    // Some PDF files have /Type broken on pages.
    return (this->isDictionary() && this->hasKey("/Kids"));
}

bool
QPDFObjectHandle::isFormXObject()
{
    if (! this->isStream())
    {
        return false;
    }
    QPDFObjectHandle dict = this->getDict();
    return (dict.getKey("/Type").isName() &&
            ("/XObject" == dict.getKey("/Type").getName()) &&
            dict.getKey("/Subtype").isName() &&
            ("/Form" == dict.getKey("/Subtype").getName()));
}

bool
QPDFObjectHandle::isImage(bool exclude_imagemask)
{
    if (! this->isStream())
    {
        return false;
    }
    QPDFObjectHandle dict = this->getDict();
    return (dict.hasKey("/Subtype") &&
            (dict.getKey("/Subtype").getName() == "/Image") &&
            ((! exclude_imagemask) ||
             (! (dict.getKey("/ImageMask").isBool() &&
                 dict.getKey("/ImageMask").getBoolValue()))));
}

void
QPDFObjectHandle::assertPageObject()
{
    if (! isPageObject())
    {
	throw std::runtime_error("page operation called on non-Page object");
    }
}

void
QPDFObjectHandle::dereference()
{
    if (this->obj.getPointer() == 0)
    {
        PointerHolder<QPDFObject> obj = QPDF::Resolver::resolve(
	    this->qpdf, this->objid, this->generation);
	if (obj.getPointer() == 0)
	{
            // QPDF::resolve never returns an uninitialized object, but
            // check just in case.
	    this->obj = new QPDF_Null();
	}
        else if (dynamic_cast<QPDF_Reserved*>(obj.getPointer()))
        {
            // Do not resolve
        }
        else
        {
            this->reserved = false;
            this->obj = obj;
        }
    }
}

void
QPDFObjectHandle::warn(QPDF* qpdf, QPDFExc const& e)
{
    // If parsing on behalf of a QPDF object and want to give a
    // warning, we can warn through the object. If parsing for some
    // other reason, such as an explicit creation of an object from a
    // string, then just throw the exception.
    if (qpdf)
    {
        QPDF::Warner::warn(qpdf, e);
    }
    else
    {
        throw e;
    }
}
