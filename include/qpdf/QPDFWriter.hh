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

#include <qpdf/DLL.hh>

#include <qpdf/QPDFXRefEntry.hh>

#include <qpdf/PointerHolder.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Buffer.hh>

class QPDF;
class QPDFObjectHandle;
class Pl_Count;

class QPDFWriter
{
  public:
    // Passing null as filename means write to stdout
    DLL_EXPORT
    QPDFWriter(QPDF& pdf, char const* filename);
    DLL_EXPORT
    ~QPDFWriter();

    // Set the value of object stream mode.  In disable mode, we never
    // generate any object streams.  In preserve mode, we preserve
    // object stream structure from the original file.  In generate
    // mode, we generate our own object streams.  In all cases, we
    // generate a conventional cross-reference table if there are no
    // object streams and a cross-reference stream if there are object
    // streams.  The default is o_preserve.
    enum object_stream_e { o_disable, o_preserve, o_generate };
    DLL_EXPORT
    void setObjectStreamMode(object_stream_e);

    // Set value of stream data mode.  In uncompress mode, we attempt
    // to uncompress any stream that we can.  In preserve mode, we
    // preserve any filtering applied to streams.  In compress mode,
    // if we can apply all filters and the stream is not already
    // optimally compressed, recompress the stream.
    enum stream_data_e { s_uncompress, s_preserve, s_compress };
    DLL_EXPORT
    void setStreamDataMode(stream_data_e);

    // Set value of content stream normalization.  The default is
    // "false".  If true, we attempt to normalize newlines inside of
    // content streams.  Some constructs such as inline images may
    // thwart our efforts.  There may be some cases where this can
    // damage the content stream.  This flag should be used only for
    // debugging and experimenting with PDF content streams.  Never
    // use it for production files.
    DLL_EXPORT
    void setContentNormalization(bool);

    // Set QDF mode.  QDF mode causes special "pretty printing" of
    // PDF objects, adds comments for easier perusing of files.
    // Resulting PDF files can be edited in a text editor and then run
    // through fix-qdf to update cross reference tables and stream
    // lengths.
    DLL_EXPORT
    void setQDFMode(bool);

    // Cause a static /ID value to be generated.  Use only in test
    // suites.
    DLL_EXPORT
    void setStaticID(bool);

    // Suppress inclusion of comments indicating original object IDs
    // when writing QDF files.  This can also be useful for testing,
    // particularly when using comparison of two qdf files to
    // determine whether two PDF files have identical content.
    DLL_EXPORT
    void setSuppressOriginalObjectIDs(bool);

    // Preserve encryption.  The default is true unless prefilering,
    // content normalization, or qdf mode has been selected in which
    // case encryption is never preserved.  Encryption is also not
    // preserved if we explicitly set encryption parameters.
    DLL_EXPORT
    void setPreserveEncryption(bool);

    // Set up for encrypted output.  Disables stream prefiltering and
    // content normalization.  Note that setting R2 encryption
    // parameters sets the PDF version to at least 1.3, and setting R3
    // encryption parameters pushes the PDF version number to at least
    // 1.4.
    DLL_EXPORT
    void setR2EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_print, bool allow_modify,
	bool allow_extract, bool allow_annotate);
    enum r3_print_e
    {
	r3p_full,		// allow all printing
	r3p_low,		// allow only low-resolution printing
	r3p_none		// allow no printing
    };
    enum r3_modify_e
    {
	r3m_all,		// allow all modification
	r3m_annotate,		// allow comment authoring and form operations
	r3m_form,		// allow form field fill-in or signing
	r3m_assembly,		// allow only document assembly
	r3m_none		// allow no modification
    };
    DLL_EXPORT
    void setR3EncryptionParameters(
	char const* user_password, char const* owner_password,
	bool allow_accessibility, bool allow_extract,
	r3_print_e print, r3_modify_e modify);

    // Create linearized output.  Disables qdf mode, content
    // normalization, and stream prefiltering.
    DLL_EXPORT
    void setLinearization(bool);

    DLL_EXPORT
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

    void pushEncryptionFilter();
    void pushDiscardFilter();

    QPDF& pdf;
    char const* filename;
    FILE* file;
    bool close_file;
    bool normalize_content_set;
    bool normalize_content;
    bool stream_data_mode_set;
    stream_data_e stream_data_mode;
    bool qdf_mode;
    bool static_id;
    bool suppress_original_object_ids;
    bool direct_stream_lengths;
    bool encrypted;
    bool preserve_encryption;
    bool linearized;
    object_stream_e object_stream_mode;
    std::string encryption_key;
    std::map<std::string, std::string> encryption_dictionary;

    std::string id1;		// for /ID key of
    std::string id2;		// trailer dictionary
    std::string min_pdf_version;
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
    int cur_stream_length;
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
