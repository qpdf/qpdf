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

#ifndef QPDF_HH
#define QPDF_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>

#include <stdio.h>
#include <string>
#include <map>
#include <list>
#include <iostream>

#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFXRefEntry.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFTokenizer.hh>
#include <qpdf/Buffer.hh>
#include <qpdf/InputSource.hh>

class QPDF_Stream;
class BitStream;
class BitWriter;

class QPDF
{
  public:
    // Get the current version of the QPDF software
    QPDF_DLL
    static std::string const& QPDFVersion();

    QPDF_DLL
    QPDF();
    QPDF_DLL
    ~QPDF();

    // Associate a file with a QPDF object and do initial parsing of
    // the file.  PDF objects are not read until they are needed.  A
    // QPDF object may be associated with only one file in its
    // lifetime.  This method must be called before any methods that
    // potentially ask for information about the PDF file are called.
    // Prior to calling this, the only methods that are allowed are
    // those that set parameters.  If the input file is not
    // encrypted,either a null password or an empty password can be
    // used.  If the file is encrypted, either the user password or
    // the owner password may be supplied. The method
    // setPasswordIsHexKey may be called prior to calling this method
    // or any of the other process methods to force the password to be
    // interpreted as a raw encryption key. See comments on
    // setPasswordIsHexKey for more information.
    QPDF_DLL
    void processFile(char const* filename, char const* password = 0);

    // Parse a PDF from a stdio FILE*.  The FILE must be open in
    // binary mode and must be seekable.  It may be open read only.
    // This works exactly like processFile except that the PDF file is
    // read from an already opened FILE*.  If close_file is true, the
    // file will be closed at the end.  Otherwise, the caller is
    // responsible for closing the file.
    QPDF_DLL
    void processFile(char const* description, FILE* file,
                     bool close_file, char const* password = 0);

    // Parse a PDF file loaded into a memory buffer.  This works
    // exactly like processFile except that the PDF file is in memory
    // instead of on disk.  The description appears in any warning or
    // error message in place of the file name.
    QPDF_DLL
    void processMemoryFile(char const* description,
			   char const* buf, size_t length,
			   char const* password = 0);

    // Parse a PDF file loaded from a custom InputSource.  If you have
    // your own method of retrieving a PDF file, you can subclass
    // InputSource and use this method.
    QPDF_DLL
    void processInputSource(PointerHolder<InputSource>,
                            char const* password = 0);

    // For certain forensic or investigatory purposes, it may
    // sometimes be useful to specify the encryption key directly,
    // even though regular PDF applications do not provide a way to do
    // this. calling setPasswordIsHexKey(true) before calling any of
    // the process methods will bypass the normal encryption key
    // computation or recovery mechanisms and interpret the bytes in
    // the password as a hex-encoded encryption key. Note that we
    // hex-encode the key because it may contain null bytes and
    // therefore can't be represented in a char const*.
    QPDF_DLL
    void setPasswordIsHexKey(bool);

    // Create a QPDF object for an empty PDF.  This PDF has no pages
    // or objects other than a minimal trailer, a document catalog,
    // and a /Pages tree containing zero pages.  Pages and other
    // objects can be added to the file in the normal way, and the
    // trailer and document catalog can be mutated.  Calling this
    // method is equivalent to calling processFile on an equivalent
    // PDF file.  See the pdf-create.cc example for a demonstration of
    // how to use this method to create a PDF file from scratch.
    QPDF_DLL
    void emptyPDF();

    // Parameter settings

    // By default, warning messages are issued to std::cerr and output
    // messages printed by certain check calls are issued to
    // std::cout.  This method allows you to specify alternative
    // streams for this purpose.  Note that no normal QPDF operations
    // generate output to std::cout, so for applications that just
    // wish to avoid creating output and don't call any check
    // functions, calling setSuppressWarnings(true) is sufficient.
    // Applications that wish to present check or warning information
    // to users may replace the output and error streams to capture
    // the output and errors for other use.  A null value for either
    // stream will cause QPDF to use std::cout or std::cerr as
    // appropriate.
    QPDF_DLL
    void setOutputStreams(std::ostream* out_stream, std::ostream* err_stream);

    // If true, ignore any cross-reference streams in a hybrid file
    // (one that contains both cross-reference streams and
    // cross-reference tables).  This can be useful for testing to
    // ensure that a hybrid file would work with an older reader.
    QPDF_DLL
    void setIgnoreXRefStreams(bool);

    // By default, any warnings are issued to std::cerr or the error
    // stream specified in a call to setOutputStreams as they are
    // encountered.  If this is called with a true value, reporting of
    // warnings is suppressed.  You may still retrieve warnings by
    // calling getWarnings.
    QPDF_DLL
    void setSuppressWarnings(bool);

    // By default, QPDF will try to recover if it finds certain types
    // of errors in PDF files.  If turned off, it will throw an
    // exception on the first such problem it finds without attempting
    // recovery.
    QPDF_DLL
    void setAttemptRecovery(bool);

    // Other public methods

    // Return the list of warnings that have been issued so far and
    // clear the list.  This method may be called even if processFile
    // throws an exception.  Note that if setSuppressWarnings was not
    // called or was called with a false value, any warnings retrieved
    // here will have already been output.
    QPDF_DLL
    std::vector<QPDFExc> getWarnings();

    QPDF_DLL
    std::string getFilename() const;
    QPDF_DLL
    std::string getPDFVersion() const;
    QPDF_DLL
    int getExtensionLevel();
    QPDF_DLL
    QPDFObjectHandle getTrailer();
    QPDF_DLL
    QPDFObjectHandle getRoot();

    // Install this object handle as an indirect object and return an
    // indirect reference to it.
    QPDF_DLL
    QPDFObjectHandle makeIndirectObject(QPDFObjectHandle);

    // Retrieve an object by object ID and generation.  Returns an
    // indirect reference to it.
    QPDF_DLL
    QPDFObjectHandle getObjectByObjGen(QPDFObjGen const&);
    QPDF_DLL
    QPDFObjectHandle getObjectByID(int objid, int generation);

    // Replace the object with the given object id with the given
    // object.  The object handle passed in must be a direct object,
    // though it may contain references to other indirect objects
    // within it.  Calling this method can have somewhat confusing
    // results.  Any existing QPDFObjectHandle instances that point to
    // the old object and that have been resolved (which happens
    // automatically if you access them in any way) will continue to
    // point to the old object even though that object will no longer
    // be associated with the PDF file.  Note that replacing an object
    // with QPDFObjectHandle::newNull() effectively removes the object
    // from the file since a non-existent object is treated as a null
    // object.  To replace a reserved object, call replaceReserved
    // instead.
    QPDF_DLL
    void replaceObject(QPDFObjGen const& og, QPDFObjectHandle);
    QPDF_DLL
    void replaceObject(int objid, int generation, QPDFObjectHandle);

    // Swap two objects given by ID.  Calling this method can have
    // confusing results.  After swapping two objects, existing
    // QPDFObjectHandle instances that reference them will still
    // reference the same underlying objects, at which point those
    // existing QPDFObjectHandle instances will have incorrect
    // information about the object and generation number of those
    // objects.  While this does not necessarily cause a problem, it
    // can certainly be confusing.  It is therefore recommended that
    // you replace any existing QPDFObjectHandle instances that point
    // to the swapped objects with new ones, possibly by calling
    // getObjectByID.
    QPDF_DLL
    void swapObjects(QPDFObjGen const& og1, QPDFObjGen const& og2);
    QPDF_DLL
    void swapObjects(int objid1, int generation1,
		     int objid2, int generation2);

    // Replace a reserved object.  This is a wrapper around
    // replaceObject but it guarantees that the underlying object is a
    // reserved object.  After this call, reserved will be a reference
    // to replacement.
    QPDF_DLL
    void
    replaceReserved(QPDFObjectHandle reserved,
                    QPDFObjectHandle replacement);

    // Copy an object from another QPDF to this one. Please note that
    // the QPDF object containing the object being copied must stick
    // around because it is still used to retrieve any stream data
    // referenced by the copied objects.
    //
    // The return value is an indirect reference to the copied object
    // in this file. This method is intended to be used to copy
    // non-page objects and will not copy page objects. To copy page
    // objects, pass the foreign page object directly to addPage (or
    // addPageAt). If you copy objects that contain references to
    // pages, you should copy the pages first using addPage(At).
    // Otherwise references to the pages that have not been copied
    // will be replaced with nulls.

    // When copying objects with this method, object structure will be
    // preserved, so all indirectly referenced indirect objects will
    // be copied as well.  This includes any circular references that
    // may exist.  The QPDF object keeps a record of what has already
    // been copied, so shared objects will not be copied multiple
    // times.  This also means that if you mutate an object that has
    // already been copied and try to copy it again, it won't work
    // since the modified object will not be recopied.  Therefore, you
    // should do all mutation on the original file that you are going
    // to do before you start copying its objects to a new file.
    QPDF_DLL
    QPDFObjectHandle copyForeignObject(QPDFObjectHandle foreign);

    // Encryption support

    enum encryption_method_e { e_none, e_unknown, e_rc4, e_aes, e_aesv3 };
    class EncryptionData
    {
      public:

	// This class holds data read from the encryption dictionary.
	EncryptionData(int V, int R, int Length_bytes, int P,
		       std::string const& O, std::string const& U,
                       std::string const& OE, std::string const& UE,
                       std::string const& Perms,
		       std::string const& id1, bool encrypt_metadata) :
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
        void setV5EncryptionParameters(std::string const& O,
                                       std::string const& OE,
                                       std::string const& U,
                                       std::string const& UE,
                                       std::string const& Perms);

      private:
        EncryptionData(EncryptionData const&);
        EncryptionData& operator=(EncryptionData const&);

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
    bool isEncrypted(int& R, int& P, int& V,
                     encryption_method_e& stream_method,
                     encryption_method_e& string_method,
                     encryption_method_e& file_method);

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

    // Helper function to trim padding from user password.  Calling
    // trim_user_password on the result of getPaddedUserPassword gives
    // getTrimmedUserPassword's result.
    QPDF_DLL
    static void trim_user_password(std::string& user_password);
    QPDF_DLL
    static std::string compute_data_key(
	std::string const& encryption_key, int objid, int generation,
	bool use_aes, int encryption_V, int encryption_R);
    QPDF_DLL
    static std::string compute_encryption_key(
	std::string const& password, EncryptionData const& data);

    QPDF_DLL
    static void compute_encryption_O_U(
	char const* user_password, char const* owner_password,
	int V, int R, int key_len, int P, bool encrypt_metadata,
	std::string const& id1,
	std::string& O, std::string& U);
    QPDF_DLL
    static void compute_encryption_parameters_V5(
	char const* user_password, char const* owner_password,
	int V, int R, int key_len, int P, bool encrypt_metadata,
	std::string const& id1,
        std::string& encryption_key,
	std::string& O, std::string& U,
        std::string& OE, std::string& UE, std::string& Perms);
    // Return the full user password as stored in the PDF file.  For
    // files encrypted with 40-bit or 128-bit keys, the user password
    // can be recovered when the file is opened using the owner
    // password.  This is not possible with newer encryption formats.
    // If you are attempting to recover the user password in a
    // user-presentable form, call getTrimmedUserPassword() instead.
    QPDF_DLL
    std::string const& getPaddedUserPassword() const;
    // Return human-readable form of user password subject to same
    // limitations as getPaddedUserPassword().
    QPDF_DLL
    std::string getTrimmedUserPassword() const;
    // Return the previously computed or retrieved encryption key for
    // this file
    QPDF_DLL
    std::string getEncryptionKey() const;

    // Linearization support

    // Returns true iff the file starts with a linearization parameter
    // dictionary.  Does no additional validation.
    QPDF_DLL
    bool isLinearized();

    // Performs various sanity checks on a linearized file.  Return
    // true if no errors or warnings.  Otherwise, return false and
    // output errors and warnings to std::cout or the output stream
    // specified in a call to setOutputStreams.
    QPDF_DLL
    bool checkLinearization();

    // Calls checkLinearization() and, if possible, prints normalized
    // contents of some of the hints tables to std::cout or the output
    // stream specified in a call to setOutputStreams.  Normalization
    // includes adding min values to delta values and adjusting
    // offsets based on the location and size of the primary hint
    // stream.
    QPDF_DLL
    void showLinearizationData();

    // Shows the contents of the cross-reference table
    QPDF_DLL
    void showXRefTable();

    // Return the approximate number of indirect objects. It is
    // approximate because not all objects in the file are preserved
    // in all cases.
    QPDF_DLL
    size_t getObjectCount();

    // Returns a list of indirect objects for every object in the xref
    // table. Useful for discovering objects that are not otherwise
    // referenced.
    QPDF_DLL
    std::vector<QPDFObjectHandle> getAllObjects();

    // Optimization support -- see doc/optimization.  Implemented in
    // QPDF_optimization.cc

    // The object_stream_data map maps from a "compressed" object to
    // the object stream that contains it.  This enables optimize to
    // populate the object <-> user maps with only uncompressed
    // objects.  If allow_changes is false, an exception will be
    // thrown if any changes are made during the optimization process.
    // This is available so that the test suite can make sure that a
    // linearized file is already optimized.  When called in this way,
    // optimize() still populates the object <-> user maps
    QPDF_DLL
    void optimize(std::map<int, int> const& object_stream_data,
		  bool allow_changes = true);

    // Traverse page tree return all /Page objects. For efficiency,
    // this method returns a const reference to an internal vector of
    // pages. Calls to addPage, addPageAt, and removePage safely
    // update this, but directly manipulation of the pages three or
    // pushing inheritable objects to the page level may invalidate
    // it. See comments for updateAllPagesCache() for additional
    // notes. Newer code should use
    // QPDFPageDocumentHelper::getAllPages instead. The decision to
    // expose this internal cache was arguably incorrect, but it is
    // being left here for compatibility. It is, however, completely
    // safe to use this for files that you are not modifying.
    QPDF_DLL
    std::vector<QPDFObjectHandle> const& getAllPages();

    // This method synchronizes QPDF's cache of the page structure
    // with the actual /Pages tree.  If you restrict changes to the
    // /Pages tree, including addition, removal, or replacement of
    // pages or changes to any /Pages objects, to calls to these page
    // handling APIs, you never need to call this method.  If you
    // modify /Pages structures directly, you must call this method
    // afterwards.  This method updates the internal list of pages, so
    // after calling this method, any previous references returned by
    // getAllPages() will be valid again.  It also resets any state
    // about having pushed inherited attributes in /Pages objects down
    // to the pages, so if you add any inheritable attributes to a
    // /Pages object, you should also call this method.
    QPDF_DLL
    void updateAllPagesCache();

    // Legacy handling API. These methods are not going anywhere, and
    // you should feel free to continue using them if it simplifies
    // your code. Newer code should make use of QPDFPageDocumentHelper
    // instead as future page handling methods will be added there.
    // The functionality and specification of these legacy methods is
    // identical to the identically named methods there, except that
    // these versions use QPDFObjectHandle instead of
    // QPDFPageObjectHelper, so please see comments in that file for
    // descriptions.
    QPDF_DLL
    void pushInheritedAttributesToPage();
    QPDF_DLL
    void addPage(QPDFObjectHandle newpage, bool first);
    QPDF_DLL
    void addPageAt(QPDFObjectHandle newpage, bool before,
                   QPDFObjectHandle refpage);
    QPDF_DLL
    void removePage(QPDFObjectHandle page);
    // End legacy page helpers

    // Writer class is restricted to QPDFWriter so that only it can
    // call certain methods.
    class Writer
    {
        friend class QPDFWriter;
      private:

        static void getLinearizedParts(
            QPDF& qpdf,
            std::map<int, int> const& object_stream_data,
            std::vector<QPDFObjectHandle>& part4,
            std::vector<QPDFObjectHandle>& part6,
            std::vector<QPDFObjectHandle>& part7,
            std::vector<QPDFObjectHandle>& part8,
            std::vector<QPDFObjectHandle>& part9)
        {
            qpdf.getLinearizedParts(object_stream_data,
                                    part4, part6, part7, part8, part9);
        }

        static void generateHintStream(
            QPDF& qpdf,
            std::map<int, QPDFXRefEntry> const& xref,
            std::map<int, qpdf_offset_t> const& lengths,
            std::map<int, int> const& obj_renumber,
            PointerHolder<Buffer>& hint_stream,
            int& S, int& O)
        {
            return qpdf.generateHintStream(xref, lengths, obj_renumber,
                                           hint_stream, S, O);
        }

        static void getObjectStreamData(QPDF& qpdf, std::map<int, int>& omap)
        {
            qpdf.getObjectStreamData(omap);
        }

        static std::vector<QPDFObjGen> getCompressibleObjGens(QPDF& qpdf)
        {
            return qpdf.getCompressibleObjGens();
        }
    };

    // Resolver class is restricted to QPDFObjectHandle so that only
    // it can resolve indirect references.
    class Resolver
    {
	friend class QPDFObjectHandle;
      private:
	static PointerHolder<QPDFObject> resolve(
	    QPDF* qpdf, int objid, int generation)
	{
	    return qpdf->resolve(objid, generation);
	}
    };
    friend class Resolver;

    // Warner class allows QPDFObjectHandle to create warnings
    class Warner
    {
	friend class QPDFObjectHandle;
        friend class QPDF_Stream;
      private:
        static void warn(QPDF* qpdf, QPDFExc const& e)
        {
            qpdf->warn(e);
        }
    };
    friend class Warner;

    // Pipe class is restricted to QPDF_Stream
    class Pipe
    {
	friend class QPDF_Stream;
      private:
	static bool pipeStreamData(QPDF* qpdf, int objid, int generation,
				   qpdf_offset_t offset, size_t length,
				   QPDFObjectHandle dict,
				   Pipeline* pipeline,
                                   bool suppress_warnings,
                                   bool will_retry)
	{
	    return qpdf->pipeStreamData(
		objid, generation, offset, length, dict, pipeline,
                suppress_warnings, will_retry);
	}
    };
    friend class Pipe;

  private:
    static std::string qpdf_version;

    class ObjCache
    {
      public:
	ObjCache() :
	    end_before_space(0),
	    end_after_space(0)
	{
	}
	ObjCache(PointerHolder<QPDFObject> object,
		 qpdf_offset_t end_before_space,
		 qpdf_offset_t end_after_space) :
	    object(object),
	    end_before_space(end_before_space),
	    end_after_space(end_after_space)
	{
	}

	PointerHolder<QPDFObject> object;
	qpdf_offset_t end_before_space;
	qpdf_offset_t end_after_space;
    };

    class ObjCopier
    {
      public:
        std::map<QPDFObjGen, QPDFObjectHandle> object_map;
        std::vector<QPDFObjectHandle> to_copy;
        std::set<QPDFObjGen> visiting;
    };

    class CopiedStreamDataProvider: public QPDFObjectHandle::StreamDataProvider
    {
      public:
        virtual ~CopiedStreamDataProvider()
        {
        }
	virtual void provideStreamData(int objid, int generation,
				       Pipeline* pipeline);
        void registerForeignStream(QPDFObjGen const& local_og,
                                   QPDFObjectHandle foreign_stream);

      private:
        std::map<QPDFObjGen, QPDFObjectHandle> foreign_streams;
    };

    class StringDecrypter: public QPDFObjectHandle::StringDecrypter
    {
        friend class QPDF;

      public:
        StringDecrypter(QPDF* qpdf, int objid, int gen);
        virtual ~StringDecrypter()
        {
        }
        virtual void decryptString(std::string& val);

      private:
        QPDF* qpdf;
        int objid;
        int gen;
    };

    class ResolveRecorder
    {
      public:
        ResolveRecorder(QPDF* qpdf, QPDFObjGen const& og) :
            qpdf(qpdf),
            og(og)
        {
            qpdf->m->resolving.insert(og);
        }
        virtual ~ResolveRecorder()
        {
            this->qpdf->m->resolving.erase(og);
        }
      private:
        QPDF* qpdf;
        QPDFObjGen og;
    };
    friend class ResolveRecorder;

    void parse(char const* password);
    void warn(QPDFExc const& e);
    void setTrailer(QPDFObjectHandle obj);
    void read_xref(qpdf_offset_t offset);
    void reconstruct_xref(QPDFExc& e);
    bool parse_xrefFirst(std::string const& line,
                         int& obj, int& num, int& bytes);
    bool parse_xrefEntry(std::string const& line,
                         qpdf_offset_t& f1, int& f2, char& type);
    qpdf_offset_t read_xrefTable(qpdf_offset_t offset);
    qpdf_offset_t read_xrefStream(qpdf_offset_t offset);
    qpdf_offset_t processXRefStream(
        qpdf_offset_t offset, QPDFObjectHandle& xref_stream);
    void insertXrefEntry(int obj, int f0, qpdf_offset_t f1, int f2,
			 bool overwrite = false);
    void setLastObjectDescription(std::string const& description,
				  int objid, int generation);
    QPDFObjectHandle readObject(
	PointerHolder<InputSource>, std::string const& description,
	int objid, int generation, bool in_object_stream);
    size_t recoverStreamLength(
	PointerHolder<InputSource> input, int objid, int generation,
	qpdf_offset_t stream_offset);
    QPDFTokenizer::Token readToken(PointerHolder<InputSource>,
                                   size_t max_len = 0);

    QPDFObjectHandle readObjectAtOffset(
	bool attempt_recovery,
	qpdf_offset_t offset, std::string const& description,
	int exp_objid, int exp_generation,
	int& act_objid, int& act_generation);
    PointerHolder<QPDFObject> resolve(int objid, int generation);
    void resolveObjectsInStream(int obj_stream_number);
    void findAttachmentStreams();

    // Calls finish() on the pipeline when done but does not delete it
    bool pipeStreamData(int objid, int generation,
			qpdf_offset_t offset, size_t length,
			QPDFObjectHandle dict,
			Pipeline* pipeline,
                        bool suppress_warnings,
                        bool will_retry);

    // For QPDFWriter:

    // Get lists of all objects in order according to the part of a
    // linearized file that they belong to.
    void getLinearizedParts(
	std::map<int, int> const& object_stream_data,
	std::vector<QPDFObjectHandle>& part4,
	std::vector<QPDFObjectHandle>& part6,
	std::vector<QPDFObjectHandle>& part7,
	std::vector<QPDFObjectHandle>& part8,
	std::vector<QPDFObjectHandle>& part9);

    void generateHintStream(std::map<int, QPDFXRefEntry> const& xref,
			    std::map<int, qpdf_offset_t> const& lengths,
			    std::map<int, int> const& obj_renumber,
			    PointerHolder<Buffer>& hint_stream,
			    int& S, int& O);

    // Map object to object stream that contains it
    void getObjectStreamData(std::map<int, int>&);

    // Get a list of objects that would be permitted in an object
    // stream.
    std::vector<QPDFObjGen> getCompressibleObjGens();

    // methods to support page handling

    void getAllPagesInternal(QPDFObjectHandle cur_pages,
			     std::vector<QPDFObjectHandle>& result);
    void getAllPagesInternal2(QPDFObjectHandle cur_pages,
                              std::vector<QPDFObjectHandle>& result,
                              std::set<QPDFObjGen>& visited);
    void insertPage(QPDFObjectHandle newpage, int pos);
    int findPage(QPDFObjGen const& og);
    int findPage(QPDFObjectHandle& page);
    void flattenPagesTree();
    void insertPageobjToPage(QPDFObjectHandle const& obj, int pos,
                             bool check_duplicate);

    // methods to support encryption -- implemented in QPDF_encryption.cc
    encryption_method_e interpretCF(QPDFObjectHandle);
    void initializeEncryption();
    std::string getKeyForObject(int objid, int generation, bool use_aes);
    void decryptString(std::string&, int objid, int generation);
    static std::string compute_encryption_key_from_password(
        std::string const& password, EncryptionData const& data);
    static std::string recover_encryption_key_with_password(
        std::string const& password, EncryptionData const& data);
    static std::string recover_encryption_key_with_password(
        std::string const& password, EncryptionData const& data,
        bool& perms_valid);
    void decryptStream(
	Pipeline*& pipeline, int objid, int generation,
	QPDFObjectHandle& stream_dict,
	std::vector<PointerHolder<Pipeline> >& heap);

    // Methods to support object copying
    QPDFObjectHandle copyForeignObject(
        QPDFObjectHandle foreign, bool allow_page);
    void reserveObjects(QPDFObjectHandle foreign, ObjCopier& obj_copier,
                        bool top);
    QPDFObjectHandle replaceForeignIndirectObjects(
        QPDFObjectHandle foreign, ObjCopier& obj_copier, bool top);

    // Linearization Hint table structures.
    // Naming conventions:

    // HSomething is the Something Hint Table or table header
    // HSomethingEntry is an entry in the Something table

    // delta_something + min_something = something
    // nbits_something = number of bits required for something

    // something_offset is the pre-adjusted offset in the file.  If >=
    // H0_offset, H0_length must be added to get an actual file
    // offset.

    // PDF 1.4: Table F.4
    struct HPageOffsetEntry
    {
	HPageOffsetEntry() :
	    delta_nobjects(0),
	    delta_page_length(0),
	    nshared_objects(0),
	    delta_content_offset(0),
	    delta_content_length(0)
	{
	}

	int delta_nobjects;			  // 1
	qpdf_offset_t delta_page_length;          // 2
	int nshared_objects;			  // 3
	// vectors' sizes = nshared_objects
	std::vector<int> shared_identifiers;	  // 4
	std::vector<int> shared_numerators;	  // 5
	qpdf_offset_t delta_content_offset;       // 6
        qpdf_offset_t delta_content_length;       // 7
    };

    // PDF 1.4: Table F.3
    struct HPageOffset
    {
	HPageOffset() :
	    min_nobjects(0),
	    first_page_offset(0),
	    nbits_delta_nobjects(0),
	    min_page_length(0),
	    nbits_delta_page_length(0),
	    min_content_offset(0),
	    nbits_delta_content_offset(0),
	    min_content_length(0),
	    nbits_delta_content_length(0),
	    nbits_nshared_objects(0),
	    nbits_shared_identifier(0),
	    nbits_shared_numerator(0),
	    shared_denominator(0)
	{
	}

	int min_nobjects;			  // 1
	qpdf_offset_t first_page_offset;          // 2
	int nbits_delta_nobjects;		  // 3
	int min_page_length;			  // 4
	int nbits_delta_page_length;		  // 5
	int min_content_offset;			  // 6
	int nbits_delta_content_offset;		  // 7
	int min_content_length;			  // 8
	int nbits_delta_content_length;		  // 9
	int nbits_nshared_objects;		  // 10
	int nbits_shared_identifier;		  // 11
	int nbits_shared_numerator;		  // 12
	int shared_denominator;			  // 13
	// vector size is npages
	std::vector<HPageOffsetEntry> entries;
    };

    // PDF 1.4: Table F.6
    struct HSharedObjectEntry
    {
	HSharedObjectEntry() :
	    delta_group_length(0),
	    signature_present(0),
	    nobjects_minus_one(0)
	{
	}

	// Item 3 is a 128-bit signature (unsupported by Acrobat)
	int delta_group_length;		 	  // 1
	int signature_present;			  // 2 -- always 0
	int nobjects_minus_one;			  // 4 -- always 0
    };

    // PDF 1.4: Table F.5
    struct HSharedObject
    {
	HSharedObject() :
	    first_shared_obj(0),
	    first_shared_offset(0),
	    nshared_first_page(0),
	    nshared_total(0),
	    nbits_nobjects(0),
	    min_group_length(0),
	    nbits_delta_group_length(0)
	{
	}

	int first_shared_obj;			  // 1
	qpdf_offset_t first_shared_offset;        // 2
	int nshared_first_page;			  // 3
	int nshared_total;			  // 4
	int nbits_nobjects;			  // 5
	int min_group_length;			  // 6
	int nbits_delta_group_length;		  // 7
	// vector size is nshared_total
	std::vector<HSharedObjectEntry> entries;
    };

    // PDF 1.4: Table F.9
    struct HGeneric
    {
	HGeneric() :
	    first_object(0),
	    first_object_offset(0),
	    nobjects(0),
	    group_length(0)
	{
	}

	int first_object;			  // 1
	qpdf_offset_t first_object_offset;        // 2
	int nobjects;				  // 3
	int group_length;			  // 4
    };

    // Other linearization data structures

    // Initialized from Linearization Parameter dictionary
    struct LinParameters
    {
	LinParameters() :
	    file_size(0),
	    first_page_object(0),
	    first_page_end(0),
	    npages(0),
	    xref_zero_offset(0),
	    first_page(0),
	    H_offset(0),
	    H_length(0)
	{
	}

	qpdf_offset_t file_size;        // /L
	int first_page_object;          // /O
	qpdf_offset_t first_page_end;	// /E
	int npages;                     // /N
	qpdf_offset_t xref_zero_offset;	// /T
	int first_page;                 // /P
        qpdf_offset_t H_offset;		// offset of primary hint stream
	qpdf_offset_t H_length;		// length of primary hint stream
    };

    // Computed hint table value data structures.  These tables
    // contain the computed values on which the hint table values are
    // based.  They exclude things like number of bits and store
    // actual values instead of mins and deltas.  File offsets are
    // also absolute rather than being offset by the size of the
    // primary hint table.  We populate the hint table structures from
    // these during writing and compare the hint table values with
    // these during validation.  We ignore some values for various
    // reasons described in the code.  Those values are omitted from
    // these structures.  Note also that object numbers are object
    // numbers from the input file, not the output file.

    // Naming convention: CHSomething is analogous to HSomething
    // above.  "CH" is computed hint.

    struct CHPageOffsetEntry
    {
	CHPageOffsetEntry() :
	    nobjects(0),
	    nshared_objects(0)
	{
	}

	int nobjects;
	int nshared_objects;
	// vectors' sizes = nshared_objects
	std::vector<int> shared_identifiers;
    };

    struct CHPageOffset
    {
	// vector size is npages
	std::vector<CHPageOffsetEntry> entries;
    };

    struct CHSharedObjectEntry
    {
	CHSharedObjectEntry(int object) :
	    object(object)
	{
	}

	int object;
    };

    // PDF 1.4: Table F.5
    struct CHSharedObject
    {
	CHSharedObject() :
	    first_shared_obj(0),
	    nshared_first_page(0),
	    nshared_total(0)
	{
	}

	int first_shared_obj;
	int nshared_first_page;
	int nshared_total;
	// vector size is nshared_total
	std::vector<CHSharedObjectEntry> entries;
    };

    // No need for CHGeneric -- HGeneric is fine as is.


    // Data structures to support optimization -- implemented in
    // QPDF_optimization.cc

    class ObjUser
    {
      public:
	enum user_e
	{
	    ou_bad,
	    ou_page,
	    ou_thumb,
	    ou_trailer_key,
	    ou_root_key,
	    ou_root
	};

	// type is set to ou_bad
	ObjUser();

	// type must be ou_root
	ObjUser(user_e type);

	// type must be one of ou_page or ou_thumb
	ObjUser(user_e type, int pageno);

	// type must be one of ou_trailer_key or ou_root_key
	ObjUser(user_e type, std::string const& key);

	bool operator<(ObjUser const&) const;

	user_e ou_type;
	int pageno;		// if ou_page;
	std::string key;	// if ou_trailer_key or ou_root_key
    };

    class PatternFinder: public InputSource::Finder
    {
      public:
        PatternFinder(QPDF& qpdf, bool (QPDF::*checker)()) :
            qpdf(qpdf),
            checker(checker)
        {
        }
        virtual ~PatternFinder()
        {
        }
        virtual bool check()
        {
            return (this->qpdf.*checker)();
        }

      private:
        QPDF& qpdf;
        bool (QPDF::*checker)();
    };

    // Methods to support pattern finding
    bool findHeader();
    bool findStartxref();
    bool findEndstream();

    // methods to support linearization checking -- implemented in
    // QPDF_linearization.cc
    void readLinearizationData();
    bool checkLinearizationInternal();
    void dumpLinearizationDataInternal();
    QPDFObjectHandle readHintStream(
        Pipeline&, qpdf_offset_t offset, size_t length);
    void readHPageOffset(BitStream);
    void readHSharedObject(BitStream);
    void readHGeneric(BitStream, HGeneric&);
    qpdf_offset_t maxEnd(ObjUser const& ou);
    qpdf_offset_t getLinearizationOffset(QPDFObjGen const&);
    QPDFObjectHandle getUncompressedObject(
	QPDFObjectHandle&, std::map<int, int> const& object_stream_data);
    int lengthNextN(int first_object, int n,
		    std::list<std::string>& errors);
    void checkHPageOffset(std::list<std::string>& errors,
			  std::list<std::string>& warnings,
			  std::vector<QPDFObjectHandle> const& pages,
			  std::map<int, int>& idx_to_obj);
    void checkHSharedObject(std::list<std::string>& warnings,
			    std::list<std::string>& errors,
			    std::vector<QPDFObjectHandle> const& pages,
			    std::map<int, int>& idx_to_obj);
    void checkHOutlines(std::list<std::string>& warnings);
    void dumpHPageOffset();
    void dumpHSharedObject();
    void dumpHGeneric(HGeneric&);
    int adjusted_offset(int offset);
    QPDFObjectHandle objGenToIndirect(QPDFObjGen const&);
    void calculateLinearizationData(
	std::map<int, int> const& object_stream_data);
    void pushOutlinesToPart(
	std::vector<QPDFObjectHandle>& part,
	std::set<QPDFObjGen>& lc_outlines,
	std::map<int, int> const& object_stream_data);
    int outputLengthNextN(
	int in_object, int n,
	std::map<int, qpdf_offset_t> const& lengths,
	std::map<int, int> const& obj_renumber);
    void calculateHPageOffset(
	std::map<int, QPDFXRefEntry> const& xref,
	std::map<int, qpdf_offset_t> const& lengths,
	std::map<int, int> const& obj_renumber);
    void calculateHSharedObject(
	std::map<int, QPDFXRefEntry> const& xref,
	std::map<int, qpdf_offset_t> const& lengths,
	std::map<int, int> const& obj_renumber);
    void calculateHOutline(
	std::map<int, QPDFXRefEntry> const& xref,
	std::map<int, qpdf_offset_t> const& lengths,
	std::map<int, int> const& obj_renumber);
    void writeHPageOffset(BitWriter&);
    void writeHSharedObject(BitWriter&);
    void writeHGeneric(BitWriter&, HGeneric&);


    // Methods to support optimization

    void pushInheritedAttributesToPage(bool allow_changes,
                                       bool warn_skipped_keys);
    void pushInheritedAttributesToPageInternal(
	QPDFObjectHandle,
	std::map<std::string, std::vector<QPDFObjectHandle> >&,
	std::vector<QPDFObjectHandle>& all_pages,
	bool allow_changes, bool warn_skipped_keys);
    void pushInheritedAttributesToPageInternal2(
	QPDFObjectHandle,
	std::map<std::string, std::vector<QPDFObjectHandle> >&,
	std::vector<QPDFObjectHandle>& all_pages,
	bool allow_changes, bool warn_skipped_keys,
        std::set<QPDFObjGen>& visited);
    void updateObjectMaps(ObjUser const& ou, QPDFObjectHandle oh);
    void updateObjectMapsInternal(ObjUser const& ou, QPDFObjectHandle oh,
				  std::set<QPDFObjGen>& visited, bool top);
    void filterCompressedObjects(std::map<int, int> const& object_stream_data);

    class Members
    {
        friend class QPDF;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members();
        Members(Members const&);

        QPDFTokenizer tokenizer;
        PointerHolder<InputSource> file;
        std::string last_object_description;
        bool provided_password_is_hex_key;
        bool encrypted;
        bool encryption_initialized;
        bool ignore_xref_streams;
        bool suppress_warnings;
        std::ostream* out_stream;
        std::ostream* err_stream;
        bool attempt_recovery;
        int encryption_V;
        int encryption_R;
        bool encrypt_metadata;
        std::map<std::string, encryption_method_e> crypt_filters;
        encryption_method_e cf_stream;
        encryption_method_e cf_string;
        encryption_method_e cf_file;
        std::string provided_password;
        std::string user_password;
        std::string encryption_key;
        std::string cached_object_encryption_key;
        int cached_key_objid;
        int cached_key_generation;
        std::string pdf_version;
        std::map<QPDFObjGen, QPDFXRefEntry> xref_table;
        std::set<int> deleted_objects;
        std::map<QPDFObjGen, ObjCache> obj_cache;
        std::set<QPDFObjGen> resolving;
        QPDFObjectHandle trailer;
        std::vector<QPDFObjectHandle> all_pages;
        std::map<QPDFObjGen, int> pageobj_to_pages_pos;
        bool pushed_inherited_attributes_to_pages;
        std::vector<QPDFExc> warnings;
        std::map<QPDF*, ObjCopier> object_copiers;
        PointerHolder<QPDFObjectHandle::StreamDataProvider> copied_streams;
        // copied_stream_data_provider is owned by copied_streams
        CopiedStreamDataProvider* copied_stream_data_provider;
        std::set<QPDFObjGen> attachment_streams;
        bool reconstructed_xref;

        // Linearization data
        qpdf_offset_t first_xref_item_offset; // actual value from file
        bool uncompressed_after_compressed;

        // Linearization parameter dictionary and hint table data: may be
        // read from file or computed prior to writing a linearized file
        QPDFObjectHandle lindict;
        LinParameters linp;
        HPageOffset page_offset_hints;
        HSharedObject shared_object_hints;
        HGeneric outline_hints;

        // Computed linearization data: used to populate above tables
        // during writing and to compare with them during validation.
        // c_ means computed.
        LinParameters c_linp;
        CHPageOffset c_page_offset_data;
        CHSharedObject c_shared_object_data;
        HGeneric c_outline_data;

        // Object ordering data for linearized files: initialized by
        // calculateLinearizationData().  Part numbers refer to the PDF
        // 1.4 specification.
        std::vector<QPDFObjectHandle> part4;
        std::vector<QPDFObjectHandle> part6;
        std::vector<QPDFObjectHandle> part7;
        std::vector<QPDFObjectHandle> part8;
        std::vector<QPDFObjectHandle> part9;

        // Optimization data
        std::map<ObjUser, std::set<QPDFObjGen> > obj_user_to_objects;
        std::map<QPDFObjGen, std::set<ObjUser> > object_to_obj_users;
    };

    // Keep all member variables inside the Members object, which we
    // dynamically allocate. This makes it possible to add new private
    // members without breaking binary compatibility.
    PointerHolder<Members> m;
};

#endif // QPDF_HH
