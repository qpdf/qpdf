// Copyright (c) 2005-2018 Jay Berkenbilt
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Versions of qpdf prior to version 7 were released under the terms
// of version 2.0 of the Artistic License. At your option, you may
// continue to consider qpdf to be licensed under those terms. Please
// see the manual for additional information.

#ifndef __QPDFOBJECTHANDLE_HH__
#define __QPDFOBJECTHANDLE_HH__

#include <qpdf/DLL.h>
#include <qpdf/Types.h>
#include <qpdf/Constants.h>

#include <string>
#include <vector>
#include <set>
#include <map>

#include <qpdf/QPDFObjGen.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Buffer.hh>
#include <qpdf/InputSource.hh>

#include <qpdf/QPDFObject.hh>

class Pipeline;
class QPDF;
class QPDF_Dictionary;
class QPDF_Array;
class QPDFTokenizer;
class QPDFExc;

class QPDFObjectHandle
{
  public:
    // This class is used by replaceStreamData.  It provides an
    // alternative way of associating stream data with a stream.  See
    // comments on replaceStreamData and newStream for additional
    // details.
    class StreamDataProvider
    {
      public:
	QPDF_DLL
	virtual ~StreamDataProvider()
	{
	}
	// The implementation of this function must write stream data
	// to the given pipeline. The stream data must conform to
	// whatever filters are explicitly associated with the stream.
	// QPDFWriter may, in some cases, add compression, but if it
	// does, it will update the filters as needed. Every call to
	// provideStreamData for a given stream must write the same
	// data.The object ID and generation passed to this method are
	// those that belong to the stream on behalf of which the
	// provider is called. They may be ignored or used by the
	// implementation for indexing or other purposes. This
	// information is made available just to make it more
	// convenient to use a single StreamDataProvider object to
	// provide data for multiple streams.
	virtual void provideStreamData(int objid, int generation,
				       Pipeline* pipeline) = 0;
    };

    // This class is used by parse to decrypt strings when reading an
    // object that contains encrypted strings.
    class StringDecrypter
    {
      public:
        QPDF_DLL
        virtual ~StringDecrypter()
        {
        }
        virtual void decryptString(std::string& val) = 0;
    };

    // This class is used by parseContentStream.  Callers must
    // instantiate a subclass of this with handlers defined to accept
    // QPDFObjectHandles that are parsed from the stream.
    class ParserCallbacks
    {
      public:
        QPDF_DLL
        virtual ~ParserCallbacks()
        {
        }
        virtual void handleObject(QPDFObjectHandle) = 0;
        virtual void handleEOF() = 0;

      protected:
        // Implementors may call this method during parsing to
        // terminate parsing early.  This method throws an exception
        // that is caught by parseContentStream, so its effect is
        // immediate.
        QPDF_DLL
        void terminateParsing();
    };


    QPDF_DLL
    QPDFObjectHandle();
    QPDF_DLL
    bool isInitialized() const;

    // Return type code and type name of underlying object.  These are
    // useful for doing rapid type tests (like switch statements) or
    // for testing and debugging.
    QPDF_DLL
    QPDFObject::object_type_e getTypeCode();
    QPDF_DLL
    char const* getTypeName();

    // Exactly one of these will return true for any object.  Operator
    // and InlineImage are only allowed in content streams.
    QPDF_DLL
    bool isBool();
    QPDF_DLL
    bool isNull();
    QPDF_DLL
    bool isInteger();
    QPDF_DLL
    bool isReal();
    QPDF_DLL
    bool isName();
    QPDF_DLL
    bool isString();
    QPDF_DLL
    bool isOperator();
    QPDF_DLL
    bool isInlineImage();
    QPDF_DLL
    bool isArray();
    QPDF_DLL
    bool isDictionary();
    QPDF_DLL
    bool isStream();
    QPDF_DLL
    bool isReserved();

    // This returns true in addition to the query for the specific
    // type for indirect objects.
    QPDF_DLL
    bool isIndirect();

    // True for everything except array, dictionary, stream, word, and
    // inline image.
    QPDF_DLL
    bool isScalar();

    // Public factory methods

    // Construct an object of any type from a string representation of
    // the object.  Throws QPDFExc with an empty filename and an
    // offset into the string if there is an error.  Any indirect
    // object syntax (obj gen R) will cause a logic_error exception to
    // be thrown.  If object_description is provided, it will appear
    // in the message of any QPDFExc exception thrown for invalid
    // syntax.
    QPDF_DLL
    static QPDFObjectHandle parse(std::string const& object_str,
                                  std::string const& object_description = "");

    // Construct an object as above by reading from the given
    // InputSource at its current position and using the tokenizer you
    // supply.  Indirect objects and encrypted strings are permitted.
    // This method is intended to be called by QPDF for parsing
    // objects that are ready from the object's input stream.
    QPDF_DLL
    static QPDFObjectHandle parse(PointerHolder<InputSource> input,
                                  std::string const& object_description,
                                  QPDFTokenizer&, bool& empty,
                                  StringDecrypter* decrypter,
                                  QPDF* context);

    // Helpers for parsing content streams
    QPDF_DLL
    static void parseContentStream(QPDFObjectHandle stream_or_array,
                                   ParserCallbacks* callbacks);

    // Type-specific factories
    QPDF_DLL
    static QPDFObjectHandle newNull();
    QPDF_DLL
    static QPDFObjectHandle newBool(bool value);
    QPDF_DLL
    static QPDFObjectHandle newInteger(long long value);
    QPDF_DLL
    static QPDFObjectHandle newReal(std::string const& value);
    QPDF_DLL
    static QPDFObjectHandle newReal(double value, int decimal_places = 0);
    QPDF_DLL
    static QPDFObjectHandle newName(std::string const& name);
    QPDF_DLL
    static QPDFObjectHandle newString(std::string const& str);
    QPDF_DLL
    static QPDFObjectHandle newOperator(std::string const&);
    QPDF_DLL
    static QPDFObjectHandle newInlineImage(std::string const&);
    QPDF_DLL
    static QPDFObjectHandle newArray();
    QPDF_DLL
    static QPDFObjectHandle newArray(
	std::vector<QPDFObjectHandle> const& items);
    QPDF_DLL
    static QPDFObjectHandle newDictionary();
    QPDF_DLL
    static QPDFObjectHandle newDictionary(
	std::map<std::string, QPDFObjectHandle> const& items);

    // Create a new stream and associate it with the given qpdf
    // object.  A subsequent call must be made to replaceStreamData()
    // to provide data for the stream.  The stream's dictionary may be
    // retrieved by calling getDict(), and the resulting dictionary
    // may be modified.  Alternatively, you can create a new
    // dictionary and call replaceDict to install it.
    QPDF_DLL
    static QPDFObjectHandle newStream(QPDF* qpdf);

    // Create a new stream and associate it with the given qpdf
    // object.  Use the given buffer as the stream data.  The stream
    // dictionary's /Length key will automatically be set to the size
    // of the data buffer.  If additional keys are required, the
    // stream's dictionary may be retrieved by calling getDict(), and
    // the resulting dictionary may be modified.  This method is just
    // a convenient wrapper around the newStream() and
    // replaceStreamData().  It is a convenience methods for streams
    // that require no parameters beyond the stream length.  Note that
    // you don't have to deal with compression yourself if you use
    // QPDFWriter.  By default, QPDFWriter will automatically compress
    // uncompressed stream data.  Example programs are provided that
    // illustrate this.
    QPDF_DLL
    static QPDFObjectHandle newStream(QPDF* qpdf, PointerHolder<Buffer> data);

    // Create new stream with data from string.  This method will
    // create a copy of the data rather than using the user-provided
    // buffer as in the PointerHolder<Buffer> version of newStream.
    QPDF_DLL
    static QPDFObjectHandle newStream(QPDF* qpdf, std::string const& data);

    // A reserved object is a special sentinel used for qpdf to
    // reserve a spot for an object that is going to be added to the
    // QPDF object.  Normally you don't have to use this type since
    // you can just call QPDF::makeIndirectObject.  However, in some
    // cases, if you have to create objects with circular references,
    // you may need to create a reserved object so that you can have a
    // reference to it and then replace the object later.  Reserved
    // objects have the special property that they can't be resolved
    // to direct objects.  This makes it possible to replace a
    // reserved object with a new object while preserving existing
    // references to them.  When you are ready to replace a reserved
    // object with its replacement, use QPDF::replaceReserved for this
    // purpose rather than the more general QPDF::replaceObject.  It
    // is an error to try to write a QPDF with QPDFWriter if it has
    // any reserved objects in it.
    QPDF_DLL
    static QPDFObjectHandle newReserved(QPDF* qpdf);

    // Accessor methods.  If an accessor method that is valid for only
    // a particular object type is called on an object of the wrong
    // type, an exception is thrown.

    // Methods for bool objects
    QPDF_DLL
    bool getBoolValue();

    // Methods for integer objects
    QPDF_DLL
    long long getIntValue();

    // Methods for real objects
    QPDF_DLL
    std::string getRealValue();

    // Methods that work for both integer and real objects
    QPDF_DLL
    bool isNumber();
    QPDF_DLL
    double getNumericValue();

    // Methods for name objects; see also name and array objects
    QPDF_DLL
    std::string getName();

    // Methods for string objects
    QPDF_DLL
    std::string getStringValue();
    QPDF_DLL
    std::string getUTF8Value();

    // Methods for content stream objects
    QPDF_DLL
    std::string getOperatorValue();
    QPDF_DLL
    std::string getInlineImageValue();

    // Methods for array objects; see also name and array objects
    QPDF_DLL
    int getArrayNItems();
    QPDF_DLL
    QPDFObjectHandle getArrayItem(int n);
    QPDF_DLL
    std::vector<QPDFObjectHandle> getArrayAsVector();

    // Methods for dictionary objects
    QPDF_DLL
    bool hasKey(std::string const&);
    QPDF_DLL
    QPDFObjectHandle getKey(std::string const&);
    QPDF_DLL
    std::set<std::string> getKeys();
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getDictAsMap();

    // Methods for name and array objects
    QPDF_DLL
    bool isOrHasName(std::string const&);

    // Return the QPDF object that owns an indirect object.  Returns
    // null for a direct object.
    QPDF_DLL
    QPDF* getOwningQPDF();

    // Create a shallow copy of an object as a direct object.  Since
    // this is a shallow copy, for dictionaries and arrays, any keys
    // or items that were indirect objects will still be indirect
    // objects that point to the same place.
    QPDF_DLL
    QPDFObjectHandle shallowCopy();

    // Mutator methods.  Use with caution.

    // Recursively copy this object, making it direct.  Throws an
    // exception if a loop is detected or any sub-object is a stream.
    QPDF_DLL
    void makeDirect();

    // Mutator methods for array objects
    QPDF_DLL
    void setArrayItem(int, QPDFObjectHandle const&);
    QPDF_DLL
    void setArrayFromVector(std::vector<QPDFObjectHandle> const& items);
    // Insert an item before the item at the given position ("at") so
    // that it has that position after insertion.  If "at" is equal to
    // the size of the array, insert the item at the end.
    QPDF_DLL
    void insertItem(int at, QPDFObjectHandle const& item);
    QPDF_DLL
    void appendItem(QPDFObjectHandle const& item);
    // Remove the item at that position, reducing the size of the
    // array by one.
    QPDF_DLL
    void eraseItem(int at);

    // Mutator methods for dictionary objects

    // Replace value of key, adding it if it does not exist
    QPDF_DLL
    void replaceKey(std::string const& key, QPDFObjectHandle const&);
    // Remove key, doing nothing if key does not exist
    QPDF_DLL
    void removeKey(std::string const& key);
    // If the object is null, remove the key.  Otherwise, replace it.
    QPDF_DLL
    void replaceOrRemoveKey(std::string const& key, QPDFObjectHandle);

    // Methods for stream objects
    QPDF_DLL
    QPDFObjectHandle getDict();

    // Returns filtered (uncompressed) stream data.  Throws an
    // exception if the stream is filtered and we can't decode it.
    QPDF_DLL
    PointerHolder<Buffer> getStreamData(
        qpdf_stream_decode_level_e level = qpdf_dl_generalized);

    // Returns unfiltered (raw) stream data.
    QPDF_DLL
    PointerHolder<Buffer> getRawStreamData();

    // Write stream data through the given pipeline. A null pipeline
    // value may be used if all you want to do is determine whether a
    // stream is filterable and would be filtered based on the
    // provided flags. If flags is 0, write raw stream data and return
    // false. Otherwise, the flags alter the behavior in the following
    // way:
    //
    // encode_flags:
    //
    // qpdf_sf_compress -- compress data with /FlateDecode if no other
    // compression filters are applied.
    //
    // qpdf_sf_normalize -- tokenize as content stream and normalize tokens
    //
    // decode_level:
    //
    // qpdf_dl_none -- do not decode any streams.
    //
    // qpdf_dl_generalized -- decode supported general-purpose
    // filters. This includes /ASCIIHexDecode, /ASCII85Decode,
    // /LZWDecode, and /FlateDecode.
    //
    // qpdf_dl_specialized -- in addition to generalized filters, also
    // decode supported non-lossy specialized filters. This includes
    // /RunLengthDecode.
    //
    // qpdf_dl_all -- in addition to generalized and non-lossy
    // specialized filters, decode supported lossy filters. This
    // includes /DCTDecode.
    //
    // If, based on the flags and the filters and decode parameters,
    // we determine that we know how to apply all requested filters,
    // do so and return true if we are successful.
    //
    // In all cases, a return value of true means that filtered data
    // has been written successfully. If filtering is requested but
    // this method returns false, it means there was some error in the
    // filtering, in which case the resulting data is likely partially
    // filtered and/or incomplete and may not be consistent with the
    // configured filters. QPDFWriter handles this by attempting to
    // get the stream data without filtering, but callers should
    // consider a false return value when decode_level is not
    // qpdf_dl_none to be a potential loss of data. If you intend to
    // retry in that case, pass true as the value of will_retry. This
    // changes the warning issued by the library to indicate that the
    // operation will be retried without filtering to avoid data loss.
    QPDF_DLL
    bool pipeStreamData(Pipeline*,
                        unsigned long encode_flags,
                        qpdf_stream_decode_level_e decode_level,
                        bool suppress_warnings = false);
    QPDF_DLL
    bool pipeStreamData(Pipeline*,
                        unsigned long encode_flags,
                        qpdf_stream_decode_level_e decode_level,
                        bool suppress_warnings,
                        bool will_retry);

    // Legacy pipeStreamData. This maps to the the flags-based
    // pipeStreamData as follows:
    //  filter = false                  -> encode_flags = 0
    //  filter = true                   -> decode_level = qpdf_dl_generalized
    //    normalize = true -> encode_flags |= qpdf_sf_normalize
    //    compress = true  -> encode_flags |= qpdf_sf_compress
    QPDF_DLL
    bool pipeStreamData(Pipeline*, bool filter,
			bool normalize, bool compress);

    // Replace a stream's dictionary.  The new dictionary must be
    // consistent with the stream's data.  This is most appropriately
    // used when creating streams from scratch that will use a stream
    // data provider and therefore start with an empty dictionary.  It
    // may be more convenient in this case than calling getDict and
    // modifying it for each key.  The pdf-create example does this.
    QPDF_DLL
    void replaceDict(QPDFObjectHandle);

    // Replace this stream's stream data with the given data buffer,
    // and replace the /Filter and /DecodeParms keys in the stream
    // dictionary with the given values.  (If either value is empty,
    // the corresponding key is removed.)  The stream's /Length key is
    // replaced with the length of the data buffer.  The stream is
    // interpreted as if the data read from the file, after any
    // decryption filters have been applied, is as presented.
    QPDF_DLL
    void replaceStreamData(PointerHolder<Buffer> data,
			   QPDFObjectHandle const& filter,
			   QPDFObjectHandle const& decode_parms);

    // Replace the stream's stream data with the given string.
    // This method will create a copy of the data rather than using
    // the user-provided buffer as in the PointerHolder<Buffer> version
    // of replaceStreamData.
    QPDF_DLL
    void replaceStreamData(std::string const& data,
			   QPDFObjectHandle const& filter,
			   QPDFObjectHandle const& decode_parms);

    // As above, replace this stream's stream data.  Instead of
    // directly providing a buffer with the stream data, call the
    // given provider's provideStreamData method.  See comments on the
    // StreamDataProvider class (defined above) for details on the
    // method.  The data must be consistent with filter and
    // decode_parms as provided.  Although it is more complex to use
    // this form of replaceStreamData than the one that takes a
    // buffer, it makes it possible to avoid allocating memory for the
    // stream data.  Example programs are provided that use both forms
    // of replaceStreamData.

    // Note about stream length: for any given stream, the provider
    // must provide the same amount of data each time it is called.
    // This is critical for making linearization work properly.
    // Versions of qpdf before 3.0.0 required a length to be specified
    // here.  Starting with version 3.0.0, this is no longer necessary
    // (or permitted).  The first time the stream data provider is
    // invoked for a given stream, the actual length is stored.
    // Subsequent times, it is enforced that the length be the same as
    // the first time.

    // If you have gotten a compile error here while building code
    // that worked with older versions of qpdf, just omit the length
    // parameter.  You can also simplify your code by not having to
    // compute the length in advance.
    QPDF_DLL
    void replaceStreamData(PointerHolder<StreamDataProvider> provider,
			   QPDFObjectHandle const& filter,
			   QPDFObjectHandle const& decode_parms);

    // Access object ID and generation.  For direct objects, return
    // object ID 0.

    // NOTE: Be careful about calling getObjectID() and
    // getGeneration() directly as this can lead to the pattern of
    // depending on object ID or generation without the other.  In
    // general, when keeping track of object IDs, it's better to use
    // QPDFObjGen instead.

    QPDF_DLL
    QPDFObjGen getObjGen() const;
    QPDF_DLL
    int getObjectID() const;
    QPDF_DLL
    int getGeneration() const;

    QPDF_DLL
    std::string unparse();
    QPDF_DLL
    std::string unparseResolved();

    // Convenience routines for commonly performed functions

    // Throws an exception if this is not a Page object.  Returns an
    // empty map if there are no images or no resources.  This
    // function does not presently support inherited resources.  If
    // this is a significant concern, call
    // pushInheritedAttributesToPage() on the QPDF object that owns
    // this page.  See comment in the source for details.  Return
    // value is a map from XObject name to the image object, which is
    // always a stream.
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getPageImages();

    // Returns a vector of stream objects representing the content
    // streams for the given page.  This routine allows the caller to
    // not care whether there are one or more than one content streams
    // for a page.  Throws an exception if this is not a Page object.
    QPDF_DLL
    std::vector<QPDFObjectHandle> getPageContents();

    // Add the given object as a new content stream for this page.  If
    // parameter 'first' is true, add to the beginning.  Otherwise,
    // add to the end.  This routine automatically converts the page
    // contents to an array if it is a scalar, allowing the caller not
    // to care what the initial structure is.  Throws an exception if
    // this is not a Page object.
    QPDF_DLL
    void addPageContents(QPDFObjectHandle contents, bool first);

    // Rotate a page. If relative is false, set the rotation of the
    // page to angle. Otherwise, add angle to the rotation of the
    // page. Angle must be a multiple of 90. Adding 90 to the rotation
    // rotates clockwise by 90 degrees.
    QPDF_DLL
    void rotatePage(int angle, bool relative);

    // Initializers for objects.  This Factory class gives the QPDF
    // class specific permission to call factory methods without
    // making it a friend of the whole QPDFObjectHandle class.
    class Factory
    {
	friend class QPDF;
      private:
	static QPDFObjectHandle newIndirect(QPDF* qpdf,
					    int objid, int generation)
	{
	    return QPDFObjectHandle::newIndirect(qpdf, objid, generation);
	}
	// object must be dictionary object
	static QPDFObjectHandle newStream(
	    QPDF* qpdf, int objid, int generation,
	    QPDFObjectHandle stream_dict, qpdf_offset_t offset, size_t length)
	{
	    return QPDFObjectHandle::newStream(
		qpdf, objid, generation, stream_dict, offset, length);
	}
    };
    friend class Factory;

    // Accessor for raw underlying object -- only QPDF is allowed to
    // call this.
    class ObjAccessor
    {
	friend class QPDF;
      private:
	static PointerHolder<QPDFObject> getObject(QPDFObjectHandle& o)
	{
	    o.dereference();
	    return o.obj;
	}
    };
    friend class ObjAccessor;

    // Provide access to specific classes for recursive
    // releaseResolved().
    class ReleaseResolver
    {
	friend class QPDF_Dictionary;
	friend class QPDF_Array;
        friend class QPDF_Stream;
      private:
	static void releaseResolved(QPDFObjectHandle& o)
	{
	    o.releaseResolved();
	}
    };
    friend class ReleaseResolver;

    // Convenience routine: Throws if the assumption is violated.
    QPDF_DLL
    void assertInitialized() const;

    QPDF_DLL
    void assertNull();
    QPDF_DLL
    void assertBool();
    QPDF_DLL
    void assertInteger();
    QPDF_DLL
    void assertReal();
    QPDF_DLL
    void assertName();
    QPDF_DLL
    void assertString();
    QPDF_DLL
    void assertOperator();
    QPDF_DLL
    void assertInlineImage();
    QPDF_DLL
    void assertArray();
    QPDF_DLL
    void assertDictionary();
    QPDF_DLL
    void assertStream();
    QPDF_DLL
    void assertReserved();

    QPDF_DLL
    void assertIndirect();
    QPDF_DLL
    void assertScalar();
    QPDF_DLL
    void assertNumber();

    QPDF_DLL
    bool isPageObject();
    QPDF_DLL
    bool isPagesObject();
    QPDF_DLL
    void assertPageObject();

  private:
    QPDFObjectHandle(QPDF*, int objid, int generation);
    QPDFObjectHandle(QPDFObject*);

    enum parser_state_e
    {
        st_top,
        st_start,
        st_stop,
        st_eof,
        st_dictionary,
        st_array
    };

    // Private object factory methods
    static QPDFObjectHandle newIndirect(QPDF*, int objid, int generation);
    static QPDFObjectHandle newStream(
	QPDF* qpdf, int objid, int generation,
	QPDFObjectHandle stream_dict, qpdf_offset_t offset, size_t length);

    void assertType(char const* type_name, bool istype) const;
    void dereference();
    void makeDirectInternal(std::set<int>& visited);
    void releaseResolved();
    static QPDFObjectHandle parseInternal(
        PointerHolder<InputSource> input,
        std::string const& object_description,
        QPDFTokenizer& tokenizer, bool& empty,
        StringDecrypter* decrypter, QPDF* context,
        bool content_stream);
    static void parseContentStream_internal(
        PointerHolder<Buffer> stream_data,
        std::string const& description,
        ParserCallbacks* callbacks);

    // Other methods
    static void warn(QPDF*, QPDFExc const&);

    bool initialized;

    QPDF* qpdf;			// 0 for direct object
    int objid;			// 0 for direct object
    int generation;
    PointerHolder<QPDFObject> obj;
    bool reserved;
};

#endif // __QPDFOBJECTHANDLE_HH__
