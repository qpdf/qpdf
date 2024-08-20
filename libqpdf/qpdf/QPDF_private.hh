#ifndef QPDF_PRIVATE_HH
#define QPDF_PRIVATE_HH

#include <qpdf/QPDF.hh>

#include <variant>

// Xref_table encapsulates the pdf's xref table and trailer.
class QPDF::Xref_table
{
  public:
    Xref_table(QPDF& qpdf, InputSource* const& file) :
        qpdf(qpdf),
        file(file)
    {
        tokenizer.allowEOF();
    }

    void initialize();
    void initialize_empty();
    void initialize_json();
    void reconstruct(QPDFExc& e);
    void show();
    bool resolve();

    QPDFObjectHandle
    trailer() const
    {
        return trailer_;
    }

    void
    trailer(QPDFObjectHandle&& oh)
    {
        trailer_ = std::move(oh);
    }

    // Returns 0 if og is not in table.
    size_t
    type(QPDFObjGen og) const
    {
        int id = og.getObj();
        if (id < 1 || static_cast<size_t>(id) >= table.size()) {
            return 0;
        }
        auto& e = table[static_cast<size_t>(id)];
        return e.gen() == og.getGen() ? e.type() : 0;
    }

    // Returns 0 if og is not in table.
    size_t
    type(size_t id) const noexcept
    {
        if (id >= table.size()) {
            return 0;
        }
        return table[id].type();
    }

    // Returns 0 if og is not in table.
    qpdf_offset_t
    offset(QPDFObjGen og) const noexcept
    {
        int id = og.getObj();
        if (id < 1 || static_cast<size_t>(id) >= table.size()) {
            return 0;
        }
        return table[static_cast<size_t>(id)].offset();
    }

    // Returns 0 if id is not in table.
    int
    stream_number(int id) const noexcept
    {
        if (id < 1 || static_cast<size_t>(id) >= table.size()) {
            return 0;
        }
        return table[static_cast<size_t>(id)].stream_number();
    }

    int
    stream_index(int id) const noexcept
    {
        if (id < 1 || static_cast<size_t>(id) >= table.size()) {
            return 0;
        }
        return table[static_cast<size_t>(id)].stream_index();
    }

    QPDFObjGen at_offset(qpdf_offset_t offset) const noexcept;

    std::map<QPDFObjGen, QPDFXRefEntry> as_map() const;

    bool
    object_streams() const noexcept
    {
        return object_streams_;
    }

    // Return a vector of object id and stream number for each compressed object.
    std::vector<std::pair<unsigned int, int>>
    compressed_objects() const
    {
        if (!initialized()) {
            throw std::logic_error("Xref_table::compressed_objects called before parsing.");
        }

        std::vector<std::pair<unsigned int, int>> result;
        result.reserve(table.size());

        unsigned int i{0};
        for (auto const& item: table) {
            if (item.type() == 2) {
                result.emplace_back(i, item.stream_number());
            }
            ++i;
        }
        return result;
    }

    // Temporary access to underlying table size
    size_t
    size() const noexcept
    {
        return table.size();
    }

    void
    ignore_streams(bool val) noexcept
    {
        ignore_streams_ = val;
    }

    bool
    initialized() const noexcept
    {
        return initialized_;
    }

    void
    attempt_recovery(bool val) noexcept
    {
        attempt_recovery_ = val;
    }

    int
    max_id() const noexcept
    {
        return max_id_;
    }

    // For Linearization

    bool
    uncompressed_after_compressed() const noexcept
    {
        return uncompressed_after_compressed_;
    }

    // Actual value from file
    qpdf_offset_t
    first_item_offset() const noexcept
    {
        return first_item_offset_;
    }

  private:
    // Object, count, offset of first entry
    typedef std::tuple<int, int, qpdf_offset_t> Subsection;

    struct Uncompressed
    {
        Uncompressed(qpdf_offset_t offset) :
            offset(offset)
        {
        }
        qpdf_offset_t offset;
    };

    struct Compressed
    {
        Compressed(int stream_number, int stream_index) :
            stream_number(stream_number),
            stream_index(stream_index)
        {
        }
        int stream_number{0};
        int stream_index{0};
    };

    typedef std::variant<std::monostate, Uncompressed, Compressed> Xref;

    struct Entry
    {
        int
        gen() const noexcept
        {
            return gen_;
        }

        size_t
        type() const noexcept
        {
            return entry.index();
        }

        qpdf_offset_t
        offset() const noexcept
        {
            return type() == 1 ? std::get<1>(entry).offset : 0;
        }

        int
        stream_number() const noexcept
        {
            return type() == 2 ? std::get<2>(entry).stream_number : 0;
        }

        int
        stream_index() const noexcept
        {
            return type() == 2 ? std::get<2>(entry).stream_index : 0;
        }

        int gen_{0};
        Xref entry;
    };

    void read(qpdf_offset_t offset);

    // Methods to parse tables
    qpdf_offset_t process_section(qpdf_offset_t offset);
    std::vector<Subsection> subsections(std::string& line);
    std::vector<Subsection> bad_subsections(std::string& line, qpdf_offset_t offset);
    Subsection subsection(std::string const& line);
    bool read_entry(qpdf_offset_t& f1, int& f2, char& type);
    bool read_bad_entry(qpdf_offset_t& f1, int& f2, char& type);

    // Methods to parse streams
    qpdf_offset_t read_stream(qpdf_offset_t offset);
    qpdf_offset_t process_stream(qpdf_offset_t offset, QPDFObjectHandle& xref_stream);
    std::pair<int, std::array<int, 3>>
    process_W(QPDFObjectHandle& dict, std::function<QPDFExc(std::string_view)> damaged);
    std::pair<int, size_t> process_Size(
        QPDFObjectHandle& dict, int entry_size, std::function<QPDFExc(std::string_view)> damaged);
    std::pair<int, std::vector<std::pair<int, int>>> process_Index(
        QPDFObjectHandle& dict,
        int max_num_entries,
        std::function<QPDFExc(std::string_view)> damaged);

    QPDFObjectHandle read_trailer();

    QPDFTokenizer::Token
    read_token(size_t max_len = 0)
    {
        return tokenizer.readToken(*file, "", true, max_len);
    }

    // Methods to insert table entries
    void insert(int obj, int f0, qpdf_offset_t f1, int f2);
    void insert_free(QPDFObjGen);

    QPDFExc
    damaged_pdf(std::string const& msg)
    {
        return qpdf.damagedPDF("", 0, msg);
    }

    QPDFExc
    damaged_table(std::string const& msg)
    {
        return qpdf.damagedPDF("xref table", msg);
    }

    void
    warn_damaged(std::string const& msg)
    {
        qpdf.warn(damaged_pdf(msg));
    }

    QPDF& qpdf;
    InputSource* const& file;
    QPDFTokenizer tokenizer;

    std::vector<Entry> table;
    QPDFObjectHandle trailer_;

    bool attempt_recovery_{true};
    bool initialized_{false};
    bool ignore_streams_{false};
    bool reconstructed_{false};
    bool object_streams_{false};
    // Before the xref table is initialized, max_id_ is an upper bound on the possible object ids
    // that could be present in the PDF file. Once the trailer has been read, max_id_ is set to the
    // value of /Size. If the file is damaged, max_id_ becomes the maximum object id in the xref
    // table after reconstruction.
    int max_id_{std::numeric_limits<int>::max() - 1};

    // Linearization data
    bool uncompressed_after_compressed_{false};
    qpdf_offset_t first_item_offset_{0}; // actual value from file
};

// The Resolver class is restricted to QPDFObject so that only it can resolve indirect
// references.
class QPDF::Resolver
{
    friend class QPDFObject;
    friend class QPDF_Unresolved;

  private:
    static QPDFObject*
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
    friend class QPDF_Stream;

  private:
    static bool
    pipeStreamData(
        QPDF* qpdf,
        QPDFObjGen const& og,
        qpdf_offset_t offset,
        size_t length,
        QPDFObjectHandle dict,
        Pipeline* pipeline,
        bool suppress_warnings,
        bool will_retry)
    {
        return qpdf->pipeStreamData(
            og, offset, length, dict, pipeline, suppress_warnings, will_retry);
    }
};

class QPDF::ObjCache
{
  public:
    ObjCache() :
        end_before_space(0),
        end_after_space(0)
    {
    }
    ObjCache(
        std::shared_ptr<QPDFObject> object,
        qpdf_offset_t end_before_space = 0,
        qpdf_offset_t end_after_space = 0) :
        object(object),
        end_before_space(end_before_space),
        end_after_space(end_after_space)
    {
    }

    std::shared_ptr<QPDFObject> object;
    qpdf_offset_t end_before_space;
    qpdf_offset_t end_after_space;
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
    EncryptionParameters();

  private:
    bool encrypted;
    bool encryption_initialized;
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
    QPDFObjGen cached_key_og;
    bool user_password_matched;
    bool owner_password_matched;
};

class QPDF::ForeignStreamData
{
    friend class QPDF;

  public:
    ForeignStreamData(
        std::shared_ptr<EncryptionParameters> encp,
        std::shared_ptr<InputSource> file,
        QPDFObjGen const& foreign_og,
        qpdf_offset_t offset,
        size_t length,
        QPDFObjectHandle local_dict);

  private:
    std::shared_ptr<EncryptionParameters> encp;
    std::shared_ptr<InputSource> file;
    QPDFObjGen foreign_og;
    qpdf_offset_t offset;
    size_t length;
    QPDFObjectHandle local_dict;
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

class QPDF::StringDecrypter: public QPDFObjectHandle::StringDecrypter
{
    friend class QPDF;

  public:
    StringDecrypter(QPDF* qpdf, QPDFObjGen const& og);
    ~StringDecrypter() override = default;
    void decryptString(std::string& val) override;

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
    int npages{0};                     // /N
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
    enum user_e { ou_bad, ou_page, ou_thumb, ou_trailer_key, ou_root_key, ou_root };

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
    int pageno;      // if ou_page;
    std::string key; // if ou_trailer_key or ou_root_key
};

struct QPDF::UpdateObjectMapsFrame
{
    UpdateObjectMapsFrame(ObjUser const& ou, QPDFObjectHandle oh, bool top);

    ObjUser const& ou;
    QPDFObjectHandle oh;
    bool top;
};

class QPDF::PatternFinder: public InputSource::Finder
{
  public:
    PatternFinder(QPDF& qpdf, bool (QPDF::*checker)()) :
        qpdf(qpdf),
        checker(checker)
    {
    }
    ~PatternFinder() override = default;
    bool
    check() override
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
    QPDF_DLL
    ~Members() = default;

  private:
    Members(QPDF& qpdf);
    Members(Members const&) = delete;

    std::shared_ptr<QPDFLogger> log;
    unsigned long long unique_id{0};
    QPDFTokenizer tokenizer;
    // Filename to use if there is no input PDF
    std::string no_input_name{"closed input source"};
    // If file_sp is updated, file must also be updated.
    std::shared_ptr<InputSource> file_sp;
    InputSource* file;
    std::string last_object_description;
    bool provided_password_is_hex_key{false};
    bool suppress_warnings{false};
    size_t max_warnings{0};
    bool attempt_recovery{true};
    bool check_mode{false};
    std::shared_ptr<EncryptionParameters> encp;
    std::string pdf_version;
    Xref_table xref_table;
    std::map<QPDFObjGen, ObjCache> obj_cache;
    std::set<QPDFObjGen> resolving;
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
    bool fixed_dangling_refs{false};
    bool immediate_copy_from{false};
    bool in_parse{false};
    std::set<int> resolved_object_streams;

    // Linearization data
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
        return qpdf.optimize(obj, skip_stream_parameters);
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
        std::shared_ptr<Buffer>& hint_stream,
        int& S,
        int& O,
        bool compressed)
    {
        return qpdf.generateHintStream(new_obj, obj, hint_stream, S, O, compressed);
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

    static Xref_table const&
    getXRefTable(QPDF& qpdf)
    {
        return qpdf.m->xref_table;
    }

    static size_t
    tableSize(QPDF& qpdf)
    {
        return qpdf.tableSize();
    }
};

#endif // QPDF_PRIVATE_HH
