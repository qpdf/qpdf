// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

// This class implements a simple writer for saving QPDF objects to
// new PDF files.  See comments through the header file for additional
// details.

#ifndef __QPDFWRITER_HH__
#define __QPDFWRITER_HH__

#include <stdio.h>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>

#include <qpdf/DLL.h>
#include <qpdf/Constants.h>

#include <qpdf/QPDFXRefEntry.hh>

#include <qpdf/PointerHolder.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Buffer.hh>

class QPDF;
class QPDFObjectHandle;
class Pl_Count;

class DLL_EXPORT QPDFWriter
{
  public:
    // Passing null as filename means write to stdout.  QPDFWriter
    // will create a zero-length output file upon construction.  If
    // write fails, the empty or partially written file will not be
    // deleted.  This is by design: sometimes the partial file may be
    // useful for tracking down problems.  If your application doesn't
    // want the partially written file to be left behind, you should
    // delete it the eventual call to write fails.
    QPDFWriter(QPDF& pdf, char const* filename);
    ~QPDFWriter();

    // Set the value of object stream mode.  In disable mode, we never
    // generate any object streams.  In preserve mode, we preserve
    // object stream structure from the original file.  In generate
    // mode, we generate our own object streams.  In all cases, we
    // generate a conventional cross-reference table if there are no
    // object streams and a cross-reference stream if there are object
    // streams.  The default is o_preserve.
    void setObjectStreamMode(qpdf_object_stream_e);

    // Set value of stream data mode.  In uncompress mode, we attempt
    // to uncompress any stream that we can.  In preserve mode, we
    // preserve any filtering applied to streams.  In compress mode,
    // if we can apply all filters and the stream is not already
    // optimally compressed, recompress the stream.
    void setStreamDataMode(qpdf_stream_data_e);

    // Set value of content stream normalization.  The default is
    // "false".  If true, we attempt to normalize newlines inside of
    // content streams.  Some constructs such as inline images may
    // thwart our efforts.  There may be some cases where this can
    // damage the content stream.  This flag should be used only for
    // debugging and experimenting with PDF content streams.  Never
    // use it for production files.
    void setContentNormalization(bool);

    // Set QDF mode.  QDF mode causes special "pretty printing" of
    // PDF objects, adds comments for easier perusing of files.
    // Resulting PDF files can be edited in a text editor and then run
    // through fix-qdf to update cross reference tables and stream
    // lengths.
    void setQDFMode(bool);

    // Set the minimum PDF version.  If the PDF version of the input
    // file (or previously set minimum version) is less than the
    // version passed to this method, the PDF version of the output
    // file will be set to this value.  If the original PDF file's
    // version or previously set minimum version is already this
    // version or later, the original file's version will be used.
    // QPDFWriter automatically sets the minimum version to 1.4 when
    // R3 encryption parameters are used, and to 1.5 when object
    // streams are used.
    void setMinimumPDFVersion(std::string const&);

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
    void forcePDFVersion(std::string const&);

    // Cause a static /ID value to be generated.  Use only in test
    // suites.
    void setStaticID(bool);

    // Use a fixed initialization vector for AES-CBC encryption.  This
    // is not secure.  It should be used only in test suites for
    // creating predictable encrypted output.
    void setStaticAesIV(bool);

    // Suppress inclusion of comments indicating original object IDs
    // when writing QDF files.  This can also be useful for testing,
    // particularly when using comparison of two qdf files to
    // determine whether two PDF files have identical content.
    void setSuppressOriginalObjectIDs(bool);

    // Preserve encryption.  The default is true unless prefilering,
    // content normalization, or qdf mode has been selected in which
    // case encryption is never preserved.  Encryption is also not
    // preserved if we explicitly set encryption parameters.
    void setPreserveEncryption(bool);

    // Set up for encrypted output.  Disables stream prefiltering and
    // content normalization.  Note that setting R2 encryption
    // parameters sets the PDF version to at least 1.3, setting R3
    // encryption parameters pushes the PDF version number to at least
    // 1.4, and setting R4 parameters pushes the version to at least
    // 1.5, or if AES is used, 1.6.
    void setR2EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_print, bool allow_modify,
	bool allow_extract, bool allow_annotate);
    void setR3EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_accessibility, bool allow_extract,
	qpdf_r3_print_e print, qpdf_r3_modify_e modify);
    void setR4EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_accessibility, bool allow_extract,
	qpdf_r3_print_e print, qpdf_r3_modify_e modify,
	bool encrypt_metadata, bool use_aes);

    // Create linearized output.  Disables qdf mode, content
    // normalization, and stream prefiltering.
    void setLinearization(bool);

    void write();

  private:
    // flags used by unparseObject
    static int const f_stream = 	1 << 0;
    static int const f_filtered =	1 << 1;
    static int const f_in_ostream =     1 << 2;

    enum trailer_e { t_normal, t_lin_first, t_lin_second };

    int bytesNeeded(unsigned long n);
    void writeBinary(unsigned long val, unsigned int bytes);
    void writeString(std::string const& str);
    void writeBuffer(PointerHolder<Buffer>&);
    void writeStringQDF(std::string const& str);
    void writeStringNoQDF(std::string const& str);
    void assignCompressedObjectNumbers(int objid);
    void enqueueObject(QPDFObjectHandle object);
    void writeObjectStreamOffsets(std::vector<int>& offsets, int first_obj);
    void writeObjectStream(QPDFObjectHandle object);
    void writeObject(QPDFObjectHandle object, int object_stream_index = -1);
    void writeTrailer(trailer_e which, int size,
		      bool xref_stream, int prev = 0);
    void unparseObject(QPDFObjectHandle object, int level,
		       unsigned int flags);
    void unparseObject(QPDFObjectHandle object, int level,
		       unsigned int flags,
		       // for stream dictionaries
		       int stream_length, bool compress);
    void unparseChild(QPDFObjectHandle child, int level, int flags);
    void initializeSpecialStreams();
    void preserveObjectStreams();
    void generateObjectStreams();
    void generateID();
    void interpretR3EncryptionParameters(
	std::set<int>& bits_to_clear,
	char const* user_password, char const* owner_password,
	bool allow_accessibility, bool allow_extract,
	qpdf_r3_print_e print, qpdf_r3_modify_e modify);
    void disableIncompatbleEncryption(float v);
    void setEncryptionParameters(
	char const* user_password, char const* owner_password,
	int V, int R, int key_len, std::set<int>& bits_to_clear);
    void setEncryptionParametersInternal(
	int V, int R, int key_len, long P,
	std::string const& O, std::string const& U,
	std::string const& id1, std::string const& user_password);
    void copyEncryptionParameters();
    void setDataKey(int objid);
    int openObject(int objid = 0);
    void closeObject(int objid);
    void writeStandard();
    void writeLinearized();
    void enqueuePart(std::vector<QPDFObjectHandle>& part);
    void writeEncryptionDictionary();
    void writeHeader();
    void writeHintStream(int hint_id);
    int writeXRefTable(trailer_e which, int first, int last, int size);
    int writeXRefTable(trailer_e which, int first, int last, int size,
		       // for linearization
		       int prev,
		       bool suppress_offsets,
		       int hint_id,
		       int hint_offset,
		       int hint_length);
    int writeXRefStream(int objid, int max_id, int max_offset,
			trailer_e which, int first, int last, int size);
    int writeXRefStream(int objid, int max_id, int max_offset,
			trailer_e which, int first, int last, int size,
			// for linearization
			int prev,
			int hint_id,
			int hint_offset,
			int hint_length);

    // When filtering subsections, push additional pipelines to the
    // stack.  When ready to switch, activate the pipeline stack.
    // Pipelines passed to pushPipeline are deleted when
    // clearPipelineStack is called.
    Pipeline* pushPipeline(Pipeline*);
    void activatePipelineStack();

    // Calls finish on the current pipeline and pops the pipeline
    // stack until the top of stack is a previous active top of stack,
    // and restores the pipeline to that point.  Deletes any pipelines
    // that it pops.  If the bp argument is non-null and any of the
    // stack items are of type Pl_Buffer, the buffer is retrieved.
    void popPipelineStack(PointerHolder<Buffer>* bp = 0);

    void adjustAESStreamLength(unsigned long& length);
    void pushEncryptionFilter();
    void pushDiscardFilter();

    QPDF& pdf;
    char const* filename;
    FILE* file;
    bool close_file;
    bool normalize_content_set;
    bool normalize_content;
    bool stream_data_mode_set;
    qpdf_stream_data_e stream_data_mode;
    bool qdf_mode;
    bool static_id;
    bool suppress_original_object_ids;
    bool direct_stream_lengths;
    bool encrypted;
    bool preserve_encryption;
    bool linearized;
    qpdf_object_stream_e object_stream_mode;
    std::string encryption_key;
    bool encrypt_metadata;
    bool encrypt_use_aes;
    std::map<std::string, std::string> encryption_dictionary;

    std::string id1;		// for /ID key of
    std::string id2;		// trailer dictionary
    std::string min_pdf_version;
    std::string forced_pdf_version;
    int encryption_dict_objid;
    std::string cur_data_key;
    std::list<PointerHolder<Pipeline> > to_delete;
    Pl_Count* pipeline;
    std::list<QPDFObjectHandle> object_queue;
    std::map<int, int> obj_renumber;
    std::map<int, QPDFXRefEntry> xref;
    std::map<int, size_t> lengths;
    int next_objid;
    int cur_stream_length_id;
    unsigned long cur_stream_length;
    bool added_newline;
    int max_ostream_index;
    std::set<int> normalized_streams;
    std::map<int, int> page_object_to_seq;
    std::map<int, int> contents_to_page_seq;
    std::map<int, int> object_to_object_stream;
    std::map<int, std::set<int> > object_stream_to_objects;
    std::list<Pipeline*> pipeline_stack;
};

#endif // __QPDFWRITER_HH__
