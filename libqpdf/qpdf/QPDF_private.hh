#ifndef QPDF_PRIVATE_HH
#define QPDF_PRIVATE_HH

#include <qpdf/QPDF.hh>

#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageLabelDocumentHelper.hh>
#include <qpdf/QPDFTokenizer_private.hh>

using namespace qpdf;

namespace qpdf
{
    class Stream;
    namespace is
    {
        class OffsetBuffer;
    } // namespace is
} // namespace qpdf

class BitStream;
class BitWriter;

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
    encryption_method_e interpretCF(Name const& cf) const;

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

// This class is used to represent a PDF document.
//
// The main function of the QPDF class is to represent a PDF document. Doc is the implementation
// class for this aspect of QPDF.
class QPDF::Doc
{
  public:
    class Encryption;
    class JobSetter;
    class Linearization;
    class Objects;
    class Pages;
    class ParseGuard;
    class Resolver;
    class Writer;

    // This is the common base-class for all document components. It is used by the other document
    // components to access common functionality. It is not meant to be used directly by the user.
    class Common
    {
      public:
        Common() = delete;
        Common(Common const&) = default;
        Common(Common&&) = delete;
        Common& operator=(Common const&) = delete;
        Common& operator=(Common&&) = delete;
        ~Common() = default;

        inline Common(QPDF& qpdf, QPDF::Members* m);

        void stopOnError(std::string const& message);
        void warn(QPDFExc const& e);
        void warn(
            qpdf_error_code_e error_code,
            std::string const& object,
            qpdf_offset_t offset,
            std::string const& message);

        static QPDFExc damagedPDF(
            InputSource& input,
            std::string const& object,
            qpdf_offset_t offset,
            std::string const& message);
        QPDFExc
        damagedPDF(InputSource& input, qpdf_offset_t offset, std::string const& message) const;
        QPDFExc damagedPDF(
            std::string const& object, qpdf_offset_t offset, std::string const& message) const;
        QPDFExc damagedPDF(std::string const& object, std::string const& message) const;
        QPDFExc damagedPDF(qpdf_offset_t offset, std::string const& message) const;
        QPDFExc damagedPDF(std::string const& message) const;

        void
        no_ci_stop_if(
            bool condition, std::string const& message, std::string const& context = {}) const
        {
            if (condition) {
                throw damagedPDF(context, message);
            }
        }

      protected:
        QPDF& qpdf;
        QPDF::Members* m;

        QPDF::Doc::Pages& pages;
    };

    Doc() = delete;
    Doc(Doc const&) = delete;
    Doc(Doc&&) = delete;
    Doc& operator=(Doc const&) = delete;
    Doc& operator=(Doc&&) = delete;
    ~Doc() = default;

    Doc(QPDF& qpdf, QPDF::Members* m) :
        qpdf(qpdf),
        m(m)
    {
    }

    inline Linearization& linearization();

    inline Objects& objects();

    inline Pages& pages();

    bool reconstructed_xref() const;

    QPDFAcroFormDocumentHelper&
    acroform()
    {
        if (!acroform_) {
            acroform_ = std::make_unique<QPDFAcroFormDocumentHelper>(qpdf);
        }
        return *acroform_;
    }

    QPDFEmbeddedFileDocumentHelper&
    embedded_files()
    {
        if (!embedded_files_) {
            embedded_files_ = std::make_unique<QPDFEmbeddedFileDocumentHelper>(qpdf);
        }
        return *embedded_files_;
    }

    QPDFOutlineDocumentHelper&
    outlines()
    {
        if (!outlines_) {
            outlines_ = std::make_unique<QPDFOutlineDocumentHelper>(qpdf);
        }
        return *outlines_;
    }

    QPDFPageDocumentHelper&
    page_dh()
    {
        if (!page_dh_) {
            page_dh_ = std::make_unique<QPDFPageDocumentHelper>(qpdf);
        }
        return *page_dh_;
    }

    QPDFPageLabelDocumentHelper&
    page_labels()
    {
        if (!page_labels_) {
            page_labels_ = std::make_unique<QPDFPageLabelDocumentHelper>(qpdf);
        }
        return *page_labels_;
    }

  private:
    QPDF& qpdf;
    QPDF::Members* m;

    // Document Helpers;
    std::unique_ptr<QPDFAcroFormDocumentHelper> acroform_;
    std::unique_ptr<QPDFEmbeddedFileDocumentHelper> embedded_files_;
    std::unique_ptr<QPDFOutlineDocumentHelper> outlines_;
    std::unique_ptr<QPDFPageDocumentHelper> page_dh_;
    std::unique_ptr<QPDFPageLabelDocumentHelper> page_labels_;
};

class QPDF::Doc::Encryption
{
  public:
    // This class holds data read from the encryption dictionary.
    Encryption(
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
        P(static_cast<unsigned long long>(P)),
        O(O),
        U(U),
        OE(OE),
        UE(UE),
        Perms(Perms),
        id1(id1),
        encrypt_metadata(encrypt_metadata)
    {
    }
    Encryption(int V, int R, int Length_bytes, bool encrypt_metadata) :
        V(V),
        R(R),
        Length_bytes(Length_bytes),
        encrypt_metadata(encrypt_metadata)
    {
    }

    int getV() const;
    int getR() const;
    int getLengthBytes() const;
    int getP() const;
    //  Bits in P are numbered from 1 as in the PDF spec.
    bool getP(size_t bit) const;
    std::string const& getO() const;
    std::string const& getU() const;
    std::string const& getOE() const;
    std::string const& getUE() const;
    std::string const& getPerms() const;
    std::string const& getId1() const;
    bool getEncryptMetadata() const;
    //  Bits in P are numbered from 1 as in the PDF spec.
    void setP(size_t bit, bool val);
    void setP(unsigned long val);
    void setO(std::string const&);
    void setU(std::string const&);
    void setId1(std::string const& val);
    void setV5EncryptionParameters(
        std::string const& O,
        std::string const& OE,
        std::string const& U,
        std::string const& UE,
        std::string const& Perms);

    std::string compute_encryption_key(std::string const& password) const;

    bool check_owner_password(std::string& user_password, std::string const& owner_password) const;

    bool check_user_password(std::string const& user_password) const;

    std::string
    recover_encryption_key_with_password(std::string const& password, bool& perms_valid) const;

    void compute_encryption_O_U(char const* user_password, char const* owner_password);

    std::string
    compute_encryption_parameters_V5(char const* user_password, char const* owner_password);

    std::string compute_parameters(char const* user_password, char const* owner_password);

  private:
    static constexpr unsigned int OU_key_bytes_V4 = 16; // ( == sizeof(MD5::Digest)

    Encryption(Encryption const&) = delete;
    Encryption& operator=(Encryption const&) = delete;

    std::string
    hash_V5(std::string const& password, std::string const& salt, std::string const& udata) const;
    std::string
    compute_O_value(std::string const& user_password, std::string const& owner_password) const;
    std::string compute_U_value(std::string const& user_password) const;
    std::string compute_encryption_key_from_password(std::string const& password) const;
    std::string recover_encryption_key_with_password(std::string const& password) const;
    bool
    check_owner_password_V4(std::string& user_password, std::string const& owner_password) const;
    bool check_owner_password_V5(std::string const& owner_passworda) const;
    std::string compute_Perms_value_V5_clear() const;
    std::string
    compute_O_rc4_key(std::string const& user_password, std::string const& owner_password) const;
    std::string compute_U_value_R2(std::string const& user_password) const;
    std::string compute_U_value_R3(std::string const& user_password) const;
    bool check_user_password_V4(std::string const& user_password) const;
    bool check_user_password_V5(std::string const& user_password) const;

    int V;
    int R;
    int Length_bytes;
    std::bitset<32> P{0xfffffffc}; // Specification always requires bits 1 and 2 to be cleared.
    std::string O;
    std::string U;
    std::string OE;
    std::string UE;
    std::string Perms;
    std::string id1;
    bool encrypt_metadata;
}; // class QPDF::Doc::Encryption

class QPDF::Doc::Linearization: Common
{
  public:
    Linearization() = delete;
    Linearization(Linearization const&) = delete;
    Linearization(Linearization&&) = delete;
    Linearization& operator=(Linearization const&) = delete;
    Linearization& operator=(Linearization&&) = delete;
    ~Linearization() = default;

    Linearization(Doc& doc) :
        Common(doc.qpdf, doc.m)
    {
    }

    // For QPDFWriter:

    template <typename T>
    void optimize_internal(
        T const& object_stream_data,
        bool allow_changes = true,
        std::function<int(QPDFObjectHandle&)> skip_stream_parameters = nullptr);
    void optimize(
        QPDFWriter::ObjTable const& obj,
        std::function<int(QPDFObjectHandle&)> skip_stream_parameters);

    // Get lists of all objects in order according to the part of a linearized file that they
    // belong to.
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

    // methods to support linearization checking -- implemented in QPDF_linearization.cc

    void readLinearizationData();
    void checkLinearizationInternal();
    void dumpLinearizationDataInternal();
    void linearizationWarning(std::string_view);
    qpdf::Dictionary readHintStream(Pipeline&, qpdf_offset_t offset, size_t length);
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

    void updateObjectMaps(
        ObjUser const& ou,
        QPDFObjectHandle oh,
        std::function<int(QPDFObjectHandle&)> skip_stream_parameters);
    void filterCompressedObjects(std::map<int, int> const& object_stream_data);
    void filterCompressedObjects(QPDFWriter::ObjTable const& object_stream_data);
};

class QPDF::Doc::Objects: Common
{
  public:
    class Foreign: Common
    {
        class Copier: Common
        {
          public:
            inline Copier(QPDF& qpdf);

            QPDFObjectHandle copied(QPDFObjectHandle const& foreign);

          private:
            QPDFObjectHandle
            replace_indirect_object(QPDFObjectHandle const& foreign, bool top = false);
            void reserve_objects(QPDFObjectHandle const& foreign, bool top = false);

            std::map<QPDFObjGen, QPDFObjectHandle> object_map;
            std::vector<QPDFObjectHandle> to_copy;
            QPDFObjGen::set visiting;
        };

      public:
        Foreign(Common& common) :
            Common(common)
        {
        }

        Foreign() = delete;
        Foreign(Foreign const&) = delete;
        Foreign(Foreign&&) = delete;
        Foreign& operator=(Foreign const&) = delete;
        Foreign& operator=(Foreign&&) = delete;
        ~Foreign() = default;

        // Return a local handle to the foreign object. Copy the foreign object if necessary.
        QPDFObjectHandle
        copied(QPDFObjectHandle const& foreign)
        {
            return copier(foreign).copied(foreign);
        }

      private:
        Copier& copier(QPDFObjectHandle const& foreign);

        std::map<unsigned long long, Copier> copiers;
    }; // class QPDF::Doc::Objects::Foreign

    class Streams: Common
    {
        // Copier manages the copying of streams into this PDF. It is used both for copying
        // local and foreign streams.
        class Copier;

      public:
        Streams(Common& common);

        Streams() = delete;
        Streams(Streams const&) = delete;
        Streams(Streams&&) = delete;
        Streams& operator=(Streams const&) = delete;
        Streams& operator=(Streams&&) = delete;
        ~Streams() = default;

      public:
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
                og,
                offset,
                length,
                dict,
                is_root_metadata,
                pipeline,
                suppress_warnings,
                will_retry);
        }

        std::shared_ptr<Copier>&
        copier()
        {
            return copier_;
        }

        bool immediate_copy_from() const;

      private:
        std::shared_ptr<Copier> copier_;
    }; // class QPDF::Doc::Objects::Streams

  public:
    Objects() = delete;
    Objects(Objects const&) = delete;
    Objects(Objects&&) = delete;
    Objects& operator=(Objects const&) = delete;
    Objects& operator=(Objects&&) = delete;
    ~Objects() = default;

    Objects(Doc& doc) :
        Common(doc.qpdf, doc.m),
        foreign_(*this),
        streams_(*this)
    {
    }

    Foreign&
    foreign()
    {
        return foreign_;
    }

    Streams&
    streams()
    {
        return streams_;
    }

    void parse(char const* password);
    std::shared_ptr<QPDFObject> const& resolve(QPDFObjGen og);
    void inParse(bool);
    QPDFObjGen nextObjGen();
    QPDFObjectHandle newIndirect(QPDFObjGen, std::shared_ptr<QPDFObject> const&);
    void updateCache(
        QPDFObjGen og,
        std::shared_ptr<QPDFObject> const& object,
        qpdf_offset_t end_before_space,
        qpdf_offset_t end_after_space,
        bool destroy = true);
    bool resolveXRefTable();
    QPDFObjectHandle readObjectAtOffset(
        qpdf_offset_t offset, std::string const& description, bool skip_cache_if_in_xref);
    QPDFTokenizer::Token readToken(InputSource& input, size_t max_len = 0);
    QPDFObjectHandle makeIndirectFromQPDFObject(std::shared_ptr<QPDFObject> const& obj);
    std::shared_ptr<QPDFObject> getObjectForParser(int id, int gen, bool parse_pdf);
    std::shared_ptr<QPDFObject> getObjectForJSON(int id, int gen);
    size_t tableSize();

    // For QPDFWriter:

    std::map<QPDFObjGen, QPDFXRefEntry> const& getXRefTableInternal();
    // Get a list of objects that would be permitted in an object stream.
    template <typename T>
    std::vector<T> getCompressibleObjGens();
    std::vector<QPDFObjGen> getCompressibleObjVector();
    std::vector<bool> getCompressibleObjSet();

  private:
    void setTrailer(QPDFObjectHandle obj);
    void reconstruct_xref(QPDFExc& e, bool found_startxref = true);
    void read_xref(qpdf_offset_t offset, bool in_stream_recovery = false);
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
    QPDFObjectHandle readTrailer();
    QPDFObjectHandle readObject(std::string const& description, QPDFObjGen og);
    void readStream(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset);
    void validateStreamLineEnd(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset);
    QPDFObjectHandle readObjectInStream(qpdf::is::OffsetBuffer& input, int stream_id, int obj_id);
    size_t recoverStreamLength(
        std::shared_ptr<InputSource> input, QPDFObjGen og, qpdf_offset_t stream_offset);

    QPDFObjGen read_object_start(qpdf_offset_t offset);
    void readObjectAtOffset(
        bool attempt_recovery,
        qpdf_offset_t offset,
        std::string const& description,
        QPDFObjGen exp_og);
    void resolveObjectsInStream(int obj_stream_number);
    bool isCached(QPDFObjGen og);
    bool isUnresolved(QPDFObjGen og);
    void setLastObjectDescription(std::string const& description, QPDFObjGen og);

    Foreign foreign_;
    Streams streams_;
}; // class QPDF::Doc::Objects

// This class is used to represent a PDF Pages tree.
class QPDF::Doc::Pages: Common
{
  public:
    Pages() = delete;
    Pages(Pages const&) = delete;
    Pages(Pages&&) = delete;
    Pages& operator=(Pages const&) = delete;
    Pages& operator=(Pages&&) = delete;
    ~Pages() = default;

    Pages(Doc& doc) :
        Common(doc.qpdf, doc.m)
    {
    }

    std::vector<QPDFObjectHandle> const& all();

    bool
    empty()
    {
        return all().empty();
    }

    size_t
    size()
    {
        return all().size();
    }

    int find(QPDFObjGen og);
    void erase(QPDFObjectHandle& page);
    void update_cache();

    void insertPage(QPDFObjectHandle newpage, int pos);
    void pushInheritedAttributesToPage(bool allow_changes, bool warn_skipped_keys);

  private:
    void flattenPagesTree();
    void insertPageobjToPage(QPDFObjectHandle const& obj, int pos, bool check_duplicate);
    void pushInheritedAttributesToPageInternal(
        QPDFObjectHandle,
        std::map<std::string, std::vector<QPDFObjectHandle>>&,
        bool allow_changes,
        bool warn_skipped_keys);

    void getAllPagesInternal(
        QPDFObjectHandle cur_pages,
        QPDFObjGen::set& visited,
        QPDFObjGen::set& seen,
        bool media_box,
        bool resources);

}; // class QPDF::Doc::Pages

class QPDF::Members: Doc
{
    friend class QPDF;
    friend class ResolveRecorder;

  public:
    Members(QPDF& qpdf);
    Members(Members const&) = delete;
    ~Members() = default;

  private:
    Doc::Common c;
    Doc::Linearization lin;
    Doc::Objects objects;
    Doc::Pages pages;
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
    bool linearization_warnings{false}; // set by linearizationWarning, used by checkLinearization

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

// The Resolver class is restricted to QPDFObject and BaseHandle so that only it can resolve
// indirect references.
class QPDF::Doc::Resolver
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;

  private:
    static std::shared_ptr<QPDFObject> const&
    resolved(QPDF* qpdf, QPDFObjGen og)
    {
        return qpdf->m->objects.resolve(og);
    }
};

inline QPDF::Doc::Common::Common(QPDF& qpdf, QPDF::Members* m) :
    qpdf(qpdf),
    m(m),
    pages(m->pages)
{
}

inline QPDF::Doc::Linearization&
QPDF::Doc::linearization()
{
    return m->lin;
};

inline QPDF::Doc::Objects&
QPDF::Doc::objects()
{
    return m->objects;
};

inline QPDF::Doc::Pages&
QPDF::Doc::pages()
{
    return m->pages;
}

inline bool
QPDF::Doc::reconstructed_xref() const
{
    return m->reconstructed_xref;
}

inline QPDF::Doc&
QPDF::doc()
{
    return *m;
}

inline QPDF::Doc::Objects::Foreign::Copier::Copier(QPDF& qpdf) :
    Common(qpdf, qpdf.doc().m)
{
}

#endif // QPDF_PRIVATE_HH
