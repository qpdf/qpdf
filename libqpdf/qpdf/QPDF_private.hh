#ifndef QPDF_PRIVATE_HH
#define QPDF_PRIVATE_HH

#include <qpdf/QPDF.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFTokenizer_private.hh>

// Writer class is restricted to QPDFWriter so that only it can call certain methods.
class QPDF::Writer
{
    friend class QPDFWriter;

  private:
    static void
    optimize(
        QPDF& qpdf,
        QPDFWriter::ObjTable const& obj,
        std::function<int(QPDFObjectHandle&)> skip_stream_parameters)
    {
        qpdf.optimize(obj, skip_stream_parameters);
    }

    static void
    getLinearizedParts(
        QPDF& qpdf,
        QPDFWriter::ObjTable const& obj,
        std::vector<QPDFObjectHandle>& part4,
        std::vector<QPDFObjectHandle>& part6,
        std::vector<QPDFObjectHandle>& part7,
        std::vector<QPDFObjectHandle>& part8,
        std::vector<QPDFObjectHandle>& part9)
    {
        qpdf.getLinearizedParts(obj, part4, part6, part7, part8, part9);
    }

    static void
    generateHintStream(
        QPDF& qpdf,
        QPDFWriter::NewObjTable const& new_obj,
        QPDFWriter::ObjTable const& obj,
        std::string& hint_stream,
        int& S,
        int& O,
        bool compressed)
    {
        qpdf.generateHintStream(new_obj, obj, hint_stream, S, O, compressed);
    }

    static std::vector<QPDFObjGen>
    getCompressibleObjGens(QPDF& qpdf)
    {
        return qpdf.getCompressibleObjVector();
    }

    static std::vector<bool>
    getCompressibleObjSet(QPDF& qpdf)
    {
        return qpdf.getCompressibleObjSet();
    }

    static std::map<QPDFObjGen, QPDFXRefEntry> const&
    getXRefTable(QPDF& qpdf)
    {
        return qpdf.getXRefTableInternal();
    }

    static size_t
    tableSize(QPDF& qpdf)
    {
        return qpdf.tableSize();
    }
};

// The Resolver class is restricted to QPDFObject so that only it can resolve indirect
// references.
class QPDF::Resolver
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;

  private:
    static std::shared_ptr<QPDFObject> const&
    resolved(QPDF* qpdf, QPDFObjGen og)
    {
        return qpdf->resolve(og);
    }
};

// StreamCopier class is restricted to QPDFObjectHandle so it can copy stream data.
class QPDF::StreamCopier
{
    friend class QPDFObjectHandle;

  private:
    static void
    copyStreamData(QPDF* qpdf, QPDFObjectHandle const& dest, QPDFObjectHandle const& src)
    {
        qpdf->copyStreamData(dest, src);
    }
};

// The ParseGuard class allows QPDFParser to detect re-entrant parsing. It also provides
// special access to allow the parser to create unresolved objects and dangling references.
class QPDF::ParseGuard
{
    friend class QPDFParser;

  private:
    ParseGuard(QPDF* qpdf) :
        qpdf(qpdf)
    {
        if (qpdf) {
            qpdf->inParse(true);
        }
    }

    static std::shared_ptr<QPDFObject>
    getObject(QPDF* qpdf, int id, int gen, bool parse_pdf)
    {
        return qpdf->getObjectForParser(id, gen, parse_pdf);
    }

    ~ParseGuard()
    {
        if (qpdf) {
            qpdf->inParse(false);
        }
    }
    QPDF* qpdf;
};

// Pipe class is restricted to QPDF_Stream.
class QPDF::Pipe
{
    friend class qpdf::Stream;

  private:
    static bool
    pipeStreamData(
        QPDF* qpdf,
        QPDFObjGen og,
        qpdf_offset_t offset,
        size_t length,
        QPDFObjectHandle dict,
        bool is_root_metadata,
        Pipeline* pipeline,
        bool suppress_warnings,
        bool will_retry)
    {
        return qpdf->pipeStreamData(
            og, offset, length, dict, is_root_metadata, pipeline, suppress_warnings, will_retry);
    }
};

class QPDF::ObjCache
{
  public:
    ObjCache() = default;
    ObjCache(
        std::shared_ptr<QPDFObject> object,
        qpdf_offset_t end_before_space = 0,
        qpdf_offset_t end_after_space = 0) :
        object(std::move(object)),
        end_before_space(end_before_space),
        end_after_space(end_after_space)
    {
    }

    std::shared_ptr<QPDFObject> object;
    qpdf_offset_t end_before_space{0};
    qpdf_offset_t end_after_space{0};
};

class QPDF::ObjCopier
{
  public:
    std::map<QPDFObjGen, QPDFObjectHandle> object_map;
    std::vector<QPDFObjectHandle> to_copy;
    QPDFObjGen::set visiting;
};

class QPDF::EncryptionParameters
{
    friend class QPDF;

  public:
    EncryptionParameters() = default;

    int
    P() const
    {
        return static_cast<int>(P_.to_ullong());
    }

    //  Bits in P are numbered from 1 as in the PDF spec.
    bool P(size_t bit) const;

    int
    R()
    {
        return R_;
    }

    void initialize(QPDF& qpdf);
    encryption_method_e interpretCF(QPDFObjectHandle const& cf) const;

  private:
    bool encrypted{false};
    bool encryption_initialized{false};
    std::bitset<32> P_{0xfffffffc}; // Specification always requires bits 1 and 2 to be cleared.
    int encryption_V{0};
    int R_{0};
    bool encrypt_metadata{true};
    std::map<std::string, encryption_method_e> crypt_filters;
    encryption_method_e cf_stream{e_none};
    encryption_method_e cf_string{e_none};
    encryption_method_e cf_file{e_none};
    std::string provided_password;
    std::string user_password;
    std::string encryption_key;
    std::string cached_object_encryption_key;
    QPDFObjGen cached_key_og{};
    bool user_password_matched{false};
    bool owner_password_matched{false};
};

class QPDF::ForeignStreamData
{
    friend class QPDF;

  public:
    ForeignStreamData(
        std::shared_ptr<EncryptionParameters> encp,
        std::shared_ptr<InputSource> file,
        QPDFObjGen foreign_og,
        qpdf_offset_t offset,
        size_t length,
        QPDFObjectHandle local_dict,
        bool is_root_metadata);

  private:
    std::shared_ptr<EncryptionParameters> encp;
    std::shared_ptr<InputSource> file;
    QPDFObjGen foreign_og;
    qpdf_offset_t offset;
    size_t length;
    QPDFObjectHandle local_dict;
    bool is_root_metadata{false};
};

class QPDF::CopiedStreamDataProvider: public QPDFObjectHandle::StreamDataProvider
{
  public:
    CopiedStreamDataProvider(QPDF& destination_qpdf);
    ~CopiedStreamDataProvider() override = default;
    bool provideStreamData(
        QPDFObjGen const& og, Pipeline* pipeline, bool suppress_warnings, bool will_retry) override;
    void registerForeignStream(QPDFObjGen const& local_og, QPDFObjectHandle foreign_stream);
    void registerForeignStream(QPDFObjGen const& local_og, std::shared_ptr<ForeignStreamData>);

  private:
    QPDF& destination_qpdf;
    std::map<QPDFObjGen, QPDFObjectHandle> foreign_streams;
    std::map<QPDFObjGen, std::shared_ptr<ForeignStreamData>> foreign_stream_data;
};

class QPDF::StringDecrypter final: public QPDFObjectHandle::StringDecrypter
{
    friend class QPDF;

  public:
    StringDecrypter(QPDF* qpdf, QPDFObjGen og);
    ~StringDecrypter() final = default;
    void
    decryptString(std::string& val) final
    {
        qpdf->decryptString(val, og);
    }

  private:
    QPDF* qpdf;
    QPDFObjGen og;
};

// PDF 1.4: Table F.4
struct QPDF::HPageOffsetEntry
{
    int delta_nobjects{0};              // 1
    qpdf_offset_t delta_page_length{0}; // 2
    // vectors' sizes = nshared_objects
    int nshared_objects{0};                // 3
    std::vector<int> shared_identifiers;   // 4
    std::vector<int> shared_numerators;    // 5
    qpdf_offset_t delta_content_offset{0}; // 6
    qpdf_offset_t delta_content_length{0}; // 7
};

// PDF 1.4: Table F.3
struct QPDF::HPageOffset
{
    int min_nobjects{0};                // 1
    qpdf_offset_t first_page_offset{0}; // 2
    int nbits_delta_nobjects{0};        // 3
    int min_page_length{0};             // 4
    int nbits_delta_page_length{0};     // 5
    int min_content_offset{0};          // 6
    int nbits_delta_content_offset{0};  // 7
    int min_content_length{0};          // 8
    int nbits_delta_content_length{0};  // 9
    int nbits_nshared_objects{0};       // 10
    int nbits_shared_identifier{0};     // 11
    int nbits_shared_numerator{0};      // 12
    int shared_denominator{0};          // 13
    // vector size is npages
    std::vector<HPageOffsetEntry> entries;
};

// PDF 1.4: Table F.6
struct QPDF::HSharedObjectEntry
{
    // Item 3 is a 128-bit signature (unsupported by Acrobat)
    int delta_group_length{0}; // 1
    int signature_present{0};  // 2 -- always 0
    int nobjects_minus_one{0}; // 4 -- always 0
};

// PDF 1.4: Table F.5
struct QPDF::HSharedObject
{
    int first_shared_obj{0};              // 1
    qpdf_offset_t first_shared_offset{0}; // 2
    int nshared_first_page{0};            // 3
    int nshared_total{0};                 // 4
    int nbits_nobjects{0};                // 5
    int min_group_length{0};              // 6
    int nbits_delta_group_length{0};      // 7
    // vector size is nshared_total
    std::vector<HSharedObjectEntry> entries;
};

// PDF 1.4: Table F.9
struct QPDF::HGeneric
{
    int first_object{0};                  // 1
    qpdf_offset_t first_object_offset{0}; // 2
    int nobjects{0};                      // 3
    int group_length{0};                  // 4
};

// Other linearization data structures

// Initialized from Linearization Parameter dictionary
struct QPDF::LinParameters
{
    qpdf_offset_t file_size{0};        // /L
    int first_page_object{0};          // /O
    qpdf_offset_t first_page_end{0};   // /E
    size_t npages{0};                  // /N
    qpdf_offset_t xref_zero_offset{0}; // /T
    int first_page{0};                 // /P
    qpdf_offset_t H_offset{0};         // offset of primary hint stream
    qpdf_offset_t H_length{0};         // length of primary hint stream
};

// Computed hint table value data structures.  These tables contain the computed values on which
// the hint table values are based.  They exclude things like number of bits and store actual
// values instead of mins and deltas.  File offsets are also absolute rather than being offset
// by the size of the primary hint table.  We populate the hint table structures from these
// during writing and compare the hint table values with these during validation.  We ignore
// some values for various reasons described in the code.  Those values are omitted from these
// structures.  Note also that object numbers are object numbers from the input file, not the
// output file.

// Naming convention: CHSomething is analogous to HSomething above.  "CH" is computed hint.

struct QPDF::CHPageOffsetEntry
{
    int nobjects{0};
    int nshared_objects{0};
    // vectors' sizes = nshared_objects
    std::vector<int> shared_identifiers;
};

struct QPDF::CHPageOffset
{
    // vector size is npages
    std::vector<CHPageOffsetEntry> entries;
};

struct QPDF::CHSharedObjectEntry
{
    CHSharedObjectEntry(int object) :
        object(object)
    {
    }

    int object;
};

// PDF 1.4: Table F.5
struct QPDF::CHSharedObject
{
    int first_shared_obj{0};
    int nshared_first_page{0};
    int nshared_total{0};
    // vector size is nshared_total
    std::vector<CHSharedObjectEntry> entries;
};

// No need for CHGeneric -- HGeneric is fine as is.

// Data structures to support optimization -- implemented in QPDF_optimization.cc

class QPDF::ObjUser
{
  public:
    enum user_e { ou_page = 1, ou_thumb, ou_trailer_key, ou_root_key, ou_root };

    ObjUser() = delete;

    // type must be ou_root
    ObjUser(user_e type);

    // type must be one of ou_page or ou_thumb
    ObjUser(user_e type, size_t pageno);

    // type must be one of ou_trailer_key or ou_root_key
    ObjUser(user_e type, std::string const& key);

    bool operator<(ObjUser const&) const;

    user_e ou_type;
    size_t pageno{0}; // if ou_page;
    std::string key;  // if ou_trailer_key or ou_root_key
};

struct QPDF::UpdateObjectMapsFrame
{
    UpdateObjectMapsFrame(ObjUser const& ou, QPDFObjectHandle oh, bool top);

    ObjUser const& ou;
    QPDFObjectHandle oh;
    bool top;
};

class QPDF::PatternFinder final: public InputSource::Finder
{
  public:
    PatternFinder(QPDF& qpdf, bool (QPDF::*checker)()) :
        qpdf(qpdf),
        checker(checker)
    {
    }
    ~PatternFinder() final = default;
    bool
    check() final
    {
        return (this->qpdf.*checker)();
    }

  private:
    QPDF& qpdf;
    bool (QPDF::*checker)();
};

class QPDF::Members
{
    friend class QPDF;
    friend class ResolveRecorder;

  public:
    Members();
    Members(Members const&) = delete;
    ~Members() = default;

  private:
    std::shared_ptr<QPDFLogger> log;
    unsigned long long unique_id{0};
    qpdf::Tokenizer tokenizer;
    std::shared_ptr<InputSource> file;
    std::string last_object_description;
    std::shared_ptr<QPDFObject::Description> last_ostream_description;
    bool provided_password_is_hex_key{false};
    bool ignore_xref_streams{false};
    bool suppress_warnings{false};
    size_t max_warnings{0};
    bool attempt_recovery{true};
    bool check_mode{false};
    std::shared_ptr<EncryptionParameters> encp;
    std::string pdf_version;
    std::map<QPDFObjGen, QPDFXRefEntry> xref_table;
    // Various tables are indexed by object id, with potential size id + 1
    int xref_table_max_id{std::numeric_limits<int>::max() - 1};
    qpdf_offset_t xref_table_max_offset{0};
    std::set<int> deleted_objects;
    std::map<QPDFObjGen, ObjCache> obj_cache;
    std::set<QPDFObjGen> resolving;
    QPDFObjectHandle trailer;
    std::vector<QPDFObjectHandle> all_pages;
    bool invalid_page_found{false};
    std::map<QPDFObjGen, int> pageobj_to_pages_pos;
    bool pushed_inherited_attributes_to_pages{false};
    bool ever_pushed_inherited_attributes_to_pages{false};
    bool ever_called_get_all_pages{false};
    std::vector<QPDFExc> warnings;
    std::map<unsigned long long, ObjCopier> object_copiers;
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> copied_streams;
    // copied_stream_data_provider is owned by copied_streams
    CopiedStreamDataProvider* copied_stream_data_provider{nullptr};
    bool reconstructed_xref{false};
    bool in_read_xref_stream{false};
    bool fixed_dangling_refs{false};
    bool immediate_copy_from{false};
    bool in_parse{false};
    bool parsed{false};
    std::set<int> resolved_object_streams;

    // Linearization data
    qpdf_offset_t first_xref_item_offset{0}; // actual value from file
    bool uncompressed_after_compressed{false};
    bool linearization_warnings{false};

    // Linearization parameter dictionary and hint table data: may be read from file or computed
    // prior to writing a linearized file
    QPDFObjectHandle lindict;
    LinParameters linp;
    HPageOffset page_offset_hints;
    HSharedObject shared_object_hints;
    HGeneric outline_hints;

    // Computed linearization data: used to populate above tables during writing and to compare
    // with them during validation. c_ means computed.
    LinParameters c_linp;
    CHPageOffset c_page_offset_data;
    CHSharedObject c_shared_object_data;
    HGeneric c_outline_data;

    // Object ordering data for linearized files: initialized by calculateLinearizationData().
    // Part numbers refer to the PDF 1.4 specification.
    std::vector<QPDFObjectHandle> part4;
    std::vector<QPDFObjectHandle> part6;
    std::vector<QPDFObjectHandle> part7;
    std::vector<QPDFObjectHandle> part8;
    std::vector<QPDFObjectHandle> part9;

    // Optimization data
    std::map<ObjUser, std::set<QPDFObjGen>> obj_user_to_objects;
    std::map<QPDFObjGen, std::set<ObjUser>> object_to_obj_users;
};

// JobSetter class is restricted to QPDFJob.
class QPDF::JobSetter
{
    friend class QPDFJob;

  private:
    // Enable enhanced warnings for pdf file checking.
    static void
    setCheckMode(QPDF& qpdf, bool val)
    {
        qpdf.m->check_mode = val;
    }
};

class QPDF::ResolveRecorder
{
  public:
    ResolveRecorder(QPDF* qpdf, QPDFObjGen const& og) :
        qpdf(qpdf),
        iter(qpdf->m->resolving.insert(og).first)
    {
    }
    virtual ~ResolveRecorder()
    {
        this->qpdf->m->resolving.erase(iter);
    }

  private:
    QPDF* qpdf;
    std::set<QPDFObjGen>::const_iterator iter;
};

inline bool
QPDF::reconstructed_xref() const
{
    return m->reconstructed_xref;
}

#endif // QPDF_PRIVATE_HH
