// Copyright (c) 2005-2021 Jay Berkenbilt
// Copyright (c) 2022-2025 Jay Berkenbilt and Manfred Holger
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under
// the License.
//
// Versions of qpdf prior to version 7 were released under the terms of version 2.0 of the Artistic
// License. At your option, you may continue to consider qpdf to be licensed under those terms.
// Please see the manual for additional information.

#ifndef QPDF_HH
#define QPDF_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>

#include <cstdio>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <qpdf/Buffer.hh>
#include <qpdf/InputSource.hh>
#include <qpdf/PDFVersion.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFStreamFilter.hh>
#include <qpdf/QPDFTokenizer.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFXRefEntry.hh>

namespace qpdf::is
{
    class OffsetBuffer;
}

class QPDF_Stream;
class BitStream;
class BitWriter;
class BufferInputSource;
class QPDFLogger;
class QPDFParser;

class QPDF
{
  public:
    // Get the current version of the QPDF software. See also qpdf/DLL.h
    QPDF_DLL
    static std::string const& QPDFVersion();

    QPDF_DLL
    QPDF();
    QPDF_DLL
    ~QPDF();

    QPDF_DLL
    static std::shared_ptr<QPDF> create();

    // Associate a file with a QPDF object and do initial parsing of the file.  PDF objects are not
    // read until they are needed.  A QPDF object may be associated with only one file in its
    // lifetime.  This method must be called before any methods that potentially ask for information
    // about the PDF file are called. Prior to calling this, the only methods that are allowed are
    // those that set parameters.  If the input file is not encrypted, either a null password or an
    // empty password can be used.  If the file is encrypted, either the user password or the owner
    // password may be supplied. The method setPasswordIsHexKey may be called prior to calling this
    // method or any of the other process methods to force the password to be interpreted as a raw
    // encryption key. See comments on setPasswordIsHexKey for more information.
    QPDF_DLL
    void processFile(char const* filename, char const* password = nullptr);

    // Parse a PDF from a stdio FILE*.  The FILE must be open in binary mode and must be seekable.
    // It may be open read only. This works exactly like processFile except that the PDF file is
    // read from an already opened FILE*.  If close_file is true, the file will be closed at the
    // end.  Otherwise, the caller is responsible for closing the file.
    QPDF_DLL
    void processFile(
        char const* description, FILE* file, bool close_file, char const* password = nullptr);

    // Parse a PDF file loaded into a memory buffer.  This works exactly like processFile except
    // that the PDF file is in memory instead of on disk.  The description appears in any warning or
    // error message in place of the file name.
    QPDF_DLL
    void processMemoryFile(
        char const* description, char const* buf, size_t length, char const* password = nullptr);

    // Parse a PDF file loaded from a custom InputSource.  If you have your own method of retrieving
    // a PDF file, you can subclass InputSource and use this method.
    QPDF_DLL
    void processInputSource(std::shared_ptr<InputSource>, char const* password = nullptr);

    // Create a PDF from an input source that contains JSON as written by writeJSON (or qpdf
    // --json-output, version 2 or higher). The JSON must be a complete representation of a PDF. See
    // "qpdf JSON" in the manual for details. The input JSON may be arbitrarily large. QPDF does not
    // load stream data into memory for more than one stream at a time, even if the stream data is
    // specified inline.
    QPDF_DLL
    void createFromJSON(std::string const& json_file);
    QPDF_DLL
    void createFromJSON(std::shared_ptr<InputSource>);

    // Update a PDF from an input source that contains JSON in the same format as is written by
    // writeJSON (or qpdf --json-output, version 2 or higher). Objects in the PDF and not in the
    // JSON are not modified. See "qpdf JSON" in the manual for details. As with createFromJSON, the
    // input JSON may be arbitrarily large.
    QPDF_DLL
    void updateFromJSON(std::string const& json_file);
    QPDF_DLL
    void updateFromJSON(std::shared_ptr<InputSource>);

    // Write qpdf JSON format to the pipeline "p". The only supported version is 2. The finish()
    // method is not called on the pipeline.
    //
    // The decode_level parameter controls which streams are uncompressed in the JSON. Use
    // qpdf_dl_none to preserve all stream data exactly as it appears in the input. The possible
    // values for json_stream_data can be found in qpdf/Constants.h and correspond to the
    // --json-stream-data command-line argument. If json_stream_data is qpdf_sj_file, file_prefix
    // must be specified. Each stream will be written to a file whose path is constructed by
    // appending "-nnn" to file_prefix, where "nnn" is the object number (not zero-filled). If
    // wanted_objects is empty, write all objects. Otherwise, write only objects whose keys are in
    // wanted_objects. Keys may be either "trailer" or of the form "obj:n n R". Invalid keys are
    // ignored. This corresponds to the --json-object command-line argument.
    //
    // QPDF is efficient with regard to memory when writing, allowing you to write arbitrarily large
    // PDF files to a pipeline. You can use a pipeline like Pl_Buffer or Pl_String to capture the
    // JSON output in memory, but do so with caution as this will allocate enough memory to hold the
    // entire PDF file.
    QPDF_DLL
    void writeJSON(
        int version,
        Pipeline* p,
        qpdf_stream_decode_level_e decode_level,
        qpdf_json_stream_data_e json_stream_data,
        std::string const& file_prefix,
        std::set<std::string> wanted_objects);

    // This version of writeJSON enables writing only the "qpdf" key of an in-progress dictionary.
    // If the value of "complete" is true, a complete JSON object containing only the "qpdf" key is
    // written to the pipeline. If the value of "complete" is false, the "qpdf" key and its value
    // are written to the pipeline assuming that a dictionary is already open. The parameter
    // first_key indicates whether this is the first key in an in-progress dictionary. It will be
    // set to false by writeJSON. The "qpdf" key and value are written as if at depth 1 in a
    // prettified JSON output. Remaining arguments are the same as the above version.
    QPDF_DLL
    void writeJSON(
        int version,
        Pipeline* p,
        bool complete,
        bool& first_key,
        qpdf_stream_decode_level_e decode_level,
        qpdf_json_stream_data_e json_stream_data,
        std::string const& file_prefix,
        std::set<std::string> wanted_objects);

    // Close or otherwise release the input source. Once this has been called, no other methods of
    // qpdf can be called safely except for getWarnings and anyWarnings(). After this has been
    // called, it is safe to perform operations on the input file such as deleting or renaming it.
    QPDF_DLL
    void closeInputSource();

    // For certain forensic or investigatory purposes, it may sometimes be useful to specify the
    // encryption key directly, even though regular PDF applications do not provide a way to do
    // this. Calling setPasswordIsHexKey(true) before calling any of the process methods will bypass
    // the normal encryption key computation or recovery mechanisms and interpret the bytes in the
    // password as a hex-encoded encryption key. Note that we hex-encode the key because it may
    // contain null bytes and therefore can't be represented in a char const*.
    QPDF_DLL
    void setPasswordIsHexKey(bool);

    // Create a QPDF object for an empty PDF.  This PDF has no pages or objects other than a minimal
    // trailer, a document catalog, and a /Pages tree containing zero pages.  Pages and other
    // objects can be added to the file in the normal way, and the trailer and document catalog can
    // be mutated.  Calling this method is equivalent to calling processFile on an equivalent PDF
    // file.  See the pdf-create.cc example for a demonstration of how to use this method to create
    // a PDF file from scratch.
    QPDF_DLL
    void emptyPDF();

    // From 10.1: register a new filter implementation for a specific stream filter. You can add
    // your own implementations for new filter types or override existing ones provided by the
    // library. Registered stream filters are used for decoding only as you can override encoding
    // with stream data providers. For example, you could use this method to add support for one of
    // the other filter types by using additional third-party libraries that qpdf does not presently
    // use. The standard filters are implemented using QPDFStreamFilter classes.
    QPDF_DLL
    static void registerStreamFilter(
        std::string const& filter_name, std::function<std::shared_ptr<QPDFStreamFilter>()> factory);

    // Parameter settings

    // To capture or redirect output, configure the logger returned by getLogger(). By default, all
    // QPDF and QPDFJob objects share the global logger. If you need a private logger for some
    // reason, pass a new one to setLogger(). See comments in QPDFLogger.hh for details on
    // configuring the logger.
    //
    // Note that no normal QPDF operations generate output to standard output, so for applications
    // that just wish to avoid creating output for warnings and don't call any check functions,
    // calling setSuppressWarnings(true) is sufficient.
    QPDF_DLL
    std::shared_ptr<QPDFLogger> getLogger();
    QPDF_DLL
    void setLogger(std::shared_ptr<QPDFLogger>);

    // This deprecated method is the old way to capture output, but it didn't capture all output.
    // See comments above for getLogger and setLogger. This will be removed in QPDF 12. For now, it
    // configures a private logger, separating this object from the default logger, and calls
    // setOutputStreams on that logger. See QPDFLogger.hh for additional details.
    [[deprecated("configure logger from getLogger() or call setLogger()")]] QPDF_DLL void
    setOutputStreams(std::ostream* out_stream, std::ostream* err_stream);

    // If true, ignore any cross-reference streams in a hybrid file (one that contains both
    // cross-reference streams and cross-reference tables).  This can be useful for testing to
    // ensure that a hybrid file would work with an older reader.
    QPDF_DLL
    void setIgnoreXRefStreams(bool);

    // By default, any warnings are issued to std::cerr or the error stream specified in a call to
    // setOutputStreams as they are encountered.  If this method is called with a true value,
    // reporting of warnings is suppressed.  You may still retrieve warnings by calling getWarnings.
    QPDF_DLL
    void setSuppressWarnings(bool);

    // Set the maximum number of warnings. A QPDFExc is thrown if the limit is exceeded.
    QPDF_DLL
    void setMaxWarnings(size_t);

    // By default, QPDF will try to recover if it finds certain types of errors in PDF files.  If
    // turned off, it will throw an exception on the first such problem it finds without attempting
    // recovery.
    QPDF_DLL
    void setAttemptRecovery(bool);

    // Tell other QPDF objects that streams copied from this QPDF need to be fully copied when
    // copyForeignObject is called on them. Calling setIgnoreXRefStreams(true) on a QPDF object
    // makes it possible for the object and its input source to disappear before streams copied from
    // it are written with the destination QPDF object. Confused? Ordinarily, if you are going to
    // copy objects from a source QPDF object to a destination QPDF object using copyForeignObject
    // or addPage, the source object's input source must stick around until after the destination
    // PDF is written. If you call this method on the source QPDF object, it sends a signal to the
    // destination object that it must fully copy the stream data when copyForeignObject. It will do
    // this by making a copy in RAM. Ordinarily the stream data is copied lazily to avoid
    // unnecessary duplication of the stream data. Note that the stream data is copied into RAM only
    // once regardless of how many objects the stream is copied into. The result is that, if you
    // called setImmediateCopyFrom(true) on a given QPDF object prior to copying any of its streams,
    // you do not need to keep it or its input source around after copying its objects to another
    // QPDF. This is true even if the source streams use StreamDataProvider. Note that this method
    // is called on the QPDF object you are copying FROM, not the one you are copying to. The
    // reasoning for this is that there's no reason a given QPDF may not get objects copied to it
    // from a variety of other objects, some transient and some not. Since what's relevant is
    // whether the source QPDF is transient, the method must be called on the source QPDF, not the
    // destination one. This method will make a copy of the stream in RAM, so be sure you have
    // enough memory to simultaneously hold all the streams you're copying.
    QPDF_DLL
    void setImmediateCopyFrom(bool);

    // Other public methods

    // Return the list of warnings that have been issued so far and clear the list.  This method may
    // be called even if processFile throws an exception.  Note that if setSuppressWarnings was not
    // called or was called with a false value, any warnings retrieved here will have already been
    // output.
    QPDF_DLL
    std::vector<QPDFExc> getWarnings();

    // Indicate whether any warnings have been issued so far. Does not clear the list of warnings.
    QPDF_DLL
    bool anyWarnings() const;

    // Indicate the number of warnings that have been issued since the last call to getWarnings.
    // Does not clear the list of warnings.
    QPDF_DLL
    size_t numWarnings() const;

    // Return an application-scoped unique ID for this QPDF object. This is not a globally unique
    // ID. It is constructed using a timestamp and a random number and is intended to be unique
    // among QPDF objects that are created by a single run of an application. While it's very likely
    // that these are actually globally unique, it is not recommended to use them for long-term
    // purposes.
    QPDF_DLL
    unsigned long long getUniqueId() const;

    // Issue a warning on behalf of this QPDF object. It will be emitted with other warnings,
    // following warning suppression rules, and it will be available with getWarnings().
    QPDF_DLL
    void warn(QPDFExc const& e);
    // Same as above but creates the QPDFExc object using the arguments passed to warn. The filename
    // argument to QPDFExc is omitted. This method uses the filename associated with the QPDF
    // object.
    QPDF_DLL
    void warn(
        qpdf_error_code_e error_code,
        std::string const& object,
        qpdf_offset_t offset,
        std::string const& message);

    // Return the filename associated with the QPDF object.
    QPDF_DLL
    std::string getFilename() const;
    // Return PDF Version and extension level together as a PDFVersion object
    QPDF_DLL
    PDFVersion getVersionAsPDFVersion();
    // Return just the PDF version from the file
    QPDF_DLL
    std::string getPDFVersion() const;
    QPDF_DLL
    int getExtensionLevel();
    QPDF_DLL
    QPDFObjectHandle getTrailer();
    QPDF_DLL
    QPDFObjectHandle getRoot();
    QPDF_DLL
    std::map<QPDFObjGen, QPDFXRefEntry> getXRefTable();

    // Public factory methods

    // Create a new stream.  A subsequent call must be made to replaceStreamData() to provide data
    // for the stream.  The stream's dictionary may be retrieved by calling getDict(), and the
    // resulting dictionary may be modified.  Alternatively, you can create a new dictionary and
    // call replaceDict to install it.
    QPDF_DLL
    QPDFObjectHandle newStream();

    // Create a new stream.  Use the given buffer as the stream data.  The stream dictionary's
    // /Length key will automatically be set to the size of the data buffer.  If additional keys are
    // required, the stream's dictionary may be retrieved by calling getDict(), and the resulting
    // dictionary may be modified.  This method is just a convenient wrapper around the newStream()
    // and replaceStreamData().  It is a convenience methods for streams that require no parameters
    // beyond the stream length. Note that you don't have to deal with compression yourself if you
    // use QPDFWriter.  By default, QPDFWriter will automatically compress uncompressed stream data.
    // Example programs are provided that illustrate this.
    QPDF_DLL
    QPDFObjectHandle newStream(std::shared_ptr<Buffer> data);

    // Create new stream with data from string.  This method will create a copy of the data rather
    // than using the user-provided buffer as in the std::shared_ptr<Buffer> version of newStream.
    QPDF_DLL
    QPDFObjectHandle newStream(std::string const& data);

    // A reserved object is a special sentinel used for qpdf to reserve a spot for an object that is
    // going to be added to the QPDF object.  Normally you don't have to use this type since you can
    // just call QPDF::makeIndirectObject.  However, in some cases, if you have to create objects
    // with circular references, you may need to create a reserved object so that you can have a
    // reference to it and then replace the object later.  Reserved objects have the special
    // property that they can't be resolved to direct objects.  This makes it possible to replace a
    // reserved object with a new object while preserving existing references to them.  When you are
    // ready to replace a reserved object with its replacement, use QPDF::replaceReserved for this
    // purpose rather than the more general QPDF::replaceObject.  It is an error to try to write a
    // QPDF with QPDFWriter if it has any reserved objects in it.
    QPDF_DLL
    QPDFObjectHandle newReserved();
    QPDF_DLL
    QPDFObjectHandle newIndirectNull();

    // Install this object handle as an indirect object and return an indirect reference to it.
    QPDF_DLL
    QPDFObjectHandle makeIndirectObject(QPDFObjectHandle);

    // Retrieve an object by object ID and generation. Returns an indirect reference to it. The
    // getObject() methods were added for qpdf 11.
    QPDF_DLL
    QPDFObjectHandle getObject(QPDFObjGen);
    QPDF_DLL
    QPDFObjectHandle getObject(int objid, int generation);
    // These are older methods, but there is no intention to deprecate
    // them.
    QPDF_DLL
    QPDFObjectHandle getObjectByObjGen(QPDFObjGen);
    QPDF_DLL
    QPDFObjectHandle getObjectByID(int objid, int generation);

    // Replace the object with the given object id with the given object. The object handle passed
    // in must be a direct object, though it may contain references to other indirect objects within
    // it. Prior to qpdf 10.2.1, after calling this method, existing QPDFObjectHandle instances that
    // pointed to the original object still pointed to the original object, resulting in confusing
    // and incorrect behavior. This was fixed in 10.2.1, so existing QPDFObjectHandle objects will
    // start pointing to the newly replaced object. Note that replacing an object with
    // QPDFObjectHandle::newNull() effectively removes the object from the file since a non-existent
    // object is treated as a null object. To replace a reserved object, call replaceReserved
    // instead.
    QPDF_DLL
    void replaceObject(QPDFObjGen og, QPDFObjectHandle);
    QPDF_DLL
    void replaceObject(int objid, int generation, QPDFObjectHandle);

    // Swap two objects given by ID. Prior to qpdf 10.2.1, existing QPDFObjectHandle instances that
    // reference them objects not notice the swap, but this was fixed in 10.2.1.
    QPDF_DLL
    void swapObjects(QPDFObjGen og1, QPDFObjGen og2);
    QPDF_DLL
    void swapObjects(int objid1, int generation1, int objid2, int generation2);

    // Replace a reserved object.  This is a wrapper around replaceObject but it guarantees that the
    // underlying object is a reserved object or a null object.  After this call, reserved will
    // be a reference to replacement.
    QPDF_DLL
    void replaceReserved(QPDFObjectHandle reserved, QPDFObjectHandle replacement);

    // Copy an object from another QPDF to this one. Starting with qpdf version 8.3.0, it is no
    // longer necessary to keep the original QPDF around after the call to copyForeignObject as long
    // as the source of any copied stream data is still available. Usually this means you just have
    // to keep the input file around, not the QPDF object. The exception to this is if you copy a
    // stream that gets its data from a QPDFObjectHandle::StreamDataProvider. In this case only, the
    // original stream's QPDF object must stick around because the QPDF object is itself the source
    // of the original stream data. For a more in-depth discussion, please see the TODO file.
    // Starting in 8.4.0, you can call setImmediateCopyFrom(true) on the SOURCE QPDF object (the one
    // you're copying FROM). If you do this prior to copying any of its objects, then neither the
    // source QPDF object nor its input source needs to stick around at all regardless of the
    // source. The cost is that the stream data is copied into RAM at the time copyForeignObject is
    // called. See setImmediateCopyFrom for more information.
    //
    // The return value of this method is an indirect reference to the copied object in this file.
    // This method is intended to be used to copy non-page objects. To copy page objects, pass the
    // foreign page object directly to addPage (or addPageAt). If you copy objects that contain
    // references to pages, you should copy the pages first using addPage(At). Otherwise references
    // to the pages that have not been copied will be replaced with nulls. It is possible to use
    // copyForeignObject on page objects if you are not going to use them as pages. Doing so copies
    // the object normally but does not update the page structure. For example, it is a valid use
    // case to use copyForeignObject for a page that you are going to turn into a form XObject,
    // though you can also use QPDFPageObjectHelper::getFormXObjectForPage for that purpose.
    //
    // When copying objects with this method, object structure will be preserved, so all indirectly
    // referenced indirect objects will be copied as well.  This includes any circular references
    // that may exist.  The QPDF object keeps a record of what has already been copied, so shared
    // objects will not be copied multiple times.  This also means that if you mutate an object that
    // has already been copied and try to copy it again, it won't work since the modified object
    // will not be recopied.  Therefore, you should do all mutation on the original file that you
    // are going to do before you start copying its objects to a new file.
    QPDF_DLL
    QPDFObjectHandle copyForeignObject(QPDFObjectHandle foreign);

    // Encryption support

    enum encryption_method_e { e_none, e_unknown, e_rc4, e_aes, e_aesv3 };
    class EncryptionData
    {
      public:
        // This class holds data read from the encryption dictionary.
        EncryptionData(
            int V,
            int R,
            int Length_bytes,
            int P,
            std::string const& O,
            std::string const& U,
            std::string const& OE,
            std::string const& UE,
            std::string const& Perms,
            std::string const& id1,
            bool encrypt_metadata) :
            V(V),
            R(R),
            Length_bytes(Length_bytes),
            P(P),
            O(O),
            U(U),
            OE(OE),
            UE(UE),
            Perms(Perms),
            id1(id1),
            encrypt_metadata(encrypt_metadata)
        {
        }

        int getV() const;
        int getR() const;
        int getLengthBytes() const;
        int getP() const;
        std::string const& getO() const;
        std::string const& getU() const;
        std::string const& getOE() const;
        std::string const& getUE() const;
        std::string const& getPerms() const;
        std::string const& getId1() const;
        bool getEncryptMetadata() const;

        void setO(std::string const&);
        void setU(std::string const&);
        void setV5EncryptionParameters(
            std::string const& O,
            std::string const& OE,
            std::string const& U,
            std::string const& UE,
            std::string const& Perms);

      private:
        EncryptionData(EncryptionData const&) = delete;
        EncryptionData& operator=(EncryptionData const&) = delete;

        int V;
        int R;
        int Length_bytes;
        int P;
        std::string O;
        std::string U;
        std::string OE;
        std::string UE;
        std::string Perms;
        std::string id1;
        bool encrypt_metadata;
    };

    QPDF_DLL
    bool isEncrypted() const;

    QPDF_DLL
    bool isEncrypted(int& R, int& P);

    QPDF_DLL
    bool isEncrypted(
        int& R,
        int& P,
        int& V,
        encryption_method_e& stream_method,
        encryption_method_e& string_method,
        encryption_method_e& file_method);

    QPDF_DLL
    bool ownerPasswordMatched() const;

    QPDF_DLL
    bool userPasswordMatched() const;

    // Encryption permissions -- not enforced by QPDF
    QPDF_DLL
    bool allowAccessibility();
    QPDF_DLL
    bool allowExtractAll();
    QPDF_DLL
    bool allowPrintLowRes();
    QPDF_DLL
    bool allowPrintHighRes();
    QPDF_DLL
    bool allowModifyAssembly();
    QPDF_DLL
    bool allowModifyForm();
    QPDF_DLL
    bool allowModifyAnnotation();
    QPDF_DLL
    bool allowModifyOther();
    QPDF_DLL
    bool allowModifyAll();

    // Helper function to trim padding from user password.  Calling trim_user_password on the result
    // of getPaddedUserPassword gives getTrimmedUserPassword's result.
    QPDF_DLL
    static void trim_user_password(std::string& user_password);
    QPDF_DLL
    static std::string compute_data_key(
        std::string const& encryption_key,
        int objid,
        int generation,
        bool use_aes,
        int encryption_V,
        int encryption_R);
    QPDF_DLL
    static std::string
    compute_encryption_key(std::string const& password, EncryptionData const& data);

    QPDF_DLL
    static void compute_encryption_O_U(
        char const* user_password,
        char const* owner_password,
        int V,
        int R,
        int key_len,
        int P,
        bool encrypt_metadata,
        std::string const& id1,
        std::string& O,
        std::string& U);
    QPDF_DLL
    static void compute_encryption_parameters_V5(
        char const* user_password,
        char const* owner_password,
        int V,
        int R,
        int key_len,
        int P,
        bool encrypt_metadata,
        std::string const& id1,
        std::string& encryption_key,
        std::string& O,
        std::string& U,
        std::string& OE,
        std::string& UE,
        std::string& Perms);
    // Return the full user password as stored in the PDF file.  For files encrypted with 40-bit or
    // 128-bit keys, the user password can be recovered when the file is opened using the owner
    // password.  This is not possible with newer encryption formats. If you are attempting to
    // recover the user password in a user-presentable form, call getTrimmedUserPassword() instead.
    QPDF_DLL
    std::string const& getPaddedUserPassword() const;
    // Return human-readable form of user password subject to same limitations as
    // getPaddedUserPassword().
    QPDF_DLL
    std::string getTrimmedUserPassword() const;
    // Return the previously computed or retrieved encryption key for this file
    QPDF_DLL
    std::string getEncryptionKey() const;
    // Remove security restrictions associated with digitally signed files. From qpdf 11.7.0, this
    // is called by QPDFAcroFormDocumentHelper::disableDigitalSignatures and is more useful when
    // called from there than when just called by itself.
    QPDF_DLL
    void removeSecurityRestrictions();

    // Linearization support

    // Returns true iff the file starts with a linearization parameter dictionary.  Does no
    // additional validation.
    QPDF_DLL
    bool isLinearized();

    // Performs various sanity checks on a linearized file. Return true if no errors or warnings.
    // Otherwise, return false and output errors and warnings to the default output stream
    // (std::cout or whatever is configured in the logger). It is recommended for linearization
    // errors to be treated as warnings.
    QPDF_DLL
    bool checkLinearization();

    // Calls checkLinearization() and, if possible, prints normalized contents of some of the hints
    // tables to the default output stream. Normalization includes adding min values to delta values
    // and adjusting offsets based on the location and size of the primary hint stream.
    QPDF_DLL
    void showLinearizationData();

    // Shows the contents of the cross-reference table
    QPDF_DLL
    void showXRefTable();

    // Starting from qpdf 11.0 user code should not need to call this method. Before 11.0 this
    // method was used to detect all indirect references to objects that don't exist and resolve
    // them by replacing them with null, which is how the PDF spec says to interpret such dangling
    // references. This method is called automatically when you try to add any new objects, if you
    // call getAllObjects, and before a file is written. The qpdf object caches whether it has run
    // this to avoid running it multiple times. Before 11.2.1 you could pass true to force it to run
    // again if you had explicitly added new objects that may have additional dangling references.
    QPDF_DLL
    void fixDanglingReferences(bool force = false);

    // Return the approximate number of indirect objects. It is/ approximate because not all objects
    // in the file are preserved in all cases, and gaps in object numbering are not preserved.
    QPDF_DLL
    size_t getObjectCount();

    // Returns a list of indirect objects for every object in the xref table. Useful for discovering
    // objects that are not otherwise referenced.
    QPDF_DLL
    std::vector<QPDFObjectHandle> getAllObjects();

    // Optimization support -- see doc/optimization.  Implemented in QPDF_optimization.cc

    // The object_stream_data map maps from a "compressed" object to the object stream that contains
    // it. This enables optimize to populate the object <-> user maps with only uncompressed
    // objects. If allow_changes is false, an exception will be thrown if any changes are made
    // during the optimization process. This is available so that the test suite can make sure that
    // a linearized file is already optimized. When called in this way, optimize() still populates
    // the object <-> user maps. The optional skip_stream_parameters parameter, if present, is
    // called for each stream object. The function should return 2 if optimization should discard
    // /Length, /Filter, and /DecodeParms; 1 if it should discard /Length, and 0 if it should
    // preserve all keys. This is used by QPDFWriter to avoid creation of dangling objects for
    // stream dictionary keys it will be regenerating.
    [[deprecated("Unused - see release notes for qpdf 12.1.0")]] QPDF_DLL void optimize(
        std::map<int, int> const& object_stream_data,
        bool allow_changes = true,
        std::function<int(QPDFObjectHandle&)> skip_stream_parameters = nullptr);

    // Traverse page tree return all /Page objects. It also detects and resolves cases in which the
    // same /Page object is duplicated. For efficiency, this method returns a const reference to an
    // internal vector of pages. Calls to addPage, addPageAt, and removePage safely update this, but
    // direct manipulation of the pages tree or pushing inheritable objects to the page level may
    // invalidate it. See comments for updateAllPagesCache() for additional notes. Newer code should
    // use QPDFPageDocumentHelper::getAllPages instead. The decision to expose this internal cache
    // was arguably incorrect, but it is being left here for compatibility. It is, however,
    // completely safe to use this for files that you are not modifying.
    QPDF_DLL
    std::vector<QPDFObjectHandle> const& getAllPages();

    QPDF_DLL
    bool everCalledGetAllPages() const;
    QPDF_DLL
    bool everPushedInheritedAttributesToPages() const;

    // These methods, given a page object or its object/generation number, returns the 0-based index
    // into the array returned by getAllPages() for that page. An exception is thrown if the page is
    // not found.
    QPDF_DLL
    int findPage(QPDFObjGen og);
    QPDF_DLL
    int findPage(QPDFObjectHandle& page);

    // This method synchronizes QPDF's cache of the page structure with the actual /Pages tree.  If
    // you restrict changes to the /Pages tree, including addition, removal, or replacement of pages
    // or changes to any /Pages objects, to calls to these page handling APIs, you never need to
    // call this method.  If you modify /Pages structures directly, you must call this method
    // afterwards.  This method updates the internal list of pages, so after calling this method,
    // any previous references returned by getAllPages() will be valid again.  It also resets any
    // state about having pushed inherited attributes in /Pages objects down to the pages, so if you
    // add any inheritable attributes to a /Pages object, you should also call this method.
    QPDF_DLL
    void updateAllPagesCache();

    // Legacy handling API. These methods are not going anywhere, and you should feel free to
    // continue using them if it simplifies your code. Newer code should make use of
    // QPDFPageDocumentHelper instead as future page handling methods will be added there. The
    // functionality and specification of these legacy methods is identical to the identically named
    // methods there, except that these versions use QPDFObjectHandle instead of
    // QPDFPageObjectHelper, so please see comments in that file for descriptions. There are
    // subtleties you need to know about, so please look at the comments there.
    QPDF_DLL
    void pushInheritedAttributesToPage();
    QPDF_DLL
    void addPage(QPDFObjectHandle newpage, bool first);
    QPDF_DLL
    void addPageAt(QPDFObjectHandle newpage, bool before, QPDFObjectHandle refpage);
    QPDF_DLL
    void removePage(QPDFObjectHandle page);
    // End legacy page helpers

    // End of the public API. The following classes and methods are for qpdf internal use only.

    class Writer;
    class Resolver;
    class StreamCopier;
    class ParseGuard;
    class Pipe;
    class JobSetter;

    // For testing only -- do not add to DLL
    static bool test_json_validators();

  private:
    // It has never been safe to copy QPDF objects as there is code in the library that assumes
    // there are no copies of a QPDF object. Copying QPDF objects was not prevented by the API until
    // qpdf 11. If you have been copying QPDF objects, use std::shared_ptr<QPDF> instead. From qpdf
    // 11, you can use QPDF::create to create them.
    QPDF(QPDF const&) = delete;
    QPDF& operator=(QPDF const&) = delete;

    static std::string const qpdf_version;

    class ObjCache;
    class ObjCopier;
    class EncryptionParameters;
    class ForeignStreamData;
    class CopiedStreamDataProvider;
    class StringDecrypter;
    class ResolveRecorder;
    class JSONReactor;

    void parse(char const* password);
    void inParse(bool);
    void setTrailer(QPDFObjectHandle obj);
    void read_xref(qpdf_offset_t offset, bool in_stream_recovery = false);
    bool resolveXRefTable();
    void reconstruct_xref(QPDFExc& e, bool found_startxref = true);
    bool parse_xrefFirst(std::string const& line, int& obj, int& num, int& bytes);
    bool read_xrefEntry(qpdf_offset_t& f1, int& f2, char& type);
    bool read_bad_xrefEntry(qpdf_offset_t& f1, int& f2, char& type);
    qpdf_offset_t read_xrefTable(qpdf_offset_t offset);
    qpdf_offset_t read_xrefStream(qpdf_offset_t offset, bool in_stream_recovery = false);
    qpdf_offset_t processXRefStream(
        qpdf_offset_t offset, QPDFObjectHandle& xref_stream, bool in_stream_recovery = false);
    std::pair<int, std::array<int, 3>>
    processXRefW(QPDFObjectHandle& dict, std::function<QPDFExc(std::string_view)> damaged);
    int processXRefSize(
        QPDFObjectHandle& dict, int entry_size, std::function<QPDFExc(std::string_view)> damaged);
    std::pair<int, std::vector<std::pair<int, int>>> processXRefIndex(
        QPDFObjectHandle& dict,
        int max_num_entries,
        std::function<QPDFExc(std::string_view)> damaged);
    void insertXrefEntry(int obj, int f0, qpdf_offset_t f1, int f2);
    void insertFreeXrefEntry(QPDFObjGen);
    void setLastObjectDescription(std::string const& description, QPDFObjGen og);
    QPDFObjectHandle readTrailer();
    QPDFObjectHandle readObject(std::string const& description, QPDFObjGen og);
    void readStream(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset);
    void validateStreamLineEnd(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset);
    QPDFObjectHandle readObjectInStream(qpdf::is::OffsetBuffer& input, int stream_id, int obj_id);
    size_t recoverStreamLength(
        std::shared_ptr<InputSource> input, QPDFObjGen og, qpdf_offset_t stream_offset);
    QPDFTokenizer::Token readToken(InputSource&, size_t max_len = 0);

    QPDFObjGen read_object_start(qpdf_offset_t offset);
    void readObjectAtOffset(
        bool attempt_recovery,
        qpdf_offset_t offset,
        std::string const& description,
        QPDFObjGen exp_og);
    QPDFObjectHandle readObjectAtOffset(
        qpdf_offset_t offset, std::string const& description, bool skip_cache_if_in_xref);
    std::shared_ptr<QPDFObject> const& resolve(QPDFObjGen og);
    void resolveObjectsInStream(int obj_stream_number);
    void stopOnError(std::string const& message);
    QPDFObjGen nextObjGen();
    QPDFObjectHandle newIndirect(QPDFObjGen, std::shared_ptr<QPDFObject> const&);
    QPDFObjectHandle makeIndirectFromQPDFObject(std::shared_ptr<QPDFObject> const& obj);
    bool isCached(QPDFObjGen og);
    bool isUnresolved(QPDFObjGen og);
    std::shared_ptr<QPDFObject> getObjectForParser(int id, int gen, bool parse_pdf);
    std::shared_ptr<QPDFObject> getObjectForJSON(int id, int gen);
    void removeObject(QPDFObjGen og);
    void updateCache(
        QPDFObjGen og,
        std::shared_ptr<QPDFObject> const& object,
        qpdf_offset_t end_before_space,
        qpdf_offset_t end_after_space,
        bool destroy = true);
    static QPDFExc damagedPDF(
        InputSource& input,
        std::string const& object,
        qpdf_offset_t offset,
        std::string const& message);
    QPDFExc damagedPDF(InputSource& input, qpdf_offset_t offset, std::string const& message);
    QPDFExc damagedPDF(std::string const& object, qpdf_offset_t offset, std::string const& message);
    QPDFExc damagedPDF(std::string const& object, std::string const& message);
    QPDFExc damagedPDF(qpdf_offset_t offset, std::string const& message);
    QPDFExc damagedPDF(std::string const& message);

    // Calls finish() on the pipeline when done but does not delete it
    bool pipeStreamData(
        QPDFObjGen og,
        qpdf_offset_t offset,
        size_t length,
        QPDFObjectHandle dict,
        bool is_root_metadata,
        Pipeline* pipeline,
        bool suppress_warnings,
        bool will_retry);
    bool pipeForeignStreamData(
        std::shared_ptr<ForeignStreamData>, Pipeline*, bool suppress_warnings, bool will_retry);
    static bool pipeStreamData(
        std::shared_ptr<QPDF::EncryptionParameters> encp,
        std::shared_ptr<InputSource> file,
        QPDF& qpdf_for_warning,
        QPDFObjGen og,
        qpdf_offset_t offset,
        size_t length,
        QPDFObjectHandle dict,
        bool is_root_metadata,
        Pipeline* pipeline,
        bool suppress_warnings,
        bool will_retry);

    // For QPDFWriter:

    std::map<QPDFObjGen, QPDFXRefEntry> const& getXRefTableInternal();
    template <typename T>
    void optimize_internal(
        T const& object_stream_data,
        bool allow_changes = true,
        std::function<int(QPDFObjectHandle&)> skip_stream_parameters = nullptr);
    void optimize(
        QPDFWriter::ObjTable const& obj,
        std::function<int(QPDFObjectHandle&)> skip_stream_parameters);
    size_t tableSize();

    // Get lists of all objects in order according to the part of a linearized file that they belong
    // to.
    void getLinearizedParts(
        QPDFWriter::ObjTable const& obj,
        std::vector<QPDFObjectHandle>& part4,
        std::vector<QPDFObjectHandle>& part6,
        std::vector<QPDFObjectHandle>& part7,
        std::vector<QPDFObjectHandle>& part8,
        std::vector<QPDFObjectHandle>& part9);

    void generateHintStream(
        QPDFWriter::NewObjTable const& new_obj,
        QPDFWriter::ObjTable const& obj,
        std::string& hint_stream,
        int& S,
        int& O,
        bool compressed);

    // Get a list of objects that would be permitted in an object stream.
    template <typename T>
    std::vector<T> getCompressibleObjGens();
    std::vector<QPDFObjGen> getCompressibleObjVector();
    std::vector<bool> getCompressibleObjSet();

    // methods to support page handling

    void getAllPagesInternal(
        QPDFObjectHandle cur_pages,
        QPDFObjGen::set& visited,
        QPDFObjGen::set& seen,
        bool media_box,
        bool resources);
    void insertPage(QPDFObjectHandle newpage, int pos);
    void flattenPagesTree();
    void insertPageobjToPage(QPDFObjectHandle const& obj, int pos, bool check_duplicate);

    // methods to support encryption -- implemented in QPDF_encryption.cc
    static encryption_method_e
    interpretCF(std::shared_ptr<EncryptionParameters> encp, QPDFObjectHandle);
    void initializeEncryption();
    static std::string
    getKeyForObject(std::shared_ptr<EncryptionParameters> encp, QPDFObjGen og, bool use_aes);
    void decryptString(std::string&, QPDFObjGen og);
    static std::string
    compute_encryption_key_from_password(std::string const& password, EncryptionData const& data);
    static std::string
    recover_encryption_key_with_password(std::string const& password, EncryptionData const& data);
    static std::string recover_encryption_key_with_password(
        std::string const& password, EncryptionData const& data, bool& perms_valid);
    static void decryptStream(
        std::shared_ptr<EncryptionParameters> encp,
        std::shared_ptr<InputSource> file,
        QPDF& qpdf_for_warning,
        Pipeline*& pipeline,
        QPDFObjGen og,
        QPDFObjectHandle& stream_dict,
        bool is_root_metadata,
        std::unique_ptr<Pipeline>& heap);

    // Methods to support object copying
    void reserveObjects(QPDFObjectHandle foreign, ObjCopier& obj_copier, bool top);
    QPDFObjectHandle
    replaceForeignIndirectObjects(QPDFObjectHandle foreign, ObjCopier& obj_copier, bool top);
    void copyStreamData(QPDFObjectHandle dest_stream, QPDFObjectHandle src_stream);

    struct HPageOffsetEntry;
    struct HPageOffset;
    struct HSharedObjectEntry;
    struct HSharedObject;
    struct HGeneric;
    struct LinParameters;
    struct CHPageOffsetEntry;
    struct CHPageOffset;
    struct CHSharedObjectEntry;
    struct CHSharedObject;
    class ObjUser;
    struct UpdateObjectMapsFrame;
    class PatternFinder;

    // Methods to support pattern finding
    static bool validatePDFVersion(char const*&, std::string& version);
    bool findHeader();
    bool findStartxref();
    bool findEndstream();

    // methods to support linearization checking -- implemented in QPDF_linearization.cc
    void readLinearizationData();
    bool checkLinearizationInternal();
    void dumpLinearizationDataInternal();
    void linearizationWarning(std::string_view);
    QPDFObjectHandle readHintStream(Pipeline&, qpdf_offset_t offset, size_t length);
    void readHPageOffset(BitStream);
    void readHSharedObject(BitStream);
    void readHGeneric(BitStream, HGeneric&);
    qpdf_offset_t maxEnd(ObjUser const& ou);
    qpdf_offset_t getLinearizationOffset(QPDFObjGen);
    QPDFObjectHandle
    getUncompressedObject(QPDFObjectHandle&, std::map<int, int> const& object_stream_data);
    QPDFObjectHandle getUncompressedObject(QPDFObjectHandle&, QPDFWriter::ObjTable const& obj);
    int lengthNextN(int first_object, int n);
    void
    checkHPageOffset(std::vector<QPDFObjectHandle> const& pages, std::map<int, int>& idx_to_obj);
    void
    checkHSharedObject(std::vector<QPDFObjectHandle> const& pages, std::map<int, int>& idx_to_obj);
    void checkHOutlines();
    void dumpHPageOffset();
    void dumpHSharedObject();
    void dumpHGeneric(HGeneric&);
    qpdf_offset_t adjusted_offset(qpdf_offset_t offset);
    template <typename T>
    void calculateLinearizationData(T const& object_stream_data);
    template <typename T>
    void pushOutlinesToPart(
        std::vector<QPDFObjectHandle>& part,
        std::set<QPDFObjGen>& lc_outlines,
        T const& object_stream_data);
    int outputLengthNextN(
        int in_object,
        int n,
        QPDFWriter::NewObjTable const& new_obj,
        QPDFWriter::ObjTable const& obj);
    void
    calculateHPageOffset(QPDFWriter::NewObjTable const& new_obj, QPDFWriter::ObjTable const& obj);
    void
    calculateHSharedObject(QPDFWriter::NewObjTable const& new_obj, QPDFWriter::ObjTable const& obj);
    void calculateHOutline(QPDFWriter::NewObjTable const& new_obj, QPDFWriter::ObjTable const& obj);
    void writeHPageOffset(BitWriter&);
    void writeHSharedObject(BitWriter&);
    void writeHGeneric(BitWriter&, HGeneric&);

    // Methods to support optimization

    void pushInheritedAttributesToPage(bool allow_changes, bool warn_skipped_keys);
    void pushInheritedAttributesToPageInternal(
        QPDFObjectHandle,
        std::map<std::string, std::vector<QPDFObjectHandle>>&,
        bool allow_changes,
        bool warn_skipped_keys);
    void updateObjectMaps(
        ObjUser const& ou,
        QPDFObjectHandle oh,
        std::function<int(QPDFObjectHandle&)> skip_stream_parameters);
    void filterCompressedObjects(std::map<int, int> const& object_stream_data);
    void filterCompressedObjects(QPDFWriter::ObjTable const& object_stream_data);

    // JSON import
    void importJSON(std::shared_ptr<InputSource>, bool must_be_complete);

    // Type conversion helper methods
    template <typename T>
    static qpdf_offset_t
    toO(T const& i)
    {
        return QIntC::to_offset(i);
    }
    template <typename T>
    static size_t
    toS(T const& i)
    {
        return QIntC::to_size(i);
    }
    template <typename T>
    static int
    toI(T const& i)
    {
        return QIntC::to_int(i);
    }
    template <typename T>
    static unsigned long long
    toULL(T const& i)
    {
        return QIntC::to_ulonglong(i);
    }

    class Members;

    // Keep all member variables inside the Members object, which we dynamically allocate. This
    // makes it possible to add new private members without breaking binary compatibility.
    std::unique_ptr<Members> m;
};

#endif // QPDF_HH
