#include <qpdf/QPDFObjectHandle.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDF_Bool.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Integer.hh>
#include <qpdf/QPDF_Real.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QPDF_String.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_Stream.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <stdexcept>
#include <stdlib.h>

QPDFObjectHandle::QPDFObjectHandle() :
    initialized(false),
    objid(0),
    generation(0)
{
}

QPDFObjectHandle::QPDFObjectHandle(QPDF* qpdf, int objid, int generation) :
    initialized(true),
    qpdf(qpdf),
    objid(objid),
    generation(generation)
{
}

QPDFObjectHandle::QPDFObjectHandle(QPDFObject* data) :
    initialized(true),
    qpdf(0),
    objid(0),
    generation(0),
    obj(data)
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

bool
QPDFObjectHandle::isInitialized() const
{
    return this->initialized;
}

template <class T>
class QPDFObjectTypeAccessor
{
  public:
    static bool check(QPDFObject* o)
    {
	return (o && dynamic_cast<T*>(o));
    }
};

bool
QPDFObjectHandle::isBool()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Bool>::check(obj.getPointer());
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
	result = getIntValue();
    }
    else if (isReal())
    {
	result = atof(getRealValue().c_str());
    }
    else
    {
	throw std::logic_error("getNumericValue called for non-numeric object");
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
QPDFObjectHandle::isIndirect()
{
    assertInitialized();
    return (this->objid != 0);
}

bool
QPDFObjectHandle::isScalar()
{
    return (! (isArray() || isDictionary() || isStream()));
}

// Bool accessors

bool
QPDFObjectHandle::getBoolValue()
{
    assertType("Boolean", isBool());
    return dynamic_cast<QPDF_Bool*>(obj.getPointer())->getVal();
}

// Integer accessors

int
QPDFObjectHandle::getIntValue()
{
    assertType("Integer", isInteger());
    return dynamic_cast<QPDF_Integer*>(obj.getPointer())->getVal();
}

// Real accessors

std::string
QPDFObjectHandle::getRealValue()
{
    assertType("Real", isReal());
    return dynamic_cast<QPDF_Real*>(obj.getPointer())->getVal();
}

// Name accessors

std::string
QPDFObjectHandle::getName()
{
    assertType("Name", isName());
    return dynamic_cast<QPDF_Name*>(obj.getPointer())->getName();
}

// String accessors

std::string
QPDFObjectHandle::getStringValue()
{
    assertType("String", isString());
    return dynamic_cast<QPDF_String*>(obj.getPointer())->getVal();
}

std::string
QPDFObjectHandle::getUTF8Value()
{
    assertType("String", isString());
    return dynamic_cast<QPDF_String*>(obj.getPointer())->getUTF8Val();
}

// Array accessors

int
QPDFObjectHandle::getArrayNItems()
{
    assertType("Array", isArray());
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->getNItems();
}

QPDFObjectHandle
QPDFObjectHandle::getArrayItem(int n)
{
    assertType("Array", isArray());
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->getItem(n);
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getArrayAsVector()
{
    assertType("Array", isArray());
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->getAsVector();
}

// Array mutators

void
QPDFObjectHandle::setArrayItem(int n, QPDFObjectHandle const& item)
{
    assertType("Array", isArray());
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->setItem(n, item);
}

void
QPDFObjectHandle::setArrayFromVector(std::vector<QPDFObjectHandle> const& items)
{
    assertType("Array", isArray());
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->setFromVector(items);
}

void
QPDFObjectHandle::insertItem(int at, QPDFObjectHandle const& item)
{
    assertType("Array", isArray());
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->insertItem(at, item);
}

void
QPDFObjectHandle::appendItem(QPDFObjectHandle const& item)
{
    assertType("Array", isArray());
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->appendItem(item);
}

void
QPDFObjectHandle::eraseItem(int at)
{
    assertType("Array", isArray());
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->eraseItem(at);
}

// Dictionary accessors

bool
QPDFObjectHandle::hasKey(std::string const& key)
{
    assertType("Dictionary", isDictionary());
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->hasKey(key);
}

QPDFObjectHandle
QPDFObjectHandle::getKey(std::string const& key)
{
    assertType("Dictionary", isDictionary());
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->getKey(key);
}

std::set<std::string>
QPDFObjectHandle::getKeys()
{
    assertType("Dictionary", isDictionary());
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->getKeys();
}

std::map<std::string, QPDFObjectHandle>
QPDFObjectHandle::getDictAsMap()
{
    assertType("Dictionary", isDictionary());
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->getAsMap();
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

// Dictionary mutators

void
QPDFObjectHandle::replaceKey(std::string const& key,
			    QPDFObjectHandle const& value)
{
    assertType("Dictionary", isDictionary());
    return dynamic_cast<QPDF_Dictionary*>(
	obj.getPointer())->replaceKey(key, value);
}

void
QPDFObjectHandle::removeKey(std::string const& key)
{
    assertType("Dictionary", isDictionary());
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->removeKey(key);
}

void
QPDFObjectHandle::replaceOrRemoveKey(std::string const& key,
				     QPDFObjectHandle value)
{
    assertType("Dictionary", isDictionary());
    return dynamic_cast<QPDF_Dictionary*>(
	obj.getPointer())->replaceOrRemoveKey(key, value);
}

// Stream accessors
QPDFObjectHandle
QPDFObjectHandle::getDict()
{
    assertType("Stream", isStream());
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getDict();
}

PointerHolder<Buffer>
QPDFObjectHandle::getStreamData()
{
    assertType("Stream", isStream());
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getStreamData();
}

PointerHolder<Buffer>
QPDFObjectHandle::getRawStreamData()
{
    assertType("Stream", isStream());
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getRawStreamData();
}

bool
QPDFObjectHandle::pipeStreamData(Pipeline* p, bool filter,
				 bool normalize, bool compress)
{
    assertType("Stream", isStream());
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->pipeStreamData(
	p, filter, normalize, compress);
}

void
QPDFObjectHandle::replaceStreamData(PointerHolder<Buffer> data,
				    QPDFObjectHandle const& filter,
				    QPDFObjectHandle const& decode_parms)
{
    assertType("Stream", isStream());
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceStreamData(
	data, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(PointerHolder<StreamDataProvider> provider,
				    QPDFObjectHandle const& filter,
				    QPDFObjectHandle const& decode_parms,
				    size_t length)
{
    assertType("Stream", isStream());
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceStreamData(
	provider, filter, decode_parms, length);
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
    assertPageObject();

    // Note: this code doesn't handle inherited resources.  If this
    // page dictionary doesn't have a /Resources key or has one whose
    // value is null or an empty dictionary, you are supposed to walk
    // up the page tree until you find a /Resources dictionary.  As of
    // this writing, I don't have any test files that use inherited
    // resources, and hand-generating one won't be a good test because
    // any mistakes in my understanding would be present in both the
    // code and the test file.

    // NOTE: If support of inherited resources (see above comment) is
    // implemented, edit comment in QPDFObjectHandle.hh for this
    // function.

    std::map<std::string, QPDFObjectHandle> result;
    if (this->hasKey("/Resources"))
    {
	QPDFObjectHandle resources = this->getKey("/Resources");
	if (resources.hasKey("/XObject"))
	{
	    QPDFObjectHandle xobject = resources.getKey("/XObject");
	    std::set<std::string> keys = xobject.getKeys();
	    for (std::set<std::string>::iterator iter = keys.begin();
		 iter != keys.end(); ++iter)
	    {
		std::string key = (*iter);
		QPDFObjectHandle value = xobject.getKey(key);
		if (value.isStream())
		{
		    QPDFObjectHandle dict = value.getDict();
		    if (dict.hasKey("/Subtype") &&
			(dict.getKey("/Subtype").getName() == "/Image") &&
			(! dict.hasKey("/ImageMask")))
		    {
			result[key] = value;
		    }
		}
	    }
	}
    }

    return result;
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getPageContents()
{
    assertPageObject();

    std::vector<QPDFObjectHandle> result;
    QPDFObjectHandle contents = this->getKey("/Contents");
    if (contents.isArray())
    {
	int n_items = contents.getArrayNItems();
	for (int i = 0; i < n_items; ++i)
	{
	    QPDFObjectHandle item = contents.getArrayItem(i);
	    if (item.isStream())
	    {
		result.push_back(item);
	    }
	    else
	    {
		throw std::runtime_error(
		    "unknown item type while inspecting "
		    "element of /Contents array in page "
		    "dictionary");
	    }
	}
    }
    else if (contents.isStream())
    {
	result.push_back(contents);
    }
    else
    {
	throw std::runtime_error("unknown object type inspecting /Contents "
				 "key in page dictionary");
    }

    return result;
}

void
QPDFObjectHandle::addPageContents(QPDFObjectHandle new_contents, bool first)
{
    assertPageObject();
    new_contents.assertType("Stream", new_contents.isStream());

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
    dereference();
    return this->obj->unparse();
}

QPDFObjectHandle
QPDFObjectHandle::newIndirect(QPDF* qpdf, int objid, int generation)
{
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
QPDFObjectHandle::newInteger(int value)
{
    return QPDFObjectHandle(new QPDF_Integer(value));
}

QPDFObjectHandle
QPDFObjectHandle::newReal(std::string const& value)
{
    return QPDFObjectHandle(new QPDF_Real(value));
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
QPDFObjectHandle::newArray(std::vector<QPDFObjectHandle> const& items)
{
    return QPDFObjectHandle(new QPDF_Array(items));
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
    return QPDFObjectHandle(new QPDF_Stream(
				qpdf, objid, generation,
				stream_dict, offset, length));
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf)
{
    QTC::TC("qpdf", "QPDFObjectHandle newStream");
    std::map<std::string, QPDFObjectHandle> keys;
    QPDFObjectHandle stream_dict = newDictionary(keys);
    QPDFObjectHandle result = qpdf->makeIndirectObject(
	QPDFObjectHandle(
	    new QPDF_Stream(qpdf, 0, 0, stream_dict, 0, 0)));
    result.dereference();
    QPDF_Stream* stream = dynamic_cast<QPDF_Stream*>(result.obj.getPointer());
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
QPDFObjectHandle::shallowCopy()
{
    assertInitialized();

    if (isStream())
    {
	QTC::TC("qpdf", "QPDFObjectHandle ERR shallow copy stream");
	throw std::runtime_error(
	    "attempt to make a shallow copy of a stream");
    }

    QPDFObjectHandle new_obj;
    if (isArray())
    {
	QTC::TC("qpdf", "QPDFObjectHandle shallow copy array");
	new_obj = newArray(getArrayAsVector());
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

    return new_obj;
}

void
QPDFObjectHandle::makeDirectInternal(std::set<int>& visited)
{
    assertInitialized();

    if (isStream())
    {
	QTC::TC("qpdf", "QPDFObjectHandle ERR clone stream");
	throw std::runtime_error(
	    "attempt to make a stream into a direct object");
    }

    int cur_objid = this->objid;
    if (cur_objid != 0)
    {
	if (visited.count(cur_objid))
	{
	    QTC::TC("qpdf", "QPDFObjectHandle makeDirect loop");
	    throw std::runtime_error(
		"loop detected while converting object from "
		"indirect to direct");
	}
	visited.insert(cur_objid);
    }

    dereference();
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
	    items.back().makeDirectInternal(visited);
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
	    items[*iter].makeDirectInternal(visited);
	}
	new_obj = new QPDF_Dictionary(items);
    }
    else
    {
	throw std::logic_error("QPDFObjectHandle::makeDirectInternal: "
			       "unknown object type");
    }

    this->obj = new_obj;

    if (cur_objid)
    {
	visited.erase(cur_objid);
    }
}

void
QPDFObjectHandle::makeDirect()
{
    std::set<int> visited;
    makeDirectInternal(visited);
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
QPDFObjectHandle::assertType(char const* type_name, bool istype)
{
    if (! istype)
    {
	throw std::logic_error(std::string("operation for ") + type_name +
			       " object attempted on object of wrong type");
    }
}

void
QPDFObjectHandle::assertPageObject()
{
    if (! (this->isDictionary() && this->hasKey("/Type") &&
	   (this->getKey("/Type").getName() == "/Page")))
    {
	throw std::logic_error("page operation called on non-Page object");
    }
}

void
QPDFObjectHandle::dereference()
{
    if (this->obj.getPointer() == 0)
    {
	this->obj = QPDF::Resolver::resolve(
	    this->qpdf, this->objid, this->generation);
	if (this->obj.getPointer() == 0)
	{
	    QTC::TC("qpdf", "QPDFObjectHandle indirect to unknown");
	    this->obj = new QPDF_Null();
	}
    }
}
