// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QPDFOBJECTHANDLE_HH__
#define __QPDFOBJECTHANDLE_HH__

#include <string>
#include <vector>
#include <set>
#include <map>

#include <qpdf/DLL.hh>

#include <qpdf/PointerHolder.hh>
#include <qpdf/Buffer.hh>

#include <qpdf/QPDFObject.hh>

class Pipeline;
class QPDF;

class QPDFObjectHandle
{
  public:
    DLL_EXPORT
    QPDFObjectHandle();
    DLL_EXPORT
    bool isInitialized() const;

    // Exactly one of these will return true for any object.
    DLL_EXPORT
    bool isBool();
    DLL_EXPORT
    bool isNull();
    DLL_EXPORT
    bool isInteger();
    DLL_EXPORT
    bool isReal();
    DLL_EXPORT
    bool isName();
    DLL_EXPORT
    bool isString();
    DLL_EXPORT
    bool isArray();
    DLL_EXPORT
    bool isDictionary();
    DLL_EXPORT
    bool isStream();

    // This returns true in addition to the query for the specific
    // type for indirect objects.
    DLL_EXPORT
    bool isIndirect();

    // True for everything except array, dictionary, and stream
    DLL_EXPORT
    bool isScalar();

    // Public factory methods

    DLL_EXPORT
    static QPDFObjectHandle newNull();
    DLL_EXPORT
    static QPDFObjectHandle newBool(bool value);
    DLL_EXPORT
    static QPDFObjectHandle newInteger(int value);
    DLL_EXPORT
    static QPDFObjectHandle newReal(std::string const& value);
    DLL_EXPORT
    static QPDFObjectHandle newName(std::string const& name);
    DLL_EXPORT
    static QPDFObjectHandle newString(std::string const& str);
    DLL_EXPORT
    static QPDFObjectHandle newArray(
	std::vector<QPDFObjectHandle> const& items);
    DLL_EXPORT
    static QPDFObjectHandle newDictionary(
	std::map<std::string, QPDFObjectHandle> const& items);

    // Accessor methods.  If an accessor method that is valid for only
    // a particular object type is called on an object of the wrong
    // type, an exception is thrown.

    // Methods for bool objects
    DLL_EXPORT
    bool getBoolValue();

    // Methods for integer objects
    DLL_EXPORT
    int getIntValue();

    // Methods for real objects
    DLL_EXPORT
    std::string getRealValue();

    // Methods that work for both integer and real objects
    DLL_EXPORT
    bool isNumber();
    DLL_EXPORT
    double getNumericValue();

    // Methods for name objects
    DLL_EXPORT
    std::string getName();

    // Methods for string objects
    DLL_EXPORT
    std::string getStringValue();
    DLL_EXPORT
    std::string getUTF8Value();

    // Methods for array objects
    DLL_EXPORT
    int getArrayNItems();
    DLL_EXPORT
    QPDFObjectHandle getArrayItem(int n);

    // Methods for dictionary objects
    DLL_EXPORT
    bool hasKey(std::string const&);
    DLL_EXPORT
    QPDFObjectHandle getKey(std::string const&);
    DLL_EXPORT
    std::set<std::string> getKeys();

    // Mutator methods.  Use with caution.

    // Recursively copy this object, making it direct.  Throws an
    // exception if a loop is detected or any sub-object is a stream.
    DLL_EXPORT
    void makeDirect();

    // Mutator methods for array objects
    DLL_EXPORT
    void setArrayItem(int, QPDFObjectHandle const&);

    // Mutator methods for dictionary objects

    // Replace value of key, adding it if it does not exist
    DLL_EXPORT
    void replaceKey(std::string const& key, QPDFObjectHandle const&);
    // Remove key, doing nothing if key does not exist
    DLL_EXPORT
    void removeKey(std::string const& key);

    // Methods for stream objects
    DLL_EXPORT
    QPDFObjectHandle getDict();

    // Returns filtered (uncompressed) stream data.  Throws an
    // exception if the stream is filtered and we can't decode it.
    DLL_EXPORT
    PointerHolder<Buffer> getStreamData();

    // Write stream data through the given pipeline.  A null pipeline
    // value may be used if all you want to do is determine whether a
    // stream is filterable.  If filter is false, write raw stream
    // data and return false.  If filter is true, then attempt to
    // apply all the decoding filters to the stream data.  If we are
    // successful, return true.  Otherwise, return false and write raw
    // data.  If filtering is requested and successfully performed,
    // then the normalize and compress flags are used to determine
    // whether stream data should be normalized and compressed.  In
    // all cases, if this function returns false, raw data has been
    // written.  If it returns true, then any requested filtering has
    // been performed.  Note that if the original stream data has no
    // filters applied to it, the return value will be equal to the
    // value of the filter parameter.  Callers may use the return
    // value of this function to determine whether or not the /Filter
    // and /DecodeParms keys in the stream dictionary should be
    // replaced if writing a new stream object.
    DLL_EXPORT
    bool pipeStreamData(Pipeline*, bool filter,
			bool normalize, bool compress);

    // return 0 for direct objects
    DLL_EXPORT
    int getObjectID() const;
    DLL_EXPORT
    int getGeneration() const;

    DLL_EXPORT
    std::string unparse();
    DLL_EXPORT
    std::string unparseResolved();

    // Convenience routines for commonly performed functions

    // Throws an exception if this is not a Page object.  Returns an
    // empty map if there are no images or no resources.  This
    // function does not presently support inherited resources.  See
    // comment in the source for details.  Return value is a map from
    // XObject name to the image object, which is always a stream.
    DLL_EXPORT
    std::map<std::string, QPDFObjectHandle> getPageImages();

    // Throws an exception if this is not a Page object.  Returns a
    // vector of stream objects representing the content streams for
    // the given page.  This routine allows the caller to not care
    // whether there are one or more than one content streams for a
    // page.
    DLL_EXPORT
    std::vector<QPDFObjectHandle> getPageContents();

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
	    QPDFObjectHandle stream_dict, off_t offset, int length)
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

  private:
    QPDFObjectHandle(QPDF*, int objid, int generation);
    QPDFObjectHandle(QPDFObject*);

    // Private object factory methods
    static QPDFObjectHandle newIndirect(QPDF*, int objid, int generation);
    static QPDFObjectHandle newStream(
	QPDF* qpdf, int objid, int generation,
	QPDFObjectHandle stream_dict, off_t offset, int length);

    void assertInitialized() const;
    void assertType(char const* type_name, bool istype);
    void assertPageObject();
    void dereference();
    void makeDirectInternal(std::set<int>& visited);

    bool initialized;

    QPDF* qpdf;			// 0 for direct object
    int objid;			// 0 for direct object
    int generation;
    PointerHolder<QPDFObject> obj;
};

#endif // __QPDFOBJECTHANDLE_HH__
