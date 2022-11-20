// Copyright (c) 2005-2022 Jay Berkenbilt
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

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/Types.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <qpdf/Buffer.hh>
#include <qpdf/InputSource.hh>
#include <qpdf/JSON.hh>
#include <qpdf/PointerHolder.hh> // unused -- remove in qpdf 12 (see #785)
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFTokenizer.hh>

class Pipeline;
class QPDF;
class QPDF_Array;
class QPDF_Bool;
class QPDF_Dictionary;
class QPDF_InlineImage;
class QPDF_Integer;
class QPDF_Name;
class QPDF_Null;
class QPDF_Operator;
class QPDF_Real;
class QPDF_Reserved;
class QPDF_Stream;
class QPDF_String;
class QPDFObject;
class QPDFTokenizer;
class QPDFExc;
class Pl_QPDFTokenizer;
class QPDFMatrix;
class QPDFParser;

class QPDFObjectHandle
{
    friend class QPDFParser;

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
        virtual ~StreamDataProvider();
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
        virtual void
        provideStreamData(QPDFObjGen const& og, Pipeline* pipeline);
        QPDF_DLL
        virtual bool provideStreamData(
            QPDFObjGen const& og,
            Pipeline* pipeline,
            bool suppress_warnings,
            bool will_retry);
        QPDF_DLL virtual void
        provideStreamData(int objid, int generation, Pipeline* pipeline);
        QPDF_DLL virtual bool provideStreamData(
            int objid,
            int generation,
            Pipeline* pipeline,
            bool suppress_warnings,
            bool will_retry);
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
    // type tt_string or tt_name, you get the canonical, "parsed"
    // representation of the token. For a string, this means that
    // there are no delimiters, and for a name, it means that all
    // escaping (# followed by two hex digits) has been resolved.
    // qpdf's internal representation of a name includes the leading
    // slash. As such, you can't write the value of token.getValue()
    // directly to output that is supposed to be valid PDF syntax. If
    // you want to do that, you need to call writeToken() instead, or
    // you can retrieve the token as it appeared in the input with
    // token.getRawValue(). To construct a new string or name token
    // from a canonical representation, use
    // QPDFTokenizer::Token(QPDFTokenizer::tt_string, "parsed-str") or
    // QPDFTokenizer::Token(QPDFTokenizer::tt_name,
    // "/Canonical-Name"). Tokens created this way won't have a
    // PDF-syntax raw value, but you can still write them with
    // writeToken(). Example:
    // writeToken(QPDFTokenizer::Token(QPDFTokenizer::tt_name, "/text/plain"))
    // would write `/text#2fplain`, and
    // writeToken(QPDFTokenizer::Token(QPDFTokenizer::tt_string, "a\\(b"))
    // would write `(a\(b)`.
    class QPDF_DLL_CLASS TokenFilter
    {
      public:
        QPDF_DLL
        TokenFilter() = default;
        QPDF_DLL
        virtual ~TokenFilter() = default;
        virtual void handleToken(QPDFTokenizer::Token const&) = 0;
        QPDF_DLL
        virtual void handleEOF();

        class PipelineAccessor
        {
            friend class Pl_QPDFTokenizer;

          private:
            static void
            setPipeline(TokenFilter* f, Pipeline* p)
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
        QPDF_DLL_PRIVATE
        void setPipeline(Pipeline*);

        Pipeline* pipeline;
    };

    // This class is used by parse to decrypt strings when reading an
    // object that contains encrypted strings.
    class StringDecrypter
    {
      public:
        QPDF_DLL
        virtual ~StringDecrypter() = default;
        virtual void decryptString(std::string& val) = 0;
    };

    // This class is used by parsePageContents. Callers must
    // instantiate a subclass of this with handlers defined to accept
    // QPDFObjectHandles that are parsed from the stream.
    class QPDF_DLL_CLASS ParserCallbacks
    {
      public:
        QPDF_DLL
        virtual ~ParserCallbacks() = default;
        // One of the handleObject methods must be overridden.
        QPDF_DLL
        virtual void handleObject(QPDFObjectHandle);
        QPDF_DLL
        virtual void
        handleObject(QPDFObjectHandle, size_t offset, size_t length);

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
        Rectangle(double llx, double lly, double urx, double ury) :
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
        Matrix(double a, double b, double c, double d, double e, double f) :
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
    QPDFObjectHandle() = default;
    QPDF_DLL
    QPDFObjectHandle(QPDFObjectHandle const&) = default;
    QPDF_DLL
    QPDFObjectHandle& operator=(QPDFObjectHandle const&) = default;
    QPDF_DLL
    inline bool isInitialized() const;

    // This method returns true if the QPDFObjectHandle objects point
    // to exactly the same underlying object, meaning that changes to
    // one are reflected in the other, or "if you paint one, the other
    // one changes color." This does not perform a structural
    // comparison of the contents of the objects.
    QPDF_DLL
    bool isSameObjectAs(QPDFObjectHandle const&) const;

    // Return type code and type name of underlying object.  These are
    // useful for doing rapid type tests (like switch statements) or
    // for testing and debugging.
    QPDF_DLL
    qpdf_object_type_e getTypeCode();
    QPDF_DLL
    char const* getTypeName();

    // Exactly one of these will return true for any initialized
    // object. Operator and InlineImage are only allowed in content
    // streams.
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
    inline bool isIndirect() const;

    // This returns true for indirect objects from a QPDF that has
    // been destroyed. Trying unparse such an object will throw a
    // logic_error.
    QPDF_DLL
    bool isDestroyed();

    // True for everything except array, dictionary, stream, word, and
    // inline image.
    QPDF_DLL
    bool isScalar();

    // True if the object is a name object representing the provided name.
    QPDF_DLL
    bool isNameAndEquals(std::string const& name);

    // True if the object is a dictionary of the specified type and
    // subtype, if any.
    QPDF_DLL
    bool isDictionaryOfType(
        std::string const& type, std::string const& subtype = "");

    // True if the object is a stream of the specified type and
    // subtype, if any.
    QPDF_DLL
    bool
    isStreamOfType(std::string const& type, std::string const& subtype = "");

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
    // syntax. See also the global `operator ""_qpdf` defined below.
    QPDF_DLL
    static QPDFObjectHandle parse(
        std::string const& object_str,
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
    static QPDFObjectHandle parse(
        QPDF* context,
        std::string const& object_str,
        std::string const& object_description = "");

    // Construct an object as above by reading from the given
    // InputSource at its current position and using the tokenizer you
    // supply.  Indirect objects and encrypted strings are permitted.
    // This method was intended to be called by QPDF for parsing
    // objects that are ready from the object's input stream.
    QPDF_DLL
    static QPDFObjectHandle parse(
        std::shared_ptr<InputSource> input,
        std::string const& object_description,
        QPDFTokenizer&,
        bool& empty,
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
    static void parseContentStream(
        QPDFObjectHandle stream_or_array, ParserCallbacks* callbacks);

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
    void pipeContentStreams(
        Pipeline* p,
        std::string const& description,
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
    QPDF_DLL
    void addTokenFilter(std::shared_ptr<TokenFilter> token_filter);

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
    void addContentTokenFilter(std::shared_ptr<TokenFilter> token_filter);
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
    static QPDFObjectHandle newReal(
        double value, int decimal_places = 0, bool trim_trailing_zeroes = true);
    // Note about name objects: qpdf's internal representation of a
    // PDF name is a sequence of bytes, excluding the NUL character,
    // and starting with a slash. Name objects as represented in the
    // PDF specification can contain characters escaped with #, but
    // such escaping is not of concern when calling QPDFObjectHandle
    // methods not directly relating to parsing. For example,
    // newName("/text/plain").getName() and
    // parse("/text#2fplain").getName() both return "/text/plain",
    // while newName("/text/plain").unparse() and
    // parse("/text#2fplain").unparse() both return "/text#2fplain".
    // When working with the qpdf API for creating, retrieving, and
    // modifying objects, you want to work with the internal,
    // canonical representation. For names containing alphanumeric
    // characters, dashes, and underscores, there is no difference
    // between the two representations. For a lengthy discussion, see
    // https://github.com/qpdf/qpdf/discussions/625.
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
    static QPDFObjectHandle
    newArray(std::vector<QPDFObjectHandle> const& items);
    QPDF_DLL
    static QPDFObjectHandle newArray(Rectangle const&);
    QPDF_DLL
    static QPDFObjectHandle newArray(Matrix const&);
    QPDF_DLL
    static QPDFObjectHandle newArray(QPDFMatrix const&);
    QPDF_DLL
    static QPDFObjectHandle newDictionary();
    QPDF_DLL
    static QPDFObjectHandle
    newDictionary(std::map<std::string, QPDFObjectHandle> const& items);

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

    // Note: new stream creation methods have were added to the QPDF
    // class starting with version 11.2.0. The ones in this class are
    // here for backward compatibility.

    // Create a new stream and associate it with the given qpdf
    // object. A subsequent call must be made to replaceStreamData()
    // to provide data for the stream. The stream's dictionary may be
    // retrieved by calling getDict(), and the resulting dictionary
    // may be modified. Alternatively, you can create a new dictionary
    // and call replaceDict to install it. From QPDF 11.2, you can
    // call QPDF::newStream() instead.
    QPDF_DLL
    static QPDFObjectHandle newStream(QPDF* qpdf);

    // Create a new stream and associate it with the given qpdf
    // object. Use the given buffer as the stream data. The stream
    // dictionary's /Length key will automatically be set to the size
    // of the data buffer. If additional keys are required, the
    // stream's dictionary may be retrieved by calling getDict(), and
    // the resulting dictionary may be modified. This method is just a
    // convenient wrapper around the newStream() and
    // replaceStreamData(). It is a convenience methods for streams
    // that require no parameters beyond the stream length. Note that
    // you don't have to deal with compression yourself if you use
    // QPDFWriter. By default, QPDFWriter will automatically compress
    // uncompressed stream data. Example programs are provided that
    // illustrate this. From QPDF 11.2, you can call QPDF::newStream()
    // instead.
    QPDF_DLL
    static QPDFObjectHandle newStream(QPDF* qpdf, std::shared_ptr<Buffer> data);

    // Create new stream with data from string. This method will
    // create a copy of the data rather than using the user-provided
    // buffer as in the std::shared_ptr<Buffer> version of newStream.
    // From QPDF 11.2, you can call QPDF::newStream() instead.
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
    // this automatically with objects that are read from the input
    // PDF and with objects that are created programmatically and
    // inserted into the QPDF as a new indirect object. Most end user
    // code will not need to call this. If an object has an owning
    // qpdf and object description, it enables qpdf to give warnings
    // with proper context in some cases where it would otherwise
    // raise exceptions. It is okay to add objects without an
    // owning_qpdf to objects that have one, but it is an error to
    // have a QPDF contain objects with owning_qpdf set to something
    // else. To add objects from another qpdf, use copyForeignObject
    // instead.
    QPDF_DLL
    void setObjectDescription(
        QPDF* owning_qpdf, std::string const& object_description);
    QPDF_DLL
    bool hasObjectDescription();

    // Accessor methods
    //
    // (Note: this comment is referenced in qpdf-c.h and the manual.)
    //
    // In PDF files, objects have specific types, but there is nothing
    // that prevents PDF files from containing objects of types that
    // aren't expected by the specification.
    //
    // There are two flavors of accessor methods:
    //
    // * getSomethingValue() returns the value and issues a type
    //   warning if the type is incorrect.
    //
    // * getValueAsSomething() returns false if the value is the wrong
    //   type. Otherwise, it returns true and initializes a reference
    //   of the appropriate type. These methods never issue type
    //   warnings.
    //
    // The getSomethingValue() accessors and some of the other methods
    // expect objects of a particular type. Prior to qpdf 8, calling
    // an accessor on a method of the wrong type, such as trying to
    // get a dictionary key from an array, trying to get the string
    // value of a number, etc., would throw an exception, but since
    // qpdf 8, qpdf issues a warning and recovers using the following
    // behavior:
    //
    // * Requesting a value of the wrong type (int value from string,
    //   array item from a scalar or dictionary, etc.) will return a
    //   zero-like value for that type: false for boolean, 0 for
    //   number, the empty string for string, or the null object for
    //   an object handle.
    //
    // * Accessing an array item that is out of bounds will return a
    //   null object.
    //
    // * Attempts to mutate an object of the wrong type (e.g.,
    //   attempting to add a dictionary key to a scalar or array) will
    //   be ignored.
    //
    // When any of these fallback behaviors are used, qpdf issues a
    // warning. Starting in qpdf 10.5, these warnings have the error
    // code qpdf_e_object. Prior to 10.5, they had the error code
    // qpdf_e_damaged_pdf. If the QPDFObjectHandle is associated with
    // a QPDF object (as is the case for all objects whose origin was
    // a PDF file), the warning is issued using the normal warning
    // mechanism (as described in QPDF.hh), making it possible to
    // suppress or otherwise detect them. If the QPDFObjectHandle is
    // not associated with a QPDF object (meaning it was created
    // programmatically), an exception will be thrown.
    //
    // The way to avoid getting any type warnings or exceptions, even
    // when working with malformed PDF files, is to always check the
    // type of a QPDFObjectHandle before accessing it (for example,
    // make sure that isString() returns true before calling
    // getStringValue()) and to always be sure that any array indices
    // are in bounds.
    //
    // For additional discussion and rationale for this behavior, see
    // the section in the QPDF manual entitled "Object Accessor
    // Methods".

    // Methods for bool objects
    QPDF_DLL
    bool getBoolValue();
    QPDF_DLL
    bool getValueAsBool(bool&);

    // Methods for integer objects. Note: if an integer value is too
    // big (too far away from zero in either direction) to fit in the
    // requested return type, the maximum or minimum value for that
    // return type may be returned. For example, on a system with
    // 32-bit int, a numeric object with a value of 2^40 (or anything
    // too big for 32 bits) will be returned as INT_MAX.
    QPDF_DLL
    long long getIntValue();
    QPDF_DLL
    bool getValueAsInt(long long&);
    QPDF_DLL
    int getIntValueAsInt();
    QPDF_DLL
    bool getValueAsInt(int&);
    QPDF_DLL
    unsigned long long getUIntValue();
    QPDF_DLL
    bool getValueAsUInt(unsigned long long&);
    QPDF_DLL
    unsigned int getUIntValueAsUInt();
    QPDF_DLL
    bool getValueAsUInt(unsigned int&);

    // Methods for real objects
    QPDF_DLL
    std::string getRealValue();
    QPDF_DLL
    bool getValueAsReal(std::string&);

    // Methods that work for both integer and real objects
    QPDF_DLL
    bool isNumber();
    QPDF_DLL
    double getNumericValue();
    QPDF_DLL
    bool getValueAsNumber(double&);

    // Methods for name objects. The returned name value is in qpdf's
    // canonical form with all escaping resolved. See comments for
    // newName() for details.
    QPDF_DLL
    std::string getName();
    QPDF_DLL
    bool getValueAsName(std::string&);

    // Methods for string objects
    QPDF_DLL
    std::string getStringValue();
    QPDF_DLL
    bool getValueAsString(std::string&);

    // If a string starts with the UTF-16 marker, it is converted from
    // UTF-16 to UTF-8. Otherwise, it is treated as a string encoded
    // with PDF Doc Encoding. PDF Doc Encoding is identical to
    // ISO-8859-1 except in the range from 0200 through 0240, where
    // there is a mapping of characters to Unicode. QPDF versions
    // prior to version 8.0.0 erroneously left characters in that range
    // unmapped.
    QPDF_DLL
    std::string getUTF8Value();
    QPDF_DLL
    bool getValueAsUTF8(std::string&);

    // Methods for content stream objects
    QPDF_DLL
    std::string getOperatorValue();
    QPDF_DLL
    bool getValueAsOperator(std::string&);
    QPDF_DLL
    std::string getInlineImageValue();
    QPDF_DLL
    bool getValueAsInlineImage(std::string&);

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
    // If the array is an array of four numeric values, return as a
    // rectangle. Otherwise, return the rectangle [0, 0, 0, 0]
    QPDF_DLL
    Rectangle getArrayAsRectangle();
    QPDF_DLL
    bool isMatrix();
    // If the array is an array of six numeric values, return as a
    // matrix. Otherwise, return the matrix [1, 0, 0, 1, 0, 0]
    QPDF_DLL
    Matrix getArrayAsMatrix();

    // Methods for dictionary objects. In all dictionary methods, keys
    // are specified/represented as canonical name strings starting
    // with a leading slash and not containing any PDF syntax
    // escaping. See comments for getName() for details.

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

    // Return true if key is present.  Keys with null values are treated as if
    // they are not present.  This is as per the PDF spec.
    QPDF_DLL
    bool hasKey(std::string const&);
    // Return the value for the key.  If the key is not present, null is
    // returned.
    QPDF_DLL
    QPDFObjectHandle getKey(std::string const&);
    // If the object is null, return null. Otherwise, call getKey().
    // This makes it easier to access lower-level dictionaries, as in
    // auto font = page.getKeyIfDict("/Resources").getKeyIfDict("/Font");
    QPDF_DLL
    QPDFObjectHandle getKeyIfDict(std::string const&);
    // Return all keys.  Keys with null values are treated as if
    // they are not present.  This is as per the PDF spec.
    QPDF_DLL
    std::set<std::string> getKeys();
    // Return dictionary as a map.  Entries with null values are included.
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getDictAsMap();

    // Methods for name and array objects. The name value is in qpdf's
    // canonical form with all escaping resolved. See comments for
    // newName() for details.
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
        std::map<std::string, std::map<std::string, std::string>>* conflicts =
            nullptr);

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
        std::set<std::string>* resource_names = nullptr);

    // A QPDFObjectHandle has an owning QPDF if it is associated with
    // ("owned by") a specific QPDF object. Indirect objects always
    // have an owning QPDF. Direct objects that are read from the
    // input source will also have an owning QPDF. Programmatically
    // created objects will only have one if setObjectDescription was
    // called.
    //
    // When the QPDF object that owns an object is destroyed, the
    // object is changed into a null, and its owner is cleared.
    // Therefore you should not retain the value of an owning QPDF
    // beyond the life of the QPDF. If in doubt, ask for it each time
    // you need it.

    // getOwningQPDF returns a pointer to the owning QPDF is the
    // object has one. Otherwise, it returns a null pointer. Use this
    // when you are able to handle the case of an object that doesn't
    // have an owning QPDF.
    QPDF_DLL
    QPDF* getOwningQPDF() const;
    // getQPDF, new in qpdf 11, returns a reference owning QPDF. If
    // there is none, it throws a runtime_error. Use this when you
    // know the object has to have an owning QPDF, such as when it's a
    // known indirect object. Since streams are always indirect
    // objects, this method can be used safely for streams. If
    // error_msg is specified, it will be used at the contents of the
    // runtime_error if there is now owner.
    QPDF_DLL
    QPDF& getQPDF(std::string const& error_msg = "") const;

    // Create a shallow copy of an object as a direct object, but do not
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

    // Mutator methods.

    // Since qpdf 11: for mutators that may add or remove an item,
    // there are additional versions whose names contain "AndGet" that
    // return the added or removed item. For example:
    //
    //   auto new_dict = dict.replaceKeyAndGetNew(
    //       "/New", QPDFObjectHandle::newDictionary());
    //
    //   auto old_value = dict.replaceKeyAndGetOld(
    //       "/New", "(something)"_qpdf);

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
    void makeDirect(bool allow_streams = false);

    // Mutator methods for array objects
    QPDF_DLL
    void setArrayItem(int, QPDFObjectHandle const&);
    QPDF_DLL
    void setArrayFromVector(std::vector<QPDFObjectHandle> const& items);
    // Insert an item before the item at the given position ("at") so
    // that it has that position after insertion. If "at" is equal to
    // the size of the array, insert the item at the end.
    QPDF_DLL
    void insertItem(int at, QPDFObjectHandle const& item);
    // Like insertItem but return the item that was inserted.
    QPDF_DLL
    QPDFObjectHandle insertItemAndGetNew(int at, QPDFObjectHandle const& item);
    // Append an item to an array.
    QPDF_DLL
    void appendItem(QPDFObjectHandle const& item);
    // Append an item, and return the newly added item.
    QPDF_DLL
    QPDFObjectHandle appendItemAndGetNew(QPDFObjectHandle const& item);
    // Remove the item at that position, reducing the size of the
    // array by one.
    QPDF_DLL
    void eraseItem(int at);
    // Erase and item and return the item that was removed.
    QPDF_DLL
    QPDFObjectHandle eraseItemAndGetOld(int at);

    // Mutator methods for dictionary objects

    // Replace value of key, adding it if it does not exist. If value
    // is null, remove the key.
    QPDF_DLL
    void replaceKey(std::string const& key, QPDFObjectHandle const& value);
    // Replace value of key and return the value.
    QPDF_DLL
    QPDFObjectHandle
    replaceKeyAndGetNew(std::string const& key, QPDFObjectHandle const& value);
    // Replace value of key and return the old value, or null if the
    // key was previously not present.
    QPDF_DLL
    QPDFObjectHandle
    replaceKeyAndGetOld(std::string const& key, QPDFObjectHandle const& value);
    // Remove key, doing nothing if key does not exist.
    QPDF_DLL
    void removeKey(std::string const& key);
    // Remove key and return the old value. If the old value didn't
    // exist, return a null object.
    QPDF_DLL
    QPDFObjectHandle removeKeyAndGetOld(std::string const& key);

    // ABI: Remove in qpdf 12
    [[deprecated("use replaceKey -- it does the same thing")]] QPDF_DLL void
    replaceOrRemoveKey(std::string const& key, QPDFObjectHandle const&);

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
    std::shared_ptr<Buffer>
    getStreamData(qpdf_stream_decode_level_e level = qpdf_dl_generalized);

    // Returns unfiltered (raw) stream data.
    QPDF_DLL
    std::shared_ptr<Buffer> getRawStreamData();

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
    bool pipeStreamData(
        Pipeline*,
        bool* filtering_attempted,
        int encode_flags,
        qpdf_stream_decode_level_e decode_level,
        bool suppress_warnings = false,
        bool will_retry = false);

    // Legacy version. Return value is whether filtering was
    // attempted. There is no way to determine success if filtering
    // was not attempted.
    QPDF_DLL
    bool pipeStreamData(
        Pipeline*,
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
    bool pipeStreamData(Pipeline*, bool filter, bool normalize, bool compress);

    // Replace a stream's dictionary.  The new dictionary must be
    // consistent with the stream's data.  This is most appropriately
    // used when creating streams from scratch that will use a stream
    // data provider and therefore start with an empty dictionary.  It
    // may be more convenient in this case than calling getDict and
    // modifying it for each key.  The pdf-create example does this.
    QPDF_DLL
    void replaceDict(QPDFObjectHandle const&);

    // REPLACING STREAM DATA

    // Note about all replaceStreamData methods: whatever values are
    // passed as filter and decode_parms will overwrite /Filter and
    // /DecodeParms in the stream. Passing a null object
    // (QPDFObjectHandle::newNull()) will remove those values from the
    // stream dictionary. From qpdf 11, passing an *uninitialized*
    // QPDFObjectHandle (QPDFObjectHandle()) will leave any existing
    // values untouched.

    // Replace this stream's stream data with the given data buffer.
    // The stream's /Length key is replaced with the length of the
    // data buffer. The stream is interpreted as if the data read from
    // the file, after any decryption filters have been applied, is as
    // presented.
    QPDF_DLL
    void replaceStreamData(
        std::shared_ptr<Buffer> data,
        QPDFObjectHandle const& filter,
        QPDFObjectHandle const& decode_parms);

    // Replace the stream's stream data with the given string.
    // This method will create a copy of the data rather than using
    // the user-provided buffer as in the std::shared_ptr<Buffer> version
    // of replaceStreamData.
    QPDF_DLL
    void replaceStreamData(
        std::string const& data,
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
    void replaceStreamData(
        std::shared_ptr<StreamDataProvider> provider,
        QPDFObjectHandle const& filter,
        QPDFObjectHandle const& decode_parms);

    // Starting in qpdf 10.2, you can use C++-11 function objects
    // instead of StreamDataProvider.

    // The provider should write the stream data to the pipeline. For
    // a one-liner to replace stream data with the contents of a file,
    // pass QUtil::file_provider(filename) as provider.
    QPDF_DLL
    void replaceStreamData(
        std::function<void(Pipeline*)> provider,
        QPDFObjectHandle const& filter,
        QPDFObjectHandle const& decode_parms);
    // The provider should write the stream data to the pipeline,
    // returning true if it succeeded without errors.
    QPDF_DLL
    void replaceStreamData(
        std::function<bool(Pipeline*, bool suppress_warnings, bool will_retry)>
            provider,
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
    inline int getObjectID() const;
    QPDF_DLL
    inline int getGeneration() const;

    QPDF_DLL
    std::string unparse();
    QPDF_DLL
    std::string unparseResolved();
    // For strings only, force binary representation. Otherwise, same
    // as unparse.
    QPDF_DLL
    std::string unparseBinary();

    // Return encoded as JSON. The constant JSON::LATEST can be used
    // to specify the latest available JSON version. The JSON is
    // generated as follows:
    // * Arrays, dictionaries, booleans, nulls, integers, and real
    //   numbers are represented by their native JSON types.
    // * Names are encoded as strings representing the canonical
    //   representation (after parsing #xx) and preceded by a slash,
    //   just as unparse() returns. For example, the JSON for the
    //   PDF-syntax name /Text#2fPlain would be "/Text/Plain".
    // * Indirect references are encoded as strings containing "obj gen R"
    // * Strings
    //   * JSON v1: Strings are encoded as UTF-8 strings with
    //     unrepresentable binary characters encoded as \uHHHH.
    //     Characters in PDF Doc encoding that don't have
    //     bidirectional unicode mappings are not reversible. There is
    //     no way to tell the difference between a string that looks
    //     like a name or indirect object from an actual name or
    //     indirect object.
    //   * JSON v2:
    //     * Unicode strings and strings encoded with PDF Doc encoding
    //       that can be bidrectionally mapped two Unicode (which is
    //       all strings without undefined characters) are represented
    //       as "u:" followed by the UTF-8 encoded string. Example:
    //       "u:potato".
    //     * All other strings are represented as "b:" followed by a
    //       hexadecimal encoding of the string. Example: "b:0102cacb"
    // * Streams
    //   * JSON v1: Only the stream's dictionary is encoded. There is
    //     no way tell a stream from a dictionary other than context.
    //   * JSON v2: A stream is encoded as {"dict": {...}} with the
    //     value being the encoding of the stream's dictionary. Since
    //     "dict" does not otherwise represent anything, this is
    //     unambiguous. The getStreamJSON() call can be used to add
    //     encoding of the stream's data.
    // * Object types that are only valid in content streams (inline
    //   image, operator) are serialized as "null". Attempting to
    //   serialize a "reserved" object is an error.
    // If dereference_indirect is true and this is an indirect object,
    // show the actual contents of the object. The effect of
    // dereference_indirect applies only to this object. It is not
    // recursive.
    QPDF_DLL
    JSON getJSON(int json_version, bool dereference_indirect = false);

    // Deprecated version uses v1 for backward compatibility.
    // ABI: remove for qpdf 12
    [[deprecated("Use getJSON(int version)")]] QPDF_DLL JSON
    getJSON(bool dereference_indirect = false);

    // This method can be called on a stream to get a more extended
    // JSON representation of the stream that includes the stream's
    // data. The JSON object returned is always a dictionary whose
    // "dict" key is an encoding of the stream's dictionary. The
    // representation of the data is determined by the json_data
    // field.
    //
    // The json_data field may have the value qpdf_sj_none,
    // qpdf_sj_inline, or qpdf_sj_file.
    //
    // If json_data is qpdf_sj_none, stream data is not represented.
    //
    // If json_data is qpdf_sj_inline or qpdf_sj_file, then stream
    // data is filtered or not based on the value of decode_level,
    // which has the same meaning as with pipeStreamData.
    //
    // If json_data is qpdf_sj_inline, the base64-encoded stream data
    // is included in the "data" field of the dictionary that is
    // returned.
    //
    // If json_data is qpdf_sj_file, then the Pipeline ("p") and
    // data_filename argument must be supplied. The value of
    // data_filename is stored in the resulting json in the "datafile"
    // key but is not otherwise use. The stream data itself (raw or
    // filtered depending on decode level), is written to the pipeline
    // via pipeStreamData().
    //
    // NOTE: When json_data is qpdf_sj_inline, the QPDF object from
    // which the stream originates must remain valid until after the
    // JSON object is written.
    QPDF_DLL
    JSON getStreamJSON(
        int json_version,
        qpdf_json_stream_data_e json_data,
        qpdf_stream_decode_level_e decode_level,
        Pipeline* p,
        std::string const& data_filename);

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
    // has a description, a warning will be issued using the owning
    // QPDF as context. Otherwise, a message will be written to the
    // default logger's error stream, which is standard error if not
    // overridden. Objects read normally from the file have
    // descriptions. See comments on setObjectDescription for
    // additional details.
    QPDF_DLL
    void warnIfPossible(std::string const& warning);

    // Initializers for objects.  This Factory class gives the QPDF
    // class specific permission to call factory methods without
    // making it a friend of the whole QPDFObjectHandle class.
    class Factory
    {
        friend class QPDF;

      private:
        static QPDFObjectHandle
        newIndirect(std::shared_ptr<QPDFObject> const& obj)
        {
            return QPDFObjectHandle(obj);
        }
    };
    friend class Factory;

    // Accessor for raw underlying object -- only QPDF is allowed to
    // call this.
    class ObjAccessor
    {
        friend class QPDF;

      private:
        static std::shared_ptr<QPDFObject>
        getObject(QPDFObjectHandle& o)
        {
            if (!o.dereference()) {
                throw std::logic_error("attempted to dereference an"
                                       " uninitialized QPDFObjectHandle");
            };
            return o.obj;
        }
        static QPDF_Array*
        asArray(QPDFObjectHandle& oh)
        {
            return oh.asArray();
        }
        static QPDF_Stream*
        asStream(QPDFObjectHandle& oh)
        {
            return oh.asStream();
        }
    };
    friend class ObjAccessor;

    // Provide access to specific classes for recursive
    // disconnected().
    class DisconnectAccess
    {
        friend class QPDF_Dictionary;
        friend class QPDF_Stream;
        friend class SparseOHArray;

      private:
        static void
        disconnect(QPDFObjectHandle& o)
        {
            o.disconnect();
        }
    };
    friend class Resetter;

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
    bool isImage(bool exclude_imagemask = true);

  private:
    QPDFObjectHandle(std::shared_ptr<QPDFObject> const& obj) :
        obj(obj)
    {
    }

    QPDF_Array* asArray();
    QPDF_Bool* asBool();
    QPDF_Dictionary* asDictionary();
    QPDF_InlineImage* asInlineImage();
    QPDF_Integer* asInteger();
    QPDF_Name* asName();
    QPDF_Null* asNull();
    QPDF_Operator* asOperator();
    QPDF_Real* asReal();
    QPDF_Reserved* asReserved();
    QPDF_Stream* asStream();
    QPDF_Stream* asStreamWithAssert();
    QPDF_String* asString();

    void typeWarning(char const* expected_type, std::string const& warning);
    void objectWarning(std::string const& warning);
    void assertType(char const* type_name, bool istype);
    bool dereference();
    void makeDirect(std::set<QPDFObjGen>& visited, bool stop_at_streams);
    void disconnect();
    void setParsedOffset(qpdf_offset_t offset);
    void parseContentStream_internal(
        std::string const& description, ParserCallbacks* callbacks);
    static void parseContentStream_data(
        std::shared_ptr<Buffer>,
        std::string const& description,
        ParserCallbacks* callbacks,
        QPDF* context);
    std::vector<QPDFObjectHandle> arrayOrStreamToStreamArray(
        std::string const& description, std::string& all_description);
    static void warn(QPDF*, QPDFExc const&);
    void checkOwnership(QPDFObjectHandle const&) const;

    // Moving members of QPDFObjectHandle into a smart pointer incurs
    // a substantial performance penalty since QPDFObjectHandle
    // objects are copied around so frequently.
    std::shared_ptr<QPDFObject> obj;
};

#ifndef QPDF_NO_QPDF_STRING
// This is short for QPDFObjectHandle::parse, so you can do

// auto oh = "<< /Key (value) >>"_qpdf;

// If this is causing problems in your code, define
// QPDF_NO_QPDF_STRING to prevent the declaration from being here.

/* clang-format off */
// Disable formatting for this declaration: emacs font-lock in cc-mode
// (as of 28.1) treats the rest of the file as a string if
// clang-format removes the space after "operator", and as of
// clang-format 15, there's no way to prevent it from doing so.
QPDF_DLL
QPDFObjectHandle operator ""_qpdf(char const* v, size_t len);
/* clang-format on */

#endif // QPDF_NO_QPDF_STRING

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

    class iterator
    {
        friend class QPDFDictItems;

      public:
        typedef std::pair<std::string, QPDFObjectHandle> T;
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = long;
        using pointer = T*;
        using reference = T&;

        QPDF_DLL
        virtual ~iterator() = default;
        QPDF_DLL
        iterator& operator++();
        QPDF_DLL
        iterator
        operator++(int)
        {
            iterator t = *this;
            ++(*this);
            return t;
        }
        QPDF_DLL
        iterator& operator--();
        QPDF_DLL
        iterator
        operator--(int)
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
        bool
        operator!=(iterator const& other) const
        {
            return !operator==(other);
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
        std::shared_ptr<Members> m;
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

    class iterator
    {
        friend class QPDFArrayItems;

      public:
        typedef QPDFObjectHandle T;
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = long;
        using pointer = T*;
        using reference = T&;

        QPDF_DLL
        virtual ~iterator() = default;
        QPDF_DLL
        iterator& operator++();
        QPDF_DLL
        iterator
        operator++(int)
        {
            iterator t = *this;
            ++(*this);
            return t;
        }
        QPDF_DLL
        iterator& operator--();
        QPDF_DLL
        iterator
        operator--(int)
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
        bool
        operator!=(iterator const& other) const
        {
            return !operator==(other);
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
        std::shared_ptr<Members> m;
        value_type ivalue;
    };

    QPDF_DLL
    iterator begin();
    QPDF_DLL
    iterator end();

  private:
    QPDFObjectHandle oh;
};

inline int
QPDFObjectHandle::getObjectID() const
{
    return getObjGen().getObj();
}

inline int
QPDFObjectHandle::getGeneration() const
{
    return getObjGen().getGen();
}

inline bool
QPDFObjectHandle::isIndirect() const
{
    return (obj != nullptr) && (getObjectID() != 0);
}

inline bool
QPDFObjectHandle::isInitialized() const
{
    return obj != nullptr;
}

#endif // QPDFOBJECTHANDLE_HH
