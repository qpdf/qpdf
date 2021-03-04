// Copyright (c) 2005-2021 Jay Berkenbilt
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

#ifndef QPDFOBJECTHANDLE_HH
#define QPDFOBJECTHANDLE_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>
#include <qpdf/Constants.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <functional>

#include <qpdf/QPDFObjGen.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Buffer.hh>
#include <qpdf/InputSource.hh>
#include <qpdf/QPDFTokenizer.hh>

#include <qpdf/QPDFObject.hh>

class Pipeline;
class QPDF;
class QPDF_Dictionary;
class QPDF_Array;
class QPDFTokenizer;
class QPDFExc;
class Pl_QPDFTokenizer;
class QPDFMatrix;

class QPDFObjectHandle
{
  public:
    // This class is used by replaceStreamData.  It provides an
    // alternative way of associating stream data with a stream.  See
    // comments on replaceStreamData and newStream for additional
    // details.
    class QPDF_DLL_CLASS StreamDataProvider
    {
      public:
        QPDF_DLL
        StreamDataProvider(bool supports_retry = false);

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
	// data. Note that, when writing linearized files, qpdf will
	// call your provideStreamData twice, and if it generates
	// different output, you risk generating invalid output or
	// having qpdf throw an exception. The object ID and
	// generation passed to this method are those that belong to
	// the stream on behalf of which the provider is called. They
	// may be ignored or used by the implementation for indexing
	// or other purposes. This information is made available just
	// to make it more convenient to use a single
	// StreamDataProvider object to provide data for multiple
	// streams.

        // A few things to keep in mind:
        //
        // * Stream data providers must not modify any objects since
        //   they may be called after some parts of the file have
        //   already been written.
        //
        // * Since qpdf may call provideStreamData multiple times when
        //   writing linearized files, if the work done by your stream
        //   data provider is slow or computationally intensive, you
        //   might want to implement your own cache.
        //
        // * Once you have called replaceStreamData, the original
        //   stream data is no longer directly accessible from the
        //   stream, but this is easy to work around by copying the
        //   stream to a separate QPDF object. The qpdf library
        //   implements this very efficiently without actually making
        //   a copy of the stream data. You can find examples of this
        //   pattern in some of the examples, including
        //   pdf-custom-filter.cc and pdf-invert-images.cc.

        // Prior to qpdf 10.0.0, it was not possible to handle errors
        // the way pipeStreamData does or to pass back success.
        // Starting in qpdf 10.0.0, those capabilities have been added
        // by allowing an alternative provideStreamData to be
        // implemented. You must implement at least one of the
        // versions of provideStreamData below. If you implement the
        // version that supports retry and returns a value, you should
        // pass true as the value of supports_retry in the base class
        // constructor. This will cause the library to call that
        // version of the method, which should also return a boolean
        // indicating whether it ran without errors.
        QPDF_DLL
	virtual void provideStreamData(int objid, int generation,
				       Pipeline* pipeline);
        QPDF_DLL
	virtual bool provideStreamData(
            int objid, int generation, Pipeline* pipeline,
            bool suppress_warnings, bool will_retry);
        QPDF_DLL
	bool supportsRetry();

      private:
        bool supports_retry;
    };

    // The TokenFilter class provides a way to filter content streams
    // in a lexically aware fashion. TokenFilters can be attached to
    // streams using the addTokenFilter or addContentTokenFilter
    // methods or can be applied on the spot by filterPageContents.
    // You may also use Pl_QPDFTokenizer directly if you need full
    // control.
    //
    // The handleToken method is called for each token, including the
    // eof token, and then handleEOF is called at the very end.
    // Handlers may call write (or writeToken) to pass data
    // downstream. Please see examples/pdf-filter-tokens.cc and
    // examples/pdf-count-strings.cc for examples of using
    // TokenFilters.
    //
    // Please note that when you call token.getValue() on a token of
    // type tt_string, you get the string value without any
    // delimiters. token.getRawValue() will return something suitable
    // for being written to output, or calling writeToken with a
    // string token will also work. The correct way to construct a
    // string token that would write the literal value (str) is
    // QPDFTokenizer::Token(QPDFTokenizer::tt_string, "str"). A
    // similar situation exists with tt_name. token.getValue() returns
    // a normalized name with # codes resolved into characters, and
    // may not be suitable for writing. You can pass it to
    // QPDF_Name::normalizeName first, or you can use writeToken with
    // a name token. The correct way to create a name token is
    // QPDFTokenizer::Token(QPDFTokenizer::tt_name, "/Name").
    class QPDF_DLL_CLASS TokenFilter
    {
      public:
        QPDF_DLL
        TokenFilter()
        {
        }
        QPDF_DLL
        virtual ~TokenFilter()
        {
        }
        virtual void handleToken(QPDFTokenizer::Token const&) = 0;
        QPDF_DLL
        virtual void handleEOF();

        class PipelineAccessor
        {
            friend class Pl_QPDFTokenizer;
          private:
            static void setPipeline(TokenFilter* f, Pipeline* p)
            {
                f->setPipeline(p);
            }
        };

      protected:
        QPDF_DLL
        void write(char const* data, size_t len);
        QPDF_DLL
        void write(std::string const& str);
        QPDF_DLL
        void writeToken(QPDFTokenizer::Token const&);

      private:
        void setPipeline(Pipeline*);

        Pipeline* pipeline;
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

    // This class is used by parsePageContents. Callers must
    // instantiate a subclass of this with handlers defined to accept
    // QPDFObjectHandles that are parsed from the stream.
    class QPDF_DLL_CLASS ParserCallbacks
    {
      public:
        QPDF_DLL
        virtual ~ParserCallbacks()
        {
        }
        // One of the handleObject methods must be overridden.
        QPDF_DLL
        virtual void handleObject(QPDFObjectHandle);
        QPDF_DLL
        virtual void handleObject(
            QPDFObjectHandle, size_t offset, size_t length);

        virtual void handleEOF() = 0;

        // Override this if you want to know the full size of the
        // contents, possibly after concatenation of multiple streams.
        // This is called before the first call to handleObject.
        QPDF_DLL
        virtual void contentSize(size_t);

      protected:
        // Implementors may call this method during parsing to
        // terminate parsing early. This method throws an exception
        // that is caught by parsePageContents, so its effect is
        // immediate.
        QPDF_DLL
        void terminateParsing();
    };

    // Convenience object for rectangles
    class Rectangle
    {
      public:
        Rectangle() :
            llx(0.0),
            lly(0.0),
            urx(0.0),
            ury(0.0)
        {
        }
        Rectangle(double llx, double lly,
                  double urx, double ury) :
            llx(llx),
            lly(lly),
            urx(urx),
            ury(ury)
        {
        }

        double llx;
        double lly;
        double urx;
        double ury;
    };

    // Convenience object for transformation matrices. See also
    // QPDFMatrix. Unfortunately we can't replace this with QPDFMatrix
    // because QPDFMatrix's default constructor creates the identity
    // transform matrix and this one is all zeroes.
    class Matrix
    {
      public:
        Matrix() :
            a(0.0),
            b(0.0),
            c(0.0),
            d(0.0),
            e(0.0),
            f(0.0)
        {
        }
        Matrix(double a, double b, double c,
               double d, double e, double f) :
            a(a),
            b(b),
            c(c),
            d(d),
            e(e),
            f(f)
        {
        }

        double a;
        double b;
        double c;
        double d;
        double e;
        double f;
    };

    QPDF_DLL
    QPDFObjectHandle();
    QPDF_DLL
    QPDFObjectHandle(QPDFObjectHandle const&) = default;
    QPDF_DLL
    QPDFObjectHandle&
    operator=(QPDFObjectHandle const&) = default;
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

    // True for objects that are direct nulls. Does not attempt to
    // resolve objects. This is intended for internal use, but it can
    // be used as an efficient way to check for nulls that are not
    // indirect objects.
    QPDF_DLL
    bool isDirectNull() const;

    // This returns true in addition to the query for the specific
    // type for indirect objects.
    QPDF_DLL
    bool isIndirect();

    // True for everything except array, dictionary, stream, word, and
    // inline image.
    QPDF_DLL
    bool isScalar();

    // Public factory methods

    // Wrap an object in an array if it is not already an array. This
    // is a helper for cases in which something in a PDF may either be
    // a single item or an array of items, which is a common idiom.
    QPDF_DLL
    QPDFObjectHandle wrapInArray();

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

    // Construct an object of any type from a string representation of
    // the object. Indirect object syntax (obj gen R) is allowed and
    // will create indirect references within the passed-in context.
    // If object_description is provided, it will appear in the
    // message of any QPDFExc exception thrown for invalid syntax.
    // Note that you can't parse an indirect object reference all by
    // itself as parse will stop at the end of the first complete
    // object, which will just be the first number and will report
    // that there is trailing data at the end of the string.
    QPDF_DLL
    static QPDFObjectHandle parse(QPDF* context,
                                  std::string const& object_str,
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

    // Return the offset where the object was found when parsed. A
    // negative value means that the object was created without
    // parsing. If the object is in a stream, the offset is from the
    // beginning of the stream. Otherwise, the offset is from the
    // beginning of the file.
    QPDF_DLL
    qpdf_offset_t getParsedOffset();

    // Older method: stream_or_array should be the value of /Contents
    // from a page object. It's more convenient to just call
    // QPDFPageObjectHelper::parsePageContents on the page object, and
    // error messages will also be more useful because the page object
    // information will be known.
    QPDF_DLL
    static void parseContentStream(QPDFObjectHandle stream_or_array,
                                   ParserCallbacks* callbacks);

    // When called on a stream or stream array that is some page's
    // content streams, do the same as pipePageContents. This method
    // is a lower level way to do what
    // QPDFPageObjectHelper::pipePageContents does, but it allows you
    // to perform this operation on a contents object that is
    // disconnected from a page object. The description argument
    // should describe the containing page and is used in error
    // messages. The all_description argument is initialized to
    // something that could be used to describe the result of the
    // pipeline. It is the description amended with the identifiers of
    // the underlying objects. Please note that if there is an array
    // of content streams, p->finish() is called after each stream. If
    // you pass a pipeline that doesn't allow write() to be called
    // after finish(), you can wrap it in an instance of
    // Pl_Concatenate and then call manualFinish() on the
    // Pl_Concatenate pipeline at the end.
    QPDF_DLL
    void pipeContentStreams(Pipeline* p, std::string const& description,
                            std::string& all_description);

    // As of qpdf 8, it is possible to add custom token filters to a
    // stream. The tokenized stream data is passed through the token
    // filter after all original filters but before content stream
    // normalization if requested. This is a low-level interface to
    // add it to a stream. You will usually want to call
    // QPDFPageObjectHelper::addContentTokenFilter instead, which can
    // be applied to a page object, and which will automatically
    // handle the case of pages whose contents are split across
    // multiple streams.
    void addTokenFilter(PointerHolder<TokenFilter> token_filter);

    // Legacy helpers for parsing content streams. These methods are
    // not going away, but newer code should call the correspond
    // methods in QPDFPageObjectHelper instead. The specification and
    // behavior of these methods are the same as the identically named
    // methods in that class, but newer functionality will be added
    // there.
    QPDF_DLL
    void parsePageContents(ParserCallbacks* callbacks);
    QPDF_DLL
    void filterPageContents(TokenFilter* filter, Pipeline* next = 0);
    // See comments for QPDFPageObjectHelper::pipeContents.
    QPDF_DLL
    void pipePageContents(Pipeline* p);
    QPDF_DLL
    void addContentTokenFilter(PointerHolder<TokenFilter> token_filter);
    // End legacy content stream helpers

    // Called on a stream to filter the stream as if it were page
    // contents. This can be used to apply a TokenFilter to a form
    // XObject, whose data is in the same format as a content stream.
    QPDF_DLL
    void filterAsContents(TokenFilter* filter, Pipeline* next = 0);
    // Called on a stream to parse the stream as page contents. This
    // can be used to parse a form XObject.
    QPDF_DLL
    void parseAsContents(ParserCallbacks* callbacks);

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
    // ABI: combine with other newReal by adding trim_trailing_zeroes
    // above as an optional parameter with a default of true.
    QPDF_DLL
    static QPDFObjectHandle newReal(double value, int decimal_places,
                                    bool trim_trailing_zeroes);
    QPDF_DLL
    static QPDFObjectHandle newName(std::string const& name);
    QPDF_DLL
    static QPDFObjectHandle newString(std::string const& str);
    // Create a string encoded from the given utf8-encoded string
    // appropriately encoded to appear in PDF files outside of content
    // streams, such as in document metadata form field values, page
    // labels, outlines, and similar locations. We try ASCII first,
    // then PDFDocEncoding, then UTF-16 as needed to successfully
    // encode all the characters.
    QPDF_DLL
    static QPDFObjectHandle newUnicodeString(std::string const& utf8_str);
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
    static QPDFObjectHandle newArray(Rectangle const&);
    QPDF_DLL
    static QPDFObjectHandle newArray(Matrix const&);
    QPDF_DLL
    static QPDFObjectHandle newArray(QPDFMatrix const&);
    QPDF_DLL
    static QPDFObjectHandle newDictionary();
    QPDF_DLL
    static QPDFObjectHandle newDictionary(
	std::map<std::string, QPDFObjectHandle> const& items);

    // Create an array from a rectangle. Equivalent to the rectangle
    // form of newArray.
    QPDF_DLL
    static QPDFObjectHandle newFromRectangle(Rectangle const&);
    // Create an array from a matrix. Equivalent to the matrix
    // form of newArray.
    QPDF_DLL
    static QPDFObjectHandle newFromMatrix(Matrix const&);
    QPDF_DLL
    static QPDFObjectHandle newFromMatrix(QPDFMatrix const&);

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

    // Provide an owning qpdf and object description. The library does
    // this automatically with objects that are read from from the
    // input PDF and with objects that are created programmatically
    // and inserted into the QPDF by adding them to an array or a
    // dictionary or creating a new indirect object. Most end user
    // code will not need to call this. If an object has an owning
    // qpdf and object description, it enables qpdf to give warnings
    // with proper context in some cases where it would otherwise
    // raise exceptions.
    QPDF_DLL
    void setObjectDescription(QPDF* owning_qpdf,
                              std::string const& object_description);
    QPDF_DLL
    bool hasObjectDescription();

    // Accessor methods.  If an accessor method that is valid for only
    // a particular object type is called on an object of the wrong
    // type, an exception is thrown.

    // Methods for bool objects
    QPDF_DLL
    bool getBoolValue();

    // Methods for integer objects
    QPDF_DLL
    long long getIntValue();
    QPDF_DLL
    int getIntValueAsInt();
    QPDF_DLL
    unsigned long long getUIntValue();
    QPDF_DLL
    unsigned int getUIntValueAsUInt();

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
    // If a string starts with the UTF-16 marker, it is converted from
    // UTF-16 to UTF-8. Otherwise, it is treated as a string encoded
    // with PDF Doc Encoding. PDF Doc Encoding is identical to
    // ISO-8859-1 except in the range from 0200 through 0240, where
    // there is a mapping of characters to Unicode. QPDF versions
    // prior to version erroneously left characters in that range
    // unmapped.
    QPDF_DLL
    std::string getUTF8Value();

    // Methods for content stream objects
    QPDF_DLL
    std::string getOperatorValue();
    QPDF_DLL
    std::string getInlineImageValue();

    // Methods for array objects; see also name and array objects.

    // Return an object that enables iteration over members. You can
    // do
    //
    // for (auto iter: obj.aitems())
    // {
    //     // iter is an array element
    // }
    class QPDFArrayItems;
    QPDF_DLL
    QPDFArrayItems aitems();

    QPDF_DLL
    int getArrayNItems();
    QPDF_DLL
    QPDFObjectHandle getArrayItem(int n);
    // Note: QPDF arrays internally optimize memory for arrays
    // containing lots of nulls. Calling getArrayAsVector may cause a
    // lot of memory to be allocated for very large arrays with lots
    // of nulls.
    QPDF_DLL
    std::vector<QPDFObjectHandle> getArrayAsVector();
    QPDF_DLL
    bool isRectangle();
    // If the array an array of four numeric values, return as a
    // rectangle. Otherwise, return the rectangle [0, 0, 0, 0]
    QPDF_DLL
    Rectangle getArrayAsRectangle();
    QPDF_DLL
    bool isMatrix();
    // If the array an array of six numeric values, return as a
    // matrix. Otherwise, return the matrix [1, 0, 0, 1, 0, 0]
    QPDF_DLL
    Matrix getArrayAsMatrix();

    // Methods for dictionary objects.

    // Return an object that enables iteration over members. You can
    // do
    //
    // for (auto iter: obj.ditems())
    // {
    //     // iter.first is the key
    //     // iter.second is the value
    // }
    class QPDFDictItems;
    QPDF_DLL
    QPDFDictItems ditems();

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

    // Make all resources in a resource dictionary indirect. This just
    // goes through all entries of top-level subdictionaries and
    // converts any direct objects to indirect objects. This can be
    // useful to call before mergeResources if it is going to be
    // called multiple times to prevent resources from being copied
    // multiple times.
    QPDF_DLL
    void makeResourcesIndirect(QPDF& owning_qpdf);

    // Merge resource dictionaries. If the "conflicts" parameter is
    // provided, conflicts in dictionary subitems are resolved, and
    // "conflicts" is initialized to a map such that
    // conflicts[resource_type][old_key] == [new_key]
    //
    // See also makeResourcesIndirect, which can be useful to call
    // before calling this.
    //
    // This method does nothing if both this object and the other
    // object are not dictionaries. Otherwise, it has following
    // behavior, where "object" refers to the object whose method is
    // invoked, and "other" refers to the argument:
    //
    // * For each key in "other" whose value is an array:
    //   * If "object" does not have that entry, shallow copy it.
    //   * Otherwise, if "object" has an array in the same place,
    //     append to that array any objects in "other"'s array that
    //     are not already present.
    // * For each key in "other" whose value is a dictionary:
    //   * If "object" does not have that entry, shallow copy it.
    //   * Otherwise, for each key in the subdictionary:
    //     * If key is not present in "object"'s entry, shallow copy
    //       it if direct or just add it if indirect.
    //     * Otherwise, if conflicts are being detected:
    //       * If there is a key (oldkey) already in the dictionary
    //         that points to the same indirect destination as key,
    //         indicate that key was replaced by oldkey. This would
    //         happen if these two resource dictionaries have
    //         previously been merged.
    //       * Otherwise pick a new key (newkey) that is unique within
    //         the resource dictionary, store that in the resource
    //         dictionary with key's destination as its destination,
    //         and indicate that key was replaced by newkey.
    //
    // The primary purpose of this method is to facilitate merging of
    // resource dictionaries that are supposed to have the same scope
    // as each other. For example, this can be used to merge a form
    // XObject's /Resources dictionary with a form field's /DR or to
    // merge two /DR dictionaries. The "conflicts" parameter may be
    // previously initialized. This method adds to whatever is already
    // there, which can be useful when merging with multiple things.
    QPDF_DLL
    void mergeResources(
        QPDFObjectHandle other,
        std::map<std::string, std::map<std::string, std::string>>* conflicts);
    // ABI: eliminate version without conflicts and make conflicts
    // default to nullptr.
    QPDF_DLL
    void mergeResources(QPDFObjectHandle other);

    // Get all resource names from a resource dictionary. If this
    // object is a dictionary, this method returns a set of all the
    // keys in all top-level subdictionaries. For resources
    // dictionaries, this is the collection of names that may be
    // referenced in the content stream.
    QPDF_DLL
    std::set<std::string> getResourceNames();

    // Find a unique name within a resource dictionary starting with a
    // given prefix. This method works by appending a number to the
    // given prefix. It searches starting with min_suffix and sets
    // min_suffix to selected value upon return. This can be used to
    // increase efficiency if adding multiple items with the same
    // prefix. (Why doesn't it set min_suffix to the next number?
    // Well, maybe you aren't going to actually use the name it
    // returns.) If you are calling this multiple times on the same
    // resource dictionary, you can initialize resource_names by
    // calling getResourceNames(), incrementally update it as you add
    // resources, and keep passing it in so that getUniqueResourceName
    // doesn't have to traverse the resource dictionary each time it's
    // called.
    QPDF_DLL
    std::string getUniqueResourceName(
        std::string const& prefix,
        int& min_suffix,
        std::set<std::string>* resource_names);
    // ABI: remove this version and make resource_names default to
    // nullptr.
    QPDF_DLL
    std::string getUniqueResourceName(std::string const& prefix,
                                      int& min_suffix);

    // Return the QPDF object that owns an indirect object.  Returns
    // null for a direct object.
    QPDF_DLL
    QPDF* getOwningQPDF();

    // Create a shallow of an object as a direct object, but do not
    // traverse across indirect object boundaries. That means that,
    // for dictionaries and arrays, any keys or items that were
    // indirect objects will still be indirect objects that point to
    // the same place. In the strictest sense, this is not a shallow
    // copy because it recursively descends arrays and dictionaries;
    // it just doesn't cross over indirect objects. See also
    // unsafeShallowCopy(). You can't copy a stream this way. See
    // copyStream() instead.
    QPDF_DLL
    QPDFObjectHandle shallowCopy();

    // Create a true shallow copy of an array or dictionary, just
    // copying the immediate items (array) or keys (dictionary). This
    // is "unsafe" because, if you *modify* any of the items in the
    // copy, you are modifying the original, which is almost never
    // what you want. However, if your intention is merely to
    // *replace* top-level items or keys and not to modify lower-level
    // items in the copy, this method is much faster than
    // shallowCopy().
    QPDF_DLL
    QPDFObjectHandle unsafeShallowCopy();

    // Create a copy of this stream. The new stream and the old stream
    // are independent: after the copy, either the original or the
    // copy's dictionary or data can be modified without affecting the
    // other. This uses StreamDataProvider internally, so no
    // unnecessary copies of the stream's data are made. If the source
    // stream's data is already being provided by a
    // StreamDataProvider, the new stream will use the same one, so
    // you have to make sure your StreamDataProvider can handle that
    // case. But if you're already using a StreamDataProvider, you
    // probably don't need to call this method.
    QPDF_DLL
    QPDFObjectHandle copyStream();

    // Mutator methods.  Use with caution.

    // Recursively copy this object, making it direct. An exception is
    // thrown if a loop is detected. With allow_streams true, keep
    // indirect object references to streams. Otherwise, throw an
    // exception if any sub-object is a stream. Note that, when
    // allow_streams is true and a stream is found, the resulting
    // object is still associated with the containing qpdf. When
    // allow_streams is false, the object will no longer be connected
    // to the original QPDF object after this call completes
    // successfully.
    QPDF_DLL
    void makeDirect(bool allow_streams);
    // Zero-arg version is equivalent to makeDirect(false).
    // ABI: delete zero-arg version of makeDirect, and make
    // allow_streams default to false.
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
    void replaceKey(std::string const& key, QPDFObjectHandle);
    // Remove key, doing nothing if key does not exist
    QPDF_DLL
    void removeKey(std::string const& key);
    // If the object is null, remove the key.  Otherwise, replace it.
    QPDF_DLL
    void replaceOrRemoveKey(std::string const& key, QPDFObjectHandle);

    // Methods for stream objects
    QPDF_DLL
    QPDFObjectHandle getDict();

    // By default, or if true passed, QPDFWriter will attempt to
    // filter a stream based on decode level, whether compression is
    // enabled, and its ability to filter. Passing false will prevent
    // QPDFWriter from attempting to filter the stream even if it can.
    // This includes both decoding and compressing. This makes it
    // possible for you to prevent QPDFWriter from uncompressing and
    // recompressing a stream that it knows how to operate on for any
    // application-specific reason, such as that you have already
    // optimized its filtering. Note that this doesn't affect any
    // other ways to get the stream's data, such as pipeStreamData or
    // getStreamData.
    QPDF_DLL
    void setFilterOnWrite(bool);
    QPDF_DLL
    bool getFilterOnWrite();

    // If addTokenFilter has been called for this stream, then the
    // original data should be considered to be modified. This means we
    // should avoid optimizations such as not filtering a stream that
    // is already compressed.
    QPDF_DLL
    bool isDataModified();

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
    // The exact meaning of the return value differs the different
    // versions of this function, but for any version, the meaning has
    // been the same. For the main version, added in qpdf 10, the
    // return value indicates whether the overall operation succeeded.
    // The filter parameter, if specified, will be set to whether or
    // not filtering was attempted. If filtering was not requested,
    // this value will be false even if the overall operation
    // succeeded.
    //
    // If filtering is requested but this method returns false, it
    // means there was some error in the filtering, in which case the
    // resulting data is likely partially filtered and/or incomplete
    // and may not be consistent with the configured filters.
    // QPDFWriter handles this by attempting to get the stream data
    // without filtering, but callers should consider a false return
    // value when decode_level is not qpdf_dl_none to be a potential
    // loss of data. If you intend to retry in that case, pass true as
    // the value of will_retry. This changes the warning issued by the
    // library to indicate that the operation will be retried without
    // filtering to avoid data loss.

    // Return value is overall success, even if filtering is not
    // requested.
    QPDF_DLL
    bool pipeStreamData(Pipeline*, bool* filtering_attempted,
                        int encode_flags,
                        qpdf_stream_decode_level_e decode_level,
                        bool suppress_warnings = false,
                        bool will_retry = false);

    // Legacy version. Return value is whether filtering was
    // attempted. There is no way to determine success if filtering
    // was not attempted.
    QPDF_DLL
    bool pipeStreamData(Pipeline*,
                        int encode_flags,
                        qpdf_stream_decode_level_e decode_level,
                        bool suppress_warnings = false,
                        bool will_retry = false);

    // Legacy pipeStreamData. This maps to the the flags-based
    // pipeStreamData as follows:
    //  filter = false                  -> encode_flags = 0
    //  filter = true                   -> decode_level = qpdf_dl_generalized
    //    normalize = true -> encode_flags |= qpdf_sf_normalize
    //    compress = true  -> encode_flags |= qpdf_sf_compress
    // Return value is whether filtering was attempted.
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

    // Starting in qpdf 10.2, you can use C++-11 function objects
    // instead of StreamDataProvider.

    // The provider should write the stream data to the pipeline. For
    // a one-liner to replace stream data with the contents of a file,
    // pass QUtil::file_provider(filename) as provider.
    QPDF_DLL
    void replaceStreamData(std::function<void(Pipeline*)> provider,
			   QPDFObjectHandle const& filter,
			   QPDFObjectHandle const& decode_parms);
    // The provider should write the stream data to the pipeline,
    // returning true if it succeeded without errors.
    QPDF_DLL
    void replaceStreamData(
        std::function<bool(Pipeline*,
                           bool suppress_warnings,
                           bool will_retry)> provider,
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
    // For strings only, force binary representation. Otherwise, same
    // as unparse.
    QPDF_DLL
    std::string unparseBinary();

    // Return encoded as JSON. For most object types, there is an
    // obvious mapping. The JSON is generated as follows:
    // * Names are encoded as strings representing the normalized value of
    //   getName()
    // * Indirect references are encoded as strings containing "obj gen R"
    // * Strings are encoded as UTF-8 strings with unrepresentable binary
    //   characters encoded as \uHHHH
    // * Encoding streams just encodes the stream's dictionary; the stream
    //   data is not represented
    // * Object types that are only valid in content streams (inline
    //   image, operator) as well as "reserved" objects are not
    //   representable and will be serialized as "null".
    // If dereference_indirect is true and this is an indirect object,
    // show the actual contents of the object. The effect of
    // dereference_indirect applies only to this object. It is not
    // recursive.
    QPDF_DLL
    JSON getJSON(bool dereference_indirect = false);

    // Legacy helper methods for commonly performed operations on
    // pages. Newer code should use QPDFPageObjectHelper instead. The
    // specification and behavior of these methods are the same as the
    // identically named methods in that class, but newer
    // functionality will be added there.
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getPageImages();
    QPDF_DLL
    std::vector<QPDFObjectHandle> getPageContents();
    QPDF_DLL
    void addPageContents(QPDFObjectHandle contents, bool first);
    QPDF_DLL
    void rotatePage(int angle, bool relative);
    QPDF_DLL
    void coalesceContentStreams();
    // End legacy page helpers

    // Issue a warning about this object if possible. If the object
    // has a description, a warning will be issued. Otherwise, if
    // throw_if_no_description is true, throw an exception. Otherwise
    // do nothing. Objects read normally from the file have
    // descriptions. See comments on setObjectDescription for
    // additional details.
    QPDF_DLL
    void warnIfPossible(std::string const& warning,
                        bool throw_if_no_description = false);

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
        friend class QPDF_Stream;
        friend class SparseOHArray;
      private:
	static void releaseResolved(QPDFObjectHandle& o)
	{
	    o.releaseResolved();
	}
    };
    friend class ReleaseResolver;

    // Convenience routine: Throws if the assumption is violated. Your
    // code will be better if you call one of the isType methods and
    // handle the case of the type being wrong, but these can be
    // convenient if you have already verified the type.
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

    // The isPageObject method checks the /Type key of the object.
    // This is not completely reliable as there are some otherwise
    // valid files whose /Type is wrong for page objects. qpdf is
    // slightly more accepting but may still return false here when
    // treating the object as a page would work. Use this sparingly.
    QPDF_DLL
    bool isPageObject();
    QPDF_DLL
    bool isPagesObject();
    QPDF_DLL
    void assertPageObject();

    QPDF_DLL
    bool isFormXObject();

    // Indicate if this is an image. If exclude_imagemask is true,
    // don't count image masks as images.
    QPDF_DLL
    bool isImage(bool exclude_imagemask=true);

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

    void typeWarning(char const* expected_type,
                     std::string const& warning);
    void objectWarning(std::string const& warning);
    void assertType(char const* type_name, bool istype);
    void dereference();
    void copyObject(std::set<QPDFObjGen>& visited, bool cross_indirect,
                    bool first_level_only, bool stop_at_streams);
    void shallowCopyInternal(QPDFObjectHandle& oh, bool first_level_only);
    void releaseResolved();
    static void setObjectDescriptionFromInput(
        QPDFObjectHandle, QPDF*, std::string const&,
        PointerHolder<InputSource>, qpdf_offset_t);
    static QPDFObjectHandle parseInternal(
        PointerHolder<InputSource> input,
        std::string const& object_description,
        QPDFTokenizer& tokenizer, bool& empty,
        StringDecrypter* decrypter, QPDF* context,
        bool content_stream);
    void setParsedOffset(qpdf_offset_t offset);
    void parseContentStream_internal(
        std::string const& description,
        ParserCallbacks* callbacks);
    static void parseContentStream_data(
        PointerHolder<Buffer>,
        std::string const& description,
        ParserCallbacks* callbacks,
        QPDF* context);
    std::vector<QPDFObjectHandle> arrayOrStreamToStreamArray(
        std::string const& description, std::string& all_description);
    static void warn(QPDF*, QPDFExc const&);

    bool initialized;

    // Moving members of QPDFObjectHandle into a smart pointer incurs
    // a substantial performance penalty since QPDFObjectHandle
    // objects are copied around so frequently.
    QPDF* qpdf;
    int objid;			// 0 for direct object
    int generation;
    PointerHolder<QPDFObject> obj;
    bool reserved;
};

class QPDFObjectHandle::QPDFDictItems
{
    // This class allows C++-style iteration, including range-for
    // iteration, around dictionaries. You can write

    // for (auto iter: QPDFDictItems(dictionary_obj))
    // {
    //     // iter.first is a string
    //     // iter.second is a QPDFObjectHandle
    // }

    // See examples/pdf-name-number-tree.cc for a demonstration of
    // using this API.

  public:
    QPDF_DLL
    QPDFDictItems(QPDFObjectHandle const& oh);

    class iterator: public std::iterator<
        std::bidirectional_iterator_tag,
        std::pair<std::string, QPDFObjectHandle>>
    {
        friend class QPDFDictItems;
      public:
        QPDF_DLL
        virtual ~iterator() = default;
        QPDF_DLL
        iterator& operator++();
        QPDF_DLL
        iterator operator++(int)
        {
            iterator t = *this;
            ++(*this);
            return t;
        }
        QPDF_DLL
        iterator& operator--();
        QPDF_DLL
        iterator operator--(int)
        {
            iterator t = *this;
            --(*this);
            return t;
        }
        QPDF_DLL
        reference operator*();
        QPDF_DLL
        pointer operator->();
        QPDF_DLL
        bool operator==(iterator const& other) const;
        QPDF_DLL
        bool operator!=(iterator const& other) const
        {
            return ! operator==(other);
        }

      private:
        iterator(QPDFObjectHandle& oh, bool for_begin);
        void updateIValue();

        class Members
        {
            friend class QPDFDictItems::iterator;

          public:
            QPDF_DLL
            ~Members() = default;

          private:
            Members(QPDFObjectHandle& oh, bool for_begin);
            Members() = delete;
            Members(Members const&) = delete;

            QPDFObjectHandle& oh;
            std::set<std::string> keys;
            std::set<std::string>::iterator iter;
            bool is_end;
        };
        PointerHolder<Members> m;
        value_type ivalue;
    };

    QPDF_DLL
    iterator begin();
    QPDF_DLL
    iterator end();

  private:
    QPDFObjectHandle oh;
};

class QPDFObjectHandle::QPDFArrayItems
{
    // This class allows C++-style iteration, including range-for
    // iteration, around arrays. You can write

    // for (auto iter: QPDFArrayItems(array_obj))
    // {
    //     // iter is a QPDFObjectHandle
    // }

    // See examples/pdf-name-number-tree.cc for a demonstration of
    // using this API.

  public:
    QPDF_DLL
    QPDFArrayItems(QPDFObjectHandle const& oh);

    class iterator: public std::iterator<
        std::bidirectional_iterator_tag,
        QPDFObjectHandle>
    {
        friend class QPDFArrayItems;
      public:
        QPDF_DLL
        virtual ~iterator() = default;
        QPDF_DLL
        iterator& operator++();
        QPDF_DLL
        iterator operator++(int)
        {
            iterator t = *this;
            ++(*this);
            return t;
        }
        QPDF_DLL
        iterator& operator--();
        QPDF_DLL
        iterator operator--(int)
        {
            iterator t = *this;
            --(*this);
            return t;
        }
        QPDF_DLL
        reference operator*();
        QPDF_DLL
        pointer operator->();
        QPDF_DLL
        bool operator==(iterator const& other) const;
        QPDF_DLL
        bool operator!=(iterator const& other) const
        {
            return ! operator==(other);
        }

      private:
        iterator(QPDFObjectHandle& oh, bool for_begin);
        void updateIValue();

        class Members
        {
            friend class QPDFArrayItems::iterator;

          public:
            QPDF_DLL
            ~Members() = default;

          private:
            Members(QPDFObjectHandle& oh, bool for_begin);
            Members() = delete;
            Members(Members const&) = delete;

            QPDFObjectHandle& oh;
            int item_number;
            bool is_end;
        };
        PointerHolder<Members> m;
        value_type ivalue;
    };

    QPDF_DLL
    iterator begin();
    QPDF_DLL
    iterator end();

  private:
    QPDFObjectHandle oh;
};


#endif // QPDFOBJECTHANDLE_HH
