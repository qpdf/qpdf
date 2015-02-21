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
#include <qpdf/BufferInputSource.hh>
#include <qpdf/QPDFExc.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <stdexcept>
#include <stdlib.h>
#include <ctype.h>

class TerminateParsing
{
};

void
QPDFObjectHandle::ParserCallbacks::terminateParsing()
{
    throw TerminateParsing();
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
        return obj->getTypeCode();
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
        return obj->getTypeName();
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
	result = static_cast<double>(getIntValue());
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
    assertBool();
    return dynamic_cast<QPDF_Bool*>(obj.getPointer())->getVal();
}

// Integer accessors

long long
QPDFObjectHandle::getIntValue()
{
    assertInteger();
    return dynamic_cast<QPDF_Integer*>(obj.getPointer())->getVal();
}

// Real accessors

std::string
QPDFObjectHandle::getRealValue()
{
    assertReal();
    return dynamic_cast<QPDF_Real*>(obj.getPointer())->getVal();
}

// Name accessors

std::string
QPDFObjectHandle::getName()
{
    assertName();
    return dynamic_cast<QPDF_Name*>(obj.getPointer())->getName();
}

// String accessors

std::string
QPDFObjectHandle::getStringValue()
{
    assertString();
    return dynamic_cast<QPDF_String*>(obj.getPointer())->getVal();
}

std::string
QPDFObjectHandle::getUTF8Value()
{
    assertString();
    return dynamic_cast<QPDF_String*>(obj.getPointer())->getUTF8Val();
}

// Operator and Inline Image accessors

std::string
QPDFObjectHandle::getOperatorValue()
{
    assertOperator();
    return dynamic_cast<QPDF_Operator*>(obj.getPointer())->getVal();
}

std::string
QPDFObjectHandle::getInlineImageValue()
{
    assertInlineImage();
    return dynamic_cast<QPDF_InlineImage*>(obj.getPointer())->getVal();
}

// Array accessors

int
QPDFObjectHandle::getArrayNItems()
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->getNItems();
}

QPDFObjectHandle
QPDFObjectHandle::getArrayItem(int n)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->getItem(n);
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getArrayAsVector()
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->getAsVector();
}

// Array mutators

void
QPDFObjectHandle::setArrayItem(int n, QPDFObjectHandle const& item)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->setItem(n, item);
}

void
QPDFObjectHandle::setArrayFromVector(std::vector<QPDFObjectHandle> const& items)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->setFromVector(items);
}

void
QPDFObjectHandle::insertItem(int at, QPDFObjectHandle const& item)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->insertItem(at, item);
}

void
QPDFObjectHandle::appendItem(QPDFObjectHandle const& item)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->appendItem(item);
}

void
QPDFObjectHandle::eraseItem(int at)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->eraseItem(at);
}

// Dictionary accessors

bool
QPDFObjectHandle::hasKey(std::string const& key)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->hasKey(key);
}

QPDFObjectHandle
QPDFObjectHandle::getKey(std::string const& key)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->getKey(key);
}

std::set<std::string>
QPDFObjectHandle::getKeys()
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->getKeys();
}

std::map<std::string, QPDFObjectHandle>
QPDFObjectHandle::getDictAsMap()
{
    assertDictionary();
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
			    QPDFObjectHandle const& value)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(
	obj.getPointer())->replaceKey(key, value);
}

void
QPDFObjectHandle::removeKey(std::string const& key)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->removeKey(key);
}

void
QPDFObjectHandle::replaceOrRemoveKey(std::string const& key,
				     QPDFObjectHandle value)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(
	obj.getPointer())->replaceOrRemoveKey(key, value);
}

// Stream accessors
QPDFObjectHandle
QPDFObjectHandle::getDict()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getDict();
}

void
QPDFObjectHandle::replaceDict(QPDFObjectHandle new_dict)
{
    assertStream();
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceDict(new_dict);
}

PointerHolder<Buffer>
QPDFObjectHandle::getStreamData()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getStreamData();
}

PointerHolder<Buffer>
QPDFObjectHandle::getRawStreamData()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getRawStreamData();
}

bool
QPDFObjectHandle::pipeStreamData(Pipeline* p, bool filter,
				 bool normalize, bool compress)
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->pipeStreamData(
	p, filter, normalize, compress);
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
    // function.  Also remove call to pushInheritedAttributesToPage
    // from qpdf.cc when show_page_images is true.

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
    else if (! contents.isNull())
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
    size_t offset = input->tell();
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
QPDFObjectHandle::parseContentStream(QPDFObjectHandle stream_or_array,
                                     ParserCallbacks* callbacks)
{
    std::vector<QPDFObjectHandle> streams;
    if (stream_or_array.isArray())
    {
        streams = stream_or_array.getArrayAsVector();
    }
    else
    {
        streams.push_back(stream_or_array);
    }
    for (std::vector<QPDFObjectHandle>::iterator iter = streams.begin();
         iter != streams.end(); ++iter)
    {
        QPDFObjectHandle stream = *iter;
        if (! stream.isStream())
        {
            throw std::logic_error(
                "QPDFObjectHandle: parseContentStream called on non-stream");
        }
        try
        {
            parseContentStream_internal(stream, callbacks);
        }
        catch (TerminateParsing&)
        {
            return;
        }
    }
    callbacks->handleEOF();
}

void
QPDFObjectHandle::parseContentStream_internal(QPDFObjectHandle stream,
                                              ParserCallbacks* callbacks)
{
    stream.assertStream();
    PointerHolder<Buffer> stream_data = stream.getStreamData();
    size_t length = stream_data->getSize();
    std::string description = "content stream object " +
        QUtil::int_to_string(stream.getObjectID()) + " " +
        QUtil::int_to_string(stream.getGeneration());
    PointerHolder<InputSource> input =
        new BufferInputSource(description, stream_data.getPointer());
    QPDFTokenizer tokenizer;
    tokenizer.allowEOF();
    bool empty = false;
    while (static_cast<size_t>(input->tell()) < length)
    {
        QPDFObjectHandle obj =
            parseInternal(input, "content", tokenizer, empty,
                          0, 0, false, false, true);
        if (! obj.isInitialized())
        {
            // EOF
            break;
        }

        callbacks->handleObject(obj);
        if (obj.isOperator() && (obj.getOperatorValue() == "ID"))
        {
            // Discard next character; it is the space after ID that
            // terminated the token.  Read until end of inline image.
            char ch;
            input->read(&ch, 1);
            char buf[4];
            memset(buf, '\0', sizeof(buf));
            bool done = false;
            std::string inline_image;
            while (! done)
            {
                if (input->read(&ch, 1) == 0)
                {
                    QTC::TC("qpdf", "QPDFObjectHandle EOF in inline image");
                    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                  "stream data", input->tell(),
                                  "EOF found while reading inline image");
                }
                inline_image += ch;
                memmove(buf, buf + 1, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = ch;
                if (strchr(" \t\n\v\f\r", buf[0]) &&
                    (buf[1] == 'E') &&
                    (buf[2] == 'I') &&
                    strchr(" \t\n\v\f\r", buf[3]))
                {
                    // We've found an EI operator.
                    done = true;
                    input->seek(-3, SEEK_CUR);
                    for (int i = 0; i < 4; ++i)
                    {
                        if (inline_image.length() > 0)
                        {
                            inline_image.erase(inline_image.length() - 1);
                        }
                    }
                }
            }
            QTC::TC("qpdf", "QPDFObjectHandle inline image token");
            callbacks->handleObject(
                QPDFObjectHandle::newInlineImage(inline_image));
        }
    }
}

QPDFObjectHandle
QPDFObjectHandle::parse(PointerHolder<InputSource> input,
                        std::string const& object_description,
                        QPDFTokenizer& tokenizer, bool& empty,
                        StringDecrypter* decrypter, QPDF* context)
{
    return parseInternal(input, object_description, tokenizer, empty,
                         decrypter, context, false, false, false);
}

QPDFObjectHandle
QPDFObjectHandle::parseInternal(PointerHolder<InputSource> input,
                                std::string const& object_description,
                                QPDFTokenizer& tokenizer, bool& empty,
                                StringDecrypter* decrypter, QPDF* context,
                                bool in_array, bool in_dictionary,
                                bool content_stream)
{
    empty = false;
    if (in_dictionary && in_array)
    {
	// Although dictionaries and arrays arbitrarily nest, these
	// variables indicate what is at the top of the stack right
	// now, so they can, by definition, never both be true.
	throw std::logic_error(
	    "INTERNAL ERROR: parseInternal: in_dict && in_array");
    }

    QPDFObjectHandle object;

    qpdf_offset_t offset = input->tell();
    std::vector<QPDFObjectHandle> olist;
    bool done = false;
    while (! done)
    {
	object = QPDFObjectHandle();

	QPDFTokenizer::Token token =
            tokenizer.readToken(input, object_description);

	switch (token.getType())
	{
          case QPDFTokenizer::tt_eof:
            if (content_stream)
            {
                // Return uninitialized object to indicate EOF
                return object;
            }
            else
            {
                // When not in content stream mode, EOF is tt_bad and
                // throws an exception before we get here.
                throw std::logic_error(
                    "EOF received while not in content stream mode");
            }
            break;

	  case QPDFTokenizer::tt_brace_open:
	  case QPDFTokenizer::tt_brace_close:
	    // Don't know what to do with these for now
	    QTC::TC("qpdf", "QPDFObjectHandle bad brace");
	    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
			  object_description,
			  input->getLastOffset(),
			  "unexpected brace token");
	    break;

	  case QPDFTokenizer::tt_array_close:
	    if (in_array)
	    {
		done = true;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDFObjectHandle bad array close");
		throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
			      object_description,
			      input->getLastOffset(),
			      "unexpected array close token");
	    }
	    break;

	  case QPDFTokenizer::tt_dict_close:
	    if (in_dictionary)
	    {
		done = true;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDFObjectHandle bad dictionary close");
		throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
			      object_description,
			      input->getLastOffset(),
			      "unexpected dictionary close token");
	    }
	    break;

	  case QPDFTokenizer::tt_array_open:
	    object = parseInternal(
		input, object_description, tokenizer, empty,
                decrypter, context, true, false, content_stream);
	    break;

	  case QPDFTokenizer::tt_dict_open:
	    object = parseInternal(
		input, object_description, tokenizer, empty,
                decrypter, context, false, true, content_stream);
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
	    object = newName(token.getValue());
	    break;

	  case QPDFTokenizer::tt_word:
	    {
		std::string const& value = token.getValue();
		if ((value == "R") && (in_array || in_dictionary) &&
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
			olist.at(olist.size() - 2).getIntValue(),
			olist.at(olist.size() - 1).getIntValue());
		    olist.pop_back();
		    olist.pop_back();
		}
		else if ((value == "endobj") &&
			 (! (in_array || in_dictionary)))
		{
		    // We just saw endobj without having read
		    // anything.  Treat this as a null and do not move
		    // the input source's offset.
		    object = newNull();
		    input->seek(input->getLastOffset(), SEEK_SET);
                    empty = true;
		}
		else if (content_stream)
                {
                    object = QPDFObjectHandle::newOperator(token.getValue());
                }
		else
		{
		    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
				  object_description,
				  input->getLastOffset(),
				  "unknown token while reading object (" +
				  value + ")");
		}
	    }
	    break;

	  case QPDFTokenizer::tt_string:
	    {
		std::string val = token.getValue();
                if (decrypter)
                {
                    decrypter->decryptString(val);
                }
		object = QPDFObjectHandle::newString(val);
	    }

	    break;

	  default:
	    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
			  object_description,
			  input->getLastOffset(),
			  "unknown token type while reading object");
	    break;
	}

	if (in_dictionary || in_array)
	{
	    if (! done)
	    {
		olist.push_back(object);
	    }
	}
	else if (! object.isInitialized())
	{
	    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
			  object_description,
			  input->getLastOffset(),
			  "parse error while reading object");
	}
	else
	{
	    done = true;
	}
    }

    if (in_array)
    {
	object = newArray(olist);
    }
    else if (in_dictionary)
    {
	// Convert list to map.  Alternating elements are keys.
	std::map<std::string, QPDFObjectHandle> dict;
	if (olist.size() % 2)
	{
	    QTC::TC("qpdf", "QPDFObjectHandle dictionary odd number of elements");
	    throw QPDFExc(
		qpdf_e_damaged_pdf, input->getName(),
		object_description, input->getLastOffset(),
		"dictionary ending here has an odd number of elements");
	}
	for (unsigned int i = 0; i < olist.size(); i += 2)
	{
	    QPDFObjectHandle key_obj = olist.at(i);
	    QPDFObjectHandle val = olist.at(i + 1);
	    if (! key_obj.isName())
	    {
		throw QPDFExc(
		    qpdf_e_damaged_pdf,
		    input->getName(), object_description, offset,
		    std::string("dictionary key not name (") +
		    key_obj.unparse() + ")");
	    }
	    dict[key_obj.getName()] = val;
	}
	object = newDictionary(dict);
    }

    return object;
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
    return QPDFObjectHandle(new QPDF_Stream(
				qpdf, objid, generation,
				stream_dict, offset, length));
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf)
{
    QTC::TC("qpdf", "QPDFObjectHandle newStream");
    QPDFObjectHandle stream_dict = newDictionary();
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
QPDFObjectHandle::assertType(char const* type_name, bool istype) const
{
    if (! istype)
    {
	throw std::logic_error(std::string("operation for ") + type_name +
			       " object attempted on object of wrong type");
    }
}

void
QPDFObjectHandle::assertNull()
{
    assertType("Null", isNull());
}

void
QPDFObjectHandle::assertBool()
{
    assertType("Boolean", isBool());
}

void
QPDFObjectHandle::assertInteger()
{
    assertType("Integer", isInteger());
}

void
QPDFObjectHandle::assertReal()
{
    assertType("Real", isReal());
}

void
QPDFObjectHandle::assertName()
{
    assertType("Name", isName());
}

void
QPDFObjectHandle::assertString()
{
    assertType("String", isString());
}

void
QPDFObjectHandle::assertOperator()
{
    assertType("Operator", isOperator());
}

void
QPDFObjectHandle::assertInlineImage()
{
    assertType("InlineImage", isInlineImage());
}

void
QPDFObjectHandle::assertArray()
{
    assertType("Array", isArray());
}

void
QPDFObjectHandle::assertDictionary()
{
    assertType("Dictionary", isDictionary());
}

void
QPDFObjectHandle::assertStream()
{
    assertType("Stream", isStream());
}

void
QPDFObjectHandle::assertReserved()
{
    assertType("Reserved", isReserved());
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
    assertType("Scalar", isScalar());
}

void
QPDFObjectHandle::assertNumber()
{
    assertType("Number", isNumber());
}

bool
QPDFObjectHandle::isPageObject()
{
    return (this->isDictionary() && this->hasKey("/Type") &&
            (this->getKey("/Type").getName() == "/Page"));
}

bool
QPDFObjectHandle::isPagesObject()
{
    return (this->isDictionary() && this->hasKey("/Type") &&
            (this->getKey("/Type").getName() == "/Pages"));
}

void
QPDFObjectHandle::assertPageObject()
{
    if (! isPageObject())
    {
	throw std::logic_error("page operation called on non-Page object");
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
	    QTC::TC("qpdf", "QPDFObjectHandle indirect to unknown");
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
