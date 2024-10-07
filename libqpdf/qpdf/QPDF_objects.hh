#ifndef QPDF_OBJECTS_HH
#define QPDF_OBJECTS_HH

#include <qpdf/QPDF.hh>

// The Objects class is responsible for keeping track of all objects belonging to a QPDF instance,
// including loading it from an input source when required.
class QPDF::Objects
{
  public:
    Objects(QPDF& qpdf, QPDF::Members* m) :
        qpdf(qpdf),
        m(m)
    {
    }

    std::map<QPDFObjGen, ObjCache> obj_cache;

    QPDFObjectHandle readObjectInStream(std::shared_ptr<InputSource>& input, int obj);
    QPDFObjectHandle read(
        bool attempt_recovery,
        qpdf_offset_t offset,
        std::string const& description,
        QPDFObjGen exp_og,
        QPDFObjGen& og,
        bool skip_cache_if_in_xref);
    QPDFObject* resolve(QPDFObjGen og);
    void resolveObjectsInStream(int obj_stream_number);
    void update_table(QPDFObjGen og, std::shared_ptr<QPDFObject> const& object);
    QPDFObjGen next_id();
    QPDFObjectHandle make_indirect(std::shared_ptr<QPDFObject> const& obj);
    std::shared_ptr<QPDFObject> get_for_parser(int id, int gen, bool parse_pdf);
    std::shared_ptr<QPDFObject> get_for_json(int id, int gen);

    // Get a list of objects that would be permitted in an object stream.
    template <typename T>
    std::vector<T> compressible();
    std::vector<QPDFObjGen> compressible_vector();
    std::vector<bool> compressible_set();

    // Used by QPDFWriter to determine the vector part of its object tables.
    size_t table_size();

  private:
    friend class QPDF::Xref_table;

    void erase(QPDFObjGen og);
    bool cached(QPDFObjGen og);
    bool unresolved(QPDFObjGen og);

    QPDFObjectHandle read_object(std::string const& description, QPDFObjGen og);
    void read_stream(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset);
    void validate_stream_line_end(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset);
    size_t recover_stream_length(
        std::shared_ptr<InputSource> input, QPDFObjGen og, qpdf_offset_t stream_offset);

    QPDF& qpdf;
    QPDF::Members* m;
}; // Objects

#endif // QPDF_OBJECTS_HH
