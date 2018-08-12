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

// This class implements a simple writer for saving QPDF objects to
// new PDF files.  See comments through the header file for additional
// details.

#ifndef QPDFWRITER_HH
#define QPDFWRITER_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>

#include <stdio.h>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>

#include <qpdf/Constants.h>

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFXRefEntry.hh>

#include <qpdf/Pl_Buffer.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Buffer.hh>

class QPDF;
class Pl_Count;
class Pl_MD5;

class QPDFWriter
{
  public:
    // Construct a QPDFWriter object without specifying output.  You
    // must call one of the output setting routines defined below.
    QPDF_DLL
    QPDFWriter(QPDF& pdf);

    // Create a QPDFWriter object that writes its output to a file or
    // to stdout.  This is equivalent to using the previous
    // constructor and then calling setOutputFilename().  See
    // setOutputFilename() for details.
    QPDF_DLL
    QPDFWriter(QPDF& pdf, char const* filename);

    // Create a QPDFWriter object that writes its output to an already
    // open FILE*.  This is equivalent to calling the first
    // constructor and then calling setOutputFile().  See
    // setOutputFile() for details.
    QPDF_DLL
    QPDFWriter(QPDF& pdf, char const* description, FILE* file, bool close_file);

    QPDF_DLL
    ~QPDFWriter();

    class ProgressReporter
    {
      public:
        virtual ~ProgressReporter()
        {
        }

        // This method is called with a value from 0 to 100 to
        // indicate approximate progress through the write process.
        // See registerProgressReporter.
        virtual void reportProgress(int) = 0;
    };

    // Setting Output.  Output may be set only one time.  If you don't
    // use the filename version of the QPDFWriter constructor, you
    // must call exactly one of these methods.

    // Passing null as filename means write to stdout.  QPDFWriter
    // will create a zero-length output file upon construction.  If
    // write fails, the empty or partially written file will not be
    // deleted.  This is by design: sometimes the partial file may be
    // useful for tracking down problems.  If your application doesn't
    // want the partially written file to be left behind, you should
    // delete it the eventual call to write fails.
    QPDF_DLL
    void setOutputFilename(char const* filename);

    // Write to the given FILE*, which must be opened by the caller.
    // If close_file is true, QPDFWriter will close the file.
    // Otherwise, the caller must close the file.  The file does not
    // need to be seekable; it will be written to in a single pass.
    // It must be open in binary mode.
    QPDF_DLL
    void setOutputFile(char const* description, FILE* file, bool close_file);

    // Indicate that QPDFWriter should create a memory buffer to
    // contain the final PDF file.  Obtain the memory by calling
    // getBuffer().
    QPDF_DLL
    void setOutputMemory();

    // Return the buffer object containing the PDF file.  If
    // setOutputMemory() has been called, this method may be called
    // exactly one time after write() has returned.  The caller is
    // responsible for deleting the buffer when done.
    QPDF_DLL
    Buffer* getBuffer();

    // Supply your own pipeline object.  Output will be written to
    // this pipeline, and QPDFWriter will call finish() on the
    // pipeline.  It is the caller's responsibility to manage the
    // memory for the pipeline.  The pipeline is never deleted by
    // QPDFWriter, which makes it possible for you to call additional
    // methods on the pipeline after the writing is finished.
    QPDF_DLL
    void setOutputPipeline(Pipeline*);

    // Setting Parameters

    // Set the value of object stream mode.  In disable mode, we never
    // generate any object streams.  In preserve mode, we preserve
    // object stream structure from the original file.  In generate
    // mode, we generate our own object streams.  In all cases, we
    // generate a conventional cross-reference table if there are no
    // object streams and a cross-reference stream if there are object
    // streams.  The default is o_preserve.
    QPDF_DLL
    void setObjectStreamMode(qpdf_object_stream_e);

    // Set value of stream data mode. This is an older interface.
    // Instead of using this, prefer setCompressStreams() and
    // setDecodeLevel(). This method is retained for compatibility,
    // but it does not cover the full range of available
    // configurations. The mapping between this and the new methods is
    // as follows:
    //
    // qpdf_s_uncompress:
    //   setCompressStreams(false)
    //   setDecodeLevel(qpdf_dl_generalized)
    // qpdf_s_preserve:
    //   setCompressStreams(false)
    //   setDecodeLevel(qpdf_dl_none)
    // qpdf_s_compress:
    //   setCompressStreams(true)
    //   setDecodeLevel(qpdf_dl_generalized)
    //
    // The default is qpdf_s_compress.
    QPDF_DLL
    void setStreamDataMode(qpdf_stream_data_e);

    // If true, compress any uncompressed streams when writing them.
    // Metadata streams are a special case and are not compressed even
    // if this is true. This is true by default for QPDFWriter. If you
    // want QPDFWriter to leave uncompressed streams uncompressed,
    // pass false to this method.
    QPDF_DLL
    void setCompressStreams(bool);

    // When QPDFWriter encounters streams, this parameter controls the
    // behavior with respect to attempting to apply any filters to the
    // streams when copying to the output. The decode levels are as
    // follows:
    //
    // qpdf_dl_none: Do not attempt to apply any filters. Streams
    // remain as they appear in the original file. Note that
    // uncompressed streams may still be compressed on output. You can
    // disable that by calling setCompressStreams(false).
    //
    // qpdf_dl_generalized: This is the default. QPDFWriter will apply
    // LZWDecode, ASCII85Decode, ASCIIHexDecode, and FlateDecode
    // filters on the input. When combined with
    // setCompressStreams(true), which the default, the effect of this
    // is that streams filtered with these older and less efficient
    // filters will be recompressed with the Flate filter. As a
    // special case, if a stream is already compressed with
    // FlateDecode and setCompressStreams is enabled, the original
    // compressed data will be preserved.
    //
    // qpdf_dl_specialized: In addition to uncompressing the
    // generalized compression formats, supported non-lossy
    // compression will also be be decoded. At present, this includes
    // the RunLengthDecode filter.
    //
    // qpdf_dl_all: In addition to generalized and non-lossy
    // specialized filters, supported lossy compression filters will
    // be applied. At present, this includes DCTDecode (JPEG)
    // compression. Note that compressing the resulting data with
    // DCTDecode again will accumulate loss, so avoid multiple
    // compression and decompression cycles. This is mostly useful for
    // retrieving image data.
    QPDF_DLL
    void setDecodeLevel(qpdf_stream_decode_level_e);

    // Set value of content stream normalization.  The default is
    // "false".  If true, we attempt to normalize newlines inside of
    // content streams.  Some constructs such as inline images may
    // thwart our efforts.  There may be some cases where this can
    // damage the content stream.  This flag should be used only for
    // debugging and experimenting with PDF content streams.  Never
    // use it for production files.
    QPDF_DLL
    void setContentNormalization(bool);

    // Set QDF mode.  QDF mode causes special "pretty printing" of
    // PDF objects, adds comments for easier perusing of files.
    // Resulting PDF files can be edited in a text editor and then run
    // through fix-qdf to update cross reference tables and stream
    // lengths.
    QPDF_DLL
    void setQDFMode(bool);

    // Preserve unreferenced objects. The default behavior is to
    // discard any object that is not visited during a traversal of
    // the object structure from the trailer.
    QPDF_DLL
    void setPreserveUnreferencedObjects(bool);

    // Always write a newline before the endstream keyword. This helps
    // with PDF/A compliance, though it is not sufficient for it.
    QPDF_DLL
    void setNewlineBeforeEndstream(bool);

    // Set the minimum PDF version.  If the PDF version of the input
    // file (or previously set minimum version) is less than the
    // version passed to this method, the PDF version of the output
    // file will be set to this value.  If the original PDF file's
    // version or previously set minimum version is already this
    // version or later, the original file's version will be used.
    // QPDFWriter automatically sets the minimum version to 1.4 when
    // R3 encryption parameters are used, and to 1.5 when object
    // streams are used.
    QPDF_DLL
    void setMinimumPDFVersion(std::string const&);
    QPDF_DLL
    void setMinimumPDFVersion(std::string const&, int extension_level);

    // Force the PDF version of the output file to be a given version.
    // Use of this function may create PDF files that will not work
    // properly with older PDF viewers.  When a PDF version is set
    // using this function, qpdf will use this version even if the
    // file contains features that are not supported in that version
    // of PDF.  In other words, you should only use this function if
    // you are sure the PDF file in question has no features of newer
    // versions of PDF or if you are willing to create files that old
    // viewers may try to open but not be able to properly interpret.
    // If any encryption has been applied to the document either
    // explicitly or by preserving the encryption of the source
    // document, forcing the PDF version to a value too low to support
    // that type of encryption will explicitly disable decryption.
    // Additionally, forcing to a version below 1.5 will disable
    // object streams.
    QPDF_DLL
    void forcePDFVersion(std::string const&);
    QPDF_DLL
    void forcePDFVersion(std::string const&, int extension_level);

    // Provide additional text to insert in the PDF file somewhere
    // near the beginning of the file.  This can be used to add
    // comments to the beginning of a PDF file, for example, if those
    // comments are to be consumed by some other application.  No
    // checks are performed to ensure that the text inserted here is
    // valid PDF.  If you want to insert multiline comments, you will
    // need to include \n in the string yourself and start each line
    // with %.  An extra newline will be appended if one is not
    // already present at the end of your text.
    QPDF_DLL
    void setExtraHeaderText(std::string const&);

    // Causes a deterministic /ID value to be generated. When this is
    // set, the current time and output file name are not used as part
    // of /ID generation. Instead, a digest of all significant parts
    // of the output file's contents is included in the /ID
    // calculation. Use of a deterministic /ID can be handy when it is
    // desirable for a repeat of the same qpdf operation on the same
    // inputs being written to the same outputs with the same
    // parameters to generate exactly the same results. This feature
    // is incompatible with encrypted files because, for encrypted
    // files, the /ID is generated before any part of the file is
    // written since it is an input to the encryption process.
    QPDF_DLL
    void setDeterministicID(bool);

    // Cause a static /ID value to be generated.  Use only in test
    // suites.  See also setDeterministicID.
    QPDF_DLL
    void setStaticID(bool);

    // Use a fixed initialization vector for AES-CBC encryption.  This
    // is not secure.  It should be used only in test suites for
    // creating predictable encrypted output.
    QPDF_DLL
    void setStaticAesIV(bool);

    // Suppress inclusion of comments indicating original object IDs
    // when writing QDF files.  This can also be useful for testing,
    // particularly when using comparison of two qdf files to
    // determine whether two PDF files have identical content.
    QPDF_DLL
    void setSuppressOriginalObjectIDs(bool);

    // Preserve encryption.  The default is true unless prefilering,
    // content normalization, or qdf mode has been selected in which
    // case encryption is never preserved.  Encryption is also not
    // preserved if we explicitly set encryption parameters.
    QPDF_DLL
    void setPreserveEncryption(bool);

    // Copy encryption parameters from another QPDF object.  If you
    // want to copy encryption from the object you are writing, call
    // setPreserveEncryption(true) instead.
    QPDF_DLL
    void copyEncryptionParameters(QPDF&);

    // Set up for encrypted output.  User and owner password both must
    // be specified.  Either or both may be the empty string.  Note
    // that qpdf does not apply any special treatment to the empty
    // string, which makes it possible to create encrypted files with
    // empty owner passwords and non-empty user passwords or with the
    // same password for both user and owner.  Some PDF reading
    // products don't handle such files very well.  Enabling
    // encryption disables stream prefiltering and content
    // normalization.  Note that setting R2 encryption parameters sets
    // the PDF version to at least 1.3, setting R3 encryption
    // parameters pushes the PDF version number to at least 1.4,
    // setting R4 parameters pushes the version to at least 1.5, or if
    // AES is used, 1.6, and setting R5 or R6 parameters pushes the
    // version to at least 1.7 with extension level 3.
    QPDF_DLL
    void setR2EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_print, bool allow_modify,
	bool allow_extract, bool allow_annotate);
    QPDF_DLL
    void setR3EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_accessibility, bool allow_extract,
	qpdf_r3_print_e print, qpdf_r3_modify_e modify);
    QPDF_DLL
    void setR4EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_accessibility, bool allow_extract,
	qpdf_r3_print_e print, qpdf_r3_modify_e modify,
	bool encrypt_metadata, bool use_aes);
    // R5 is deprecated.  Do not use it for production use.  Writing
    // R5 is supported by qpdf primarily to generate test files for
    // applications that may need to test R5 support.
    QPDF_DLL
    void setR5EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_accessibility, bool allow_extract,
	qpdf_r3_print_e print, qpdf_r3_modify_e modify,
	bool encrypt_metadata);
    QPDF_DLL
    void setR6EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_accessibility, bool allow_extract,
	qpdf_r3_print_e print, qpdf_r3_modify_e modify,
	bool encrypt_metadata_aes);

    // Create linearized output.  Disables qdf mode, content
    // normalization, and stream prefiltering.
    QPDF_DLL
    void setLinearization(bool);

    // For debugging QPDF: provide the name of a file to write pass1
    // of linearization to. The only reason to use this is to debug
    // QPDF. To linearize, QPDF writes out the file in two passes.
    // Usually the first pass is discarded, but lots of computations
    // are made in pass 1. If a linearized file comes out wrong, it
    // can be helpful to look at the first pass.
    QPDF_DLL
    void setLinearizationPass1Filename(std::string const&);

    // Create PCLm output. This is only useful for clients that know
    // how to create PCLm files. If a file is structured exactly as
    // PCLm requires, this call will tell QPDFWriter to write the PCLm
    // header, create certain unreferenced streams required by the
    // standard, and write the objects in the required order. Calling
    // this on an ordinary PDF serves no purpose. There is no
    // command-line argument that causes this method to be called.
    QPDF_DLL
    void setPCLm(bool);

    // If you want to be notified of progress, derive a class from
    // ProgressReporter and override the reportProgress method.
    QPDF_DLL
    void registerProgressReporter(PointerHolder<ProgressReporter>);

    QPDF_DLL
    void write();

  private:
    // flags used by unparseObject
    static int const f_stream = 	1 << 0;
    static int const f_filtered =	1 << 1;
    static int const f_in_ostream =     1 << 2;

    enum trailer_e { t_normal, t_lin_first, t_lin_second };

    int bytesNeeded(unsigned long long n);
    void writeBinary(unsigned long long val, unsigned int bytes);
    void writeString(std::string const& str);
    void writeBuffer(PointerHolder<Buffer>&);
    void writeStringQDF(std::string const& str);
    void writeStringNoQDF(std::string const& str);
    void writePad(int nspaces);
    void assignCompressedObjectNumbers(QPDFObjGen const& og);
    void enqueueObject(QPDFObjectHandle object);
    void writeObjectStreamOffsets(
        std::vector<qpdf_offset_t>& offsets, int first_obj);
    void writeObjectStream(QPDFObjectHandle object);
    void writeObject(QPDFObjectHandle object, int object_stream_index = -1);
    void writeTrailer(trailer_e which, int size,
		      bool xref_stream, qpdf_offset_t prev,
                      int linearization_pass);
    void unparseObject(QPDFObjectHandle object, int level,
		       unsigned int flags);
    void unparseObject(QPDFObjectHandle object, int level,
		       unsigned int flags,
		       // for stream dictionaries
		       size_t stream_length, bool compress);
    void unparseChild(QPDFObjectHandle child, int level, int flags);
    void initializeSpecialStreams();
    void preserveObjectStreams();
    void generateObjectStreams();
    std::string getOriginalID1();
    void generateID();
    void interpretR3EncryptionParameters(
	std::set<int>& bits_to_clear,
	char const* user_password, char const* owner_password,
	bool allow_accessibility, bool allow_extract,
	qpdf_r3_print_e print, qpdf_r3_modify_e modify);
    void disableIncompatibleEncryption(int major, int minor,
                                       int extension_level);
    void parseVersion(std::string const& version, int& major, int& minor) const;
    int compareVersions(int major1, int minor1, int major2, int minor2) const;
    void setEncryptionParameters(
	char const* user_password, char const* owner_password,
	int V, int R, int key_len, std::set<int>& bits_to_clear);
    void setEncryptionParametersInternal(
	int V, int R, int key_len, long P,
	std::string const& O, std::string const& U,
	std::string const& OE, std::string const& UE, std::string const& Perms,
	std::string const& id1, std::string const& user_password,
        std::string const& encryption_key);
    void setDataKey(int objid);
    int openObject(int objid = 0);
    void closeObject(int objid);
    QPDFObjectHandle getTrimmedTrailer();
    void prepareFileForWrite();
    void enqueueObjectsStandard();
    void enqueueObjectsPCLm();
    void indicateProgress(bool decrement, bool finished);
    void writeStandard();
    void writeLinearized();
    void enqueuePart(std::vector<QPDFObjectHandle>& part);
    void writeEncryptionDictionary();
    void writeHeader();
    void writeHintStream(int hint_id);
    qpdf_offset_t writeXRefTable(
        trailer_e which, int first, int last, int size);
    qpdf_offset_t writeXRefTable(
        trailer_e which, int first, int last, int size,
        // for linearization
        qpdf_offset_t prev,
        bool suppress_offsets,
        int hint_id,
        qpdf_offset_t hint_offset,
        qpdf_offset_t hint_length,
        int linearization_pass);
    qpdf_offset_t writeXRefStream(
        int objid, int max_id, qpdf_offset_t max_offset,
        trailer_e which, int first, int last, int size);
    qpdf_offset_t writeXRefStream(
        int objid, int max_id, qpdf_offset_t max_offset,
        trailer_e which, int first, int last, int size,
        // for linearization
        qpdf_offset_t prev,
        int hint_id,
        qpdf_offset_t hint_offset,
        qpdf_offset_t hint_length,
        bool skip_compression,
        int linearization_pass);
    int calculateXrefStreamPadding(int xref_bytes);

    // When filtering subsections, push additional pipelines to the
    // stack.  When ready to switch, activate the pipeline stack.
    // Pipelines passed to pushPipeline are deleted when
    // clearPipelineStack is called.
    Pipeline* pushPipeline(Pipeline*);
    void activatePipelineStack();
    void initializePipelineStack(Pipeline *);

    // Calls finish on the current pipeline and pops the pipeline
    // stack until the top of stack is a previous active top of stack,
    // and restores the pipeline to that point.  Deletes any pipelines
    // that it pops.  If the bp argument is non-null and any of the
    // stack items are of type Pl_Buffer, the buffer is retrieved.
    void popPipelineStack(PointerHolder<Buffer>* bp = 0);

    void adjustAESStreamLength(size_t& length);
    void pushEncryptionFilter();
    void pushDiscardFilter();
    void pushMD5Pipeline();
    void computeDeterministicIDData();

    void discardGeneration(std::map<QPDFObjGen, int> const& in,
                           std::map<int, int>& out);

    class Members
    {
        friend class QPDFWriter;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members(QPDF& pdf);
        Members(Members const&);

        QPDF& pdf;
        char const* filename;
        FILE* file;
        bool close_file;
        Pl_Buffer* buffer_pipeline;
        Buffer* output_buffer;
        bool normalize_content_set;
        bool normalize_content;
        bool compress_streams;
        bool compress_streams_set;
        qpdf_stream_decode_level_e stream_decode_level;
        bool stream_decode_level_set;
        bool qdf_mode;
        bool preserve_unreferenced_objects;
        bool newline_before_endstream;
        bool static_id;
        bool suppress_original_object_ids;
        bool direct_stream_lengths;
        bool encrypted;
        bool preserve_encryption;
        bool linearized;
        bool pclm;
        qpdf_object_stream_e object_stream_mode;
        std::string encryption_key;
        bool encrypt_metadata;
        bool encrypt_use_aes;
        std::map<std::string, std::string> encryption_dictionary;
        int encryption_V;
        int encryption_R;

        std::string id1;		// for /ID key of
        std::string id2;		// trailer dictionary
        std::string final_pdf_version;
        int final_extension_level;
        std::string min_pdf_version;
        int min_extension_level;
        std::string forced_pdf_version;
        int forced_extension_level;
        std::string extra_header_text;
        int encryption_dict_objid;
        std::string cur_data_key;
        std::list<PointerHolder<Pipeline> > to_delete;
        Pl_Count* pipeline;
        std::list<QPDFObjectHandle> object_queue;
        std::map<QPDFObjGen, int> obj_renumber;
        std::map<int, QPDFXRefEntry> xref;
        std::map<int, qpdf_offset_t> lengths;
        int next_objid;
        int cur_stream_length_id;
        size_t cur_stream_length;
        bool added_newline;
        int max_ostream_index;
        std::set<QPDFObjGen> normalized_streams;
        std::map<QPDFObjGen, int> page_object_to_seq;
        std::map<QPDFObjGen, int> contents_to_page_seq;
        std::map<QPDFObjGen, int> object_to_object_stream;
        std::map<int, std::set<QPDFObjGen> > object_stream_to_objects;
        std::list<Pipeline*> pipeline_stack;
        bool deterministic_id;
        Pl_MD5* md5_pipeline;
        std::string deterministic_id_data;

        // For linearization only
        std::string lin_pass1_filename;
        std::map<int, int> obj_renumber_no_gen;
        std::map<int, int> object_to_object_stream_no_gen;

        // For progress reporting
        PointerHolder<ProgressReporter> progress_reporter;
        int events_expected;
        int events_seen;
        int next_progress_report;
    };

    // Keep all member variables inside the Members object, which we
    // dynamically allocate. This makes it possible to add new private
    // members without breaking binary compatibility.
    PointerHolder<Members> m;
};

#endif // QPDFWRITER_HH
