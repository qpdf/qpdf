#ifndef QPDF_PRIVATE_HH
#define QPDF_PRIVATE_HH

#include <qpdf/QPDF.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_Null.hh>

// The object table is a combination of the previous object cache and (, in future,) the xref_table
// (, as well as possibly other existing structures). It is a hybrid of a map and a vector indexed
// (currently by objgen but in future) by object id, in that it allows random non-contiguous
// insertion/ access as well as appending. The interface is modelled on std::map and std::vector as
// appropriate. Return types are simplified where this reduces code. For validity of references and
// iterators the worst case of std::map and std::vector should be assumed.
class QPDF::ObjTable
{
  public:
    class Entry
    {
      public:
        Entry() = default;

        Entry(
            std::shared_ptr<QPDFObject>&& object,
            qpdf_offset_t end_before_space = -1,
            qpdf_offset_t end_after_space = -1) :
            object(object),
            end_before_space(end_before_space),
            end_after_space(end_after_space)
        {
        }

        std::shared_ptr<QPDFObject> object;
        qpdf_offset_t end_before_space{0};
        qpdf_offset_t end_after_space{0};
    };

    using iterator = std::map<QPDFObjGen, Entry>::iterator;
    using const_reverse_iterator = std::map<QPDFObjGen, Entry>::const_reverse_iterator;

    ObjTable(QPDF& qpdf) :
        qpdf(qpdf)
    {
    }

    iterator
    begin() noexcept
    {
        return entries.begin();
    }

    iterator
    end() noexcept
    {
        return entries.end();
    }

    const_reverse_iterator
    crbegin() const noexcept
    {
        return entries.crbegin();
    }

    bool
    contains(QPDFObjGen og)
    {
        return entries.count(og) != 0;
    }

    bool
    containsResolved(QPDFObjGen og)
    {
        auto it = entries.find(og);
        return it != entries.end() && it->second.object->getTypeCode() != ::ot_unresolved;
    }

    bool
    empty()
    {
        return entries.empty();
    }

    void
    erase(QPDFObjGen og)
    {
        static std::shared_ptr<QPDFObject> null = QPDF_Null::create();
        if (auto cached = entries.find(og); cached != entries.end()) {
            // Take care of any object handles that may be floating around.
            cached->second.object->assign(null);
            entries.erase(cached);
        }
    }

    iterator
    find(QPDFObjGen og)
    {
        return entries.find(og);
    }

    iterator
    insert(std::pair<QPDFObjGen const, Entry>&& pair)
    {
        pair.second.object->setObjGen(&qpdf, pair.first);
        return entries.insert(std::move(pair)).first;
    }

    iterator insert_or_assign(QPDFObjGen oh, Entry&& obj);

    iterator
    upper_bound(QPDFObjGen og)
    {
        return entries.upper_bound(og);
    }

  private:
    std::map<QPDFObjGen, Entry> entries;
    QPDF& qpdf;
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
    std::shared_ptr<InputSource> file;
    std::string last_object_description;
    bool provided_password_is_hex_key{false};
    bool ignore_xref_streams{false};
    bool suppress_warnings{false};
    bool attempt_recovery{true};
    bool check_mode{false};
    std::shared_ptr<EncryptionParameters> encp;
    std::string pdf_version;
    std::map<QPDFObjGen, QPDFXRefEntry> xref_table;
    std::set<int> deleted_objects;
    ObjTable obj_cache;
    std::set<QPDFObjGen> resolving;
    QPDFObjectHandle trailer;
    std::vector<QPDFObjectHandle> all_pages;
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

#endif // QPDF_PRIVATE_HH
