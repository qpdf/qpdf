#include <qpdf/qpdf-config.h> // include first for large file support

#include <qpdf/QPDF_private.hh>

#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

#include <qpdf/FileInputSource.hh>
#include <qpdf/InputSource_private.hh>
#include <qpdf/OffsetInputSource.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFParser.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>

using namespace qpdf;
using namespace std::literals;

using Common = impl::Doc::Common;
using Objects = impl::Doc::Objects;
using Foreign = Objects::Foreign;
using Streams = Objects::Streams;

// This must be a fixed value. This API returns a const reference to it, and the C API relies on its
// being static as well.
std::string const QPDF::qpdf_version(QPDF_VERSION);

static char const* EMPTY_PDF = (
    // force line break
    "%PDF-1.3\n"
    "1 0 obj\n"
    "<< /Type /Catalog /Pages 2 0 R >>\n"
    "endobj\n"
    "2 0 obj\n"
    "<< /Type /Pages /Kids [] /Count 0 >>\n"
    "endobj\n"
    "xref\n"
    "0 3\n"
    "0000000000 65535 f \n"
    "0000000009 00000 n \n"
    "0000000058 00000 n \n"
    "trailer << /Size 3 /Root 1 0 R >>\n"
    "startxref\n"
    "110\n"
    "%%EOF\n");

namespace
{
    class InvalidInputSource: public InputSource
    {
      public:
        ~InvalidInputSource() override = default;
        qpdf_offset_t
        findAndSkipNextEOL() override
        {
            throwException();
            return 0;
        }
        std::string const&
        getName() const override
        {
            static std::string name("closed input source");
            return name;
        }
        qpdf_offset_t
        tell() override
        {
            throwException();
            return 0;
        }
        void
        seek(qpdf_offset_t offset, int whence) override
        {
            throwException();
        }
        void
        rewind() override
        {
            throwException();
        }
        size_t
        read(char* buffer, size_t length) override
        {
            throwException();
            return 0;
        }
        void
        unreadCh(char ch) override
        {
            throwException();
        }

      private:
        void
        throwException()
        {
            throw std::logic_error(
                "QPDF operation attempted on a QPDF object with no input "
                "source. QPDF operations are invalid before processFile (or "
                "another process method) or after closeInputSource");
        }
    };
} // namespace

QPDF::StringDecrypter::StringDecrypter(QPDF* qpdf, QPDFObjGen og) :
    qpdf(qpdf),
    og(og)
{
}

std::string const&
QPDF::QPDFVersion()
{
    // The C API relies on this being a static value.
    return QPDF::qpdf_version;
}

QPDF::Members::Members(QPDF& qpdf) :
    Doc(qpdf, this),
    c(qpdf, this),
    lin(*this),
    objects(*this),
    pages(*this),
    file(std::make_shared<InvalidInputSource>()),
    encp(std::make_shared<EncryptionParameters>())
{
}

QPDF::QPDF() :
    m(std::make_unique<Members>(*this))
{
    m->tokenizer.allowEOF();
    // Generate a unique ID. It just has to be unique among all QPDF objects allocated throughout
    // the lifetime of this running application.
    static std::atomic<unsigned long long> unique_id{0};
    m->unique_id = unique_id.fetch_add(1ULL);
}

// Provide access to disconnect(). Disconnect will in due course be merged into the current ObjCache
// (future Objects::Entry) to centralize all QPDF access to QPDFObject.
class Disconnect: BaseHandle
{
  public:
    Disconnect(std::shared_ptr<QPDFObject> const& obj) :
        BaseHandle(obj)
    {
    }
    void
    disconnect()
    {
        BaseHandle::disconnect(false);
        if (raw_type_code() != ::ot_null) {
            obj->value = QPDF_Destroyed();
        }
    }
};

QPDF::~QPDF()
{
    // If two objects are mutually referential (through each object having an array or dictionary
    // that contains an indirect reference to the other), the circular references in the
    // std::shared_ptr objects will prevent the objects from being deleted. Walk through all objects
    // in the object cache, which is those objects that we read from the file, and break all
    // resolved indirect references by replacing them with an internal object type representing that
    // they have been destroyed. Note that we can't break references like this at any time when the
    // QPDF object is active. The call to reset also causes all direct QPDFObjectHandle objects that
    // are reachable from this object to release their association with this QPDF. Direct objects
    // are not destroyed since they can be moved to other QPDF objects safely.

    // At this point, obviously no one is still using the QPDF object, but we'll explicitly clear
    // the xref table anyway just to prevent any possibility of resolve() succeeding.
    m->xref_table.clear();
    for (auto const& iter: m->obj_cache) {
        Disconnect(iter.second.object).disconnect();
    }
}

std::shared_ptr<QPDF>
QPDF::create()
{
    return std::make_shared<QPDF>();
}

void
QPDF::processFile(char const* filename, char const* password)
{
    auto* fi = new FileInputSource(filename);
    processInputSource(std::shared_ptr<InputSource>(fi), password);
}

void
QPDF::processFile(char const* description, FILE* filep, bool close_file, char const* password)
{
    auto* fi = new FileInputSource(description, filep, close_file);
    processInputSource(std::shared_ptr<InputSource>(fi), password);
}

void
QPDF::processMemoryFile(
    char const* description, char const* buf, size_t length, char const* password)
{
    auto is = std::make_shared<is::OffsetBuffer>(description, std::string_view{buf, length});
    processInputSource(is, password);
}

void
QPDF::processInputSource(std::shared_ptr<InputSource> source, char const* password)
{
    m->file = source;
    m->objects.parse(password);
}

void
QPDF::closeInputSource()
{
    m->file = std::shared_ptr<InputSource>(new InvalidInputSource());
}

void
QPDF::setPasswordIsHexKey(bool val)
{
    m->cf.password_is_hex_key(val);
}

void
QPDF::emptyPDF()
{
    processMemoryFile("empty PDF", EMPTY_PDF, strlen(EMPTY_PDF));
}

void
QPDF::registerStreamFilter(
    std::string const& filter_name, std::function<std::shared_ptr<QPDFStreamFilter>()> factory)
{
    qpdf::Stream::registerStreamFilter(filter_name, factory);
}

void
QPDF::setIgnoreXRefStreams(bool val)
{
    (void)m->cf.ignore_xref_streams(val);
}

std::shared_ptr<QPDFLogger>
QPDF::getLogger()
{
    return m->cf.log();
}

void
QPDF::setLogger(std::shared_ptr<QPDFLogger> l)
{
    m->cf.log(l);
}

void
QPDF::setOutputStreams(std::ostream* out, std::ostream* err)
{
    setLogger(QPDFLogger::create());
    m->cf.log()->setOutputStreams(out, err);
}

void
QPDF::setSuppressWarnings(bool val)
{
    (void)m->cf.suppress_warnings(val);
}

void
QPDF::setMaxWarnings(size_t val)
{
    (void)m->cf.max_warnings(val);
}

void
QPDF::setAttemptRecovery(bool val)
{
    (void)m->cf.surpress_recovery(!val);
}

void
QPDF::setImmediateCopyFrom(bool val)
{
    (void)m->cf.immediate_copy_from(val);
}

std::vector<QPDFExc>
QPDF::getWarnings()
{
    std::vector<QPDFExc> result = std::move(m->warnings);
    m->warnings.clear();
    return result;
}

bool
QPDF::anyWarnings() const
{
    return !m->warnings.empty();
}

size_t
QPDF::numWarnings() const
{
    return m->warnings.size();
}

void
QPDF::warn(QPDFExc const& e)
{
    m->c.warn(e);
}

void
Common::warn(QPDFExc const& e)
{
    if (cf.max_warnings() > 0 && m->warnings.size() >= cf.max_warnings()) {
        stopOnError("Too many warnings - file is too badly damaged");
    }
    m->warnings.emplace_back(e);
    if (!cf.suppress_warnings()) {
        *cf.log()->getWarn() << "WARNING: " << m->warnings.back().what() << "\n";
    }
}

void
QPDF::warn(
    qpdf_error_code_e error_code,
    std::string const& object,
    qpdf_offset_t offset,
    std::string const& message)
{
    m->c.warn(QPDFExc(error_code, getFilename(), object, offset, message));
}

void
Common::warn(
    qpdf_error_code_e error_code,
    std::string const& object,
    qpdf_offset_t offset,
    std::string const& message)
{
    warn(QPDFExc(error_code, qpdf.getFilename(), object, offset, message));
}

QPDFObjectHandle
QPDF::newReserved()
{
    return m->objects.makeIndirectFromQPDFObject(QPDFObject::create<QPDF_Reserved>());
}

QPDFObjectHandle
QPDF::newIndirectNull()
{
    return m->objects.makeIndirectFromQPDFObject(QPDFObject::create<QPDF_Null>());
}

QPDFObjectHandle
QPDF::newStream()
{
    return makeIndirectObject(
        qpdf::Stream(*this, m->objects.nextObjGen(), Dictionary::empty(), 0, 0));
}

QPDFObjectHandle
QPDF::newStream(std::shared_ptr<Buffer> data)
{
    auto result = newStream();
    result.replaceStreamData(data, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
    return result;
}

QPDFObjectHandle
QPDF::newStream(std::string const& data)
{
    auto result = newStream();
    result.replaceStreamData(data, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
    return result;
}

QPDFObjectHandle
QPDF::getObject(int objid, int generation)
{
    return getObject(QPDFObjGen(objid, generation));
}

QPDFObjectHandle
QPDF::getObjectByObjGen(QPDFObjGen og)
{
    return getObject(og);
}

QPDFObjectHandle
QPDF::getObjectByID(int objid, int generation)
{
    return getObject(QPDFObjGen(objid, generation));
}

QPDFObjectHandle
QPDF::copyForeignObject(QPDFObjectHandle foreign)
{
    return m->objects.foreign().copied(foreign);
}

Objects ::Foreign::Copier&
Objects::Foreign::copier(QPDFObjectHandle const& foreign)
{
    if (!foreign.isIndirect()) {
        throw std::logic_error("QPDF::copyForeign called with direct object handle");
    }
    QPDF& other = *foreign.qpdf();
    if (&other == &qpdf) {
        throw std::logic_error("QPDF::copyForeign called with object from this QPDF");
    }
    return copiers.insert({other.getUniqueId(), {qpdf}}).first->second;
}

QPDFObjectHandle
Objects::Foreign::Copier::copied(QPDFObjectHandle const& foreign)
{
    // Here's an explanation of what's going on here.
    //
    // A QPDFObjectHandle that is an indirect object has an owning QPDF. The object ID and
    // generation refers to an object in the owning QPDF. When we copy the QPDFObjectHandle from a
    // foreign QPDF into the local QPDF, we have to replace all indirect object references with
    // references to the corresponding object in the local file.
    //
    // To do this, we maintain mappings from foreign object IDs to local object IDs for each foreign
    // QPDF that we are copying from. The mapping is stored in an Foreign::Copier, which contains a
    // mapping from the foreign ObjGen to the local QPDFObjectHandle.
    //
    // To copy, we do a deep traversal of the foreign object with loop detection to discover all
    // indirect objects that are encountered, stopping at page boundaries. Whenever we encounter an
    // indirect object, we check to see if we have already created a local copy of it. If not, we
    // allocate a "reserved" object (or, for a stream, just a new stream) and store in the map the
    // mapping from the foreign object ID to the new object. While we
    // do this, we keep a list of objects to copy.
    //
    // Once we are done with the traversal, we copy all the objects that we need to copy. However,
    // the copies will contain indirect object IDs that refer to objects in the foreign file. We
    // need to replace them with references to objects in the local file. This is what
    // replaceForeignIndirectObjects does. Once we have created a copy of the foreign object with
    // all the indirect references replaced with new ones in the local context, we can replace the
    // local reserved object with the copy. This mechanism allows us to copy objects with circular
    // references in any order.

    // For streams, rather than copying the objects, we set up the stream data to pull from the
    // original stream by using a stream data provider. This is done in a manner that doesn't
    // require the original QPDF object but may require the original source of the stream data with
    // special handling for immediate_copy_from. This logic is also in
    // replaceForeignIndirectObjects.

    // Note that we explicitly allow use of copyForeignObject on page objects. It is a documented
    // use case to copy pages this way if the intention is to not update the pages tree.

    util::assertion(
        visiting.empty(), "obj_copier.visiting is not empty at the beginning of copyForeignObject");

    // Make sure we have an object in this file for every referenced object in the old file.
    // obj_copier.object_map maps foreign QPDFObjGen to local objects.  For everything new that we
    // have to copy, the local object will be a reservation, unless it is a stream, in which case
    // the local object will already be a stream.
    reserve_objects(foreign, true);

    util::assertion(visiting.empty(), "obj_copier.visiting is not empty after reserving objects");

    // Copy any new objects and replace the reservations.
    for (auto& oh: to_copy) {
        auto copy = replace_indirect_object(oh, true);
        if (!oh.isStream()) {
            qpdf.replaceReserved(object_map[oh], copy);
        }
    }
    to_copy.clear();

    auto og = foreign.getObjGen();
    if (!object_map.contains(og)) {
        warn(damagedPDF(
            foreign.qpdf()->getFilename() + " object " + og.unparse(' '),
            foreign.offset(),
            "unexpected reference to /Pages object while copying foreign object; replacing with "
            "null"));
        return QPDFObjectHandle::newNull();
    }
    return object_map[foreign];
}

void
Objects::Foreign::Copier::reserve_objects(QPDFObjectHandle const& foreign, bool top)
{
    auto foreign_tc = foreign.type_code();
    util::assertion(
        foreign_tc != ::ot_reserved, "QPDF: attempting to copy a foreign reserved object");

    if (foreign.isPagesObject()) {
        return;
    }

    if (foreign.indirect()) {
        QPDFObjGen foreign_og(foreign.getObjGen());
        if (!visiting.add(foreign_og)) {
            return;
        }
        if (object_map.contains(foreign_og)) {
            if (!(top && foreign.isPageObject() && object_map[foreign_og].null())) {
                visiting.erase(foreign);
                return;
            }
        } else {
            object_map[foreign_og] = foreign.isStream() ? qpdf.newStream() : qpdf.newIndirectNull();
            if (!top && foreign.isPageObject()) {
                visiting.erase(foreign_og);
                return;
            }
        }
        to_copy.emplace_back(foreign);
    }

    if (foreign_tc == ::ot_array) {
        for (auto const& item: Array(foreign)) {
            reserve_objects(item);
        }
    } else if (foreign_tc == ::ot_dictionary) {
        for (auto const& item: Dictionary(foreign)) {
            if (!item.second.null()) {
                reserve_objects(item.second);
            }
        }
    } else if (foreign_tc == ::ot_stream) {
        reserve_objects(foreign.getDict());
    }

    visiting.erase(foreign);
}

QPDFObjectHandle
Objects::Foreign::Copier::replace_indirect_object(QPDFObjectHandle const& foreign, bool top)
{
    auto foreign_tc = foreign.type_code();

    if (!top && foreign.indirect()) {
        auto mapping = object_map.find(foreign.id_gen());
        if (mapping == object_map.end()) {
            // This case would occur if this is a reference to a Pages object that we didn't
            // traverse into.
            return QPDFObjectHandle::newNull();
        }
        return mapping->second;
    }

    if (foreign_tc == ::ot_array) {
        Array array = foreign;
        std::vector<QPDFObjectHandle> result;
        result.reserve(array.size());
        for (auto const& item: array) {
            result.emplace_back(replace_indirect_object(item));
        }
        return Array(std::move(result));
    }

    if (foreign_tc == ::ot_dictionary) {
        auto result = Dictionary::empty();
        for (auto const& [key, value]: Dictionary(foreign)) {
            if (!value.null()) {
                result.replaceKey(key, replace_indirect_object(value));
            }
        }
        return result;
    }

    if (foreign_tc == ::ot_stream) {
        Stream stream = foreign;
        Stream result = object_map[foreign];
        auto dict = result.getDict();
        for (auto const& [key, value]: stream.getDict()) {
            if (!value.null()) {
                dict.replaceKey(key, replace_indirect_object(value));
            }
        }
        stream.copy_data_to(result);
        return result;
    }

    foreign.assertScalar();
    auto result = foreign;
    result.makeDirect();
    return result;
}

unsigned long long
QPDF::getUniqueId() const
{
    return m->unique_id;
}

std::string
QPDF::getFilename() const
{
    return m->file->getName();
}

PDFVersion
QPDF::getVersionAsPDFVersion()
{
    int major = 1;
    int minor = 3;
    int extension_level = getExtensionLevel();

    std::regex v("^[[:space:]]*([0-9]+)\\.([0-9]+)");
    std::smatch match;
    if (std::regex_search(m->pdf_version, match, v)) {
        major = QUtil::string_to_int(match[1].str().c_str());
        minor = QUtil::string_to_int(match[2].str().c_str());
    }

    return {major, minor, extension_level};
}

std::string
QPDF::getPDFVersion() const
{
    return m->pdf_version;
}

int
QPDF::getExtensionLevel()
{
    int result = 0;
    QPDFObjectHandle obj = getRoot();
    if (obj.hasKey("/Extensions")) {
        obj = obj.getKey("/Extensions");
        if (obj.isDictionary() && obj.hasKey("/ADBE")) {
            obj = obj.getKey("/ADBE");
            if (obj.isDictionary() && obj.hasKey("/ExtensionLevel")) {
                obj = obj.getKey("/ExtensionLevel");
                if (obj.isInteger()) {
                    result = obj.getIntValueAsInt();
                }
            }
        }
    }
    return result;
}

QPDFObjectHandle
QPDF::getTrailer()
{
    return m->trailer;
}

QPDFObjectHandle
QPDF::getRoot()
{
    QPDFObjectHandle root = m->trailer.getKey("/Root");
    if (!root.isDictionary()) {
        throw m->c.damagedPDF("", -1, "unable to find /Root dictionary");
    } else if (
        // Check_mode is an interim solution to request #810 pending a more comprehensive review of
        // the approach to more extensive checks and warning levels.
        m->cf.check_mode() && !root.getKey("/Type").isNameAndEquals("/Catalog")) {
        warn(m->c.damagedPDF("", -1, "catalog /Type entry missing or invalid"));
        root.replaceKey("/Type", "/Catalog"_qpdf);
    }
    return root;
}

std::map<QPDFObjGen, QPDFXRefEntry>
QPDF::getXRefTable()
{
    return m->objects.xref_table();
}

std::map<QPDFObjGen, QPDFXRefEntry> const&
Objects::xref_table()
{
    if (!m->parsed) {
        throw std::logic_error("QPDF::getXRefTable called before parsing.");
    }

    return m->xref_table;
}

bool
QPDF::pipeStreamData(
    std::shared_ptr<EncryptionParameters> encp,
    std::shared_ptr<InputSource> file,
    QPDF& qpdf_for_warning,
    QPDFObjGen og,
    qpdf_offset_t offset,
    size_t length,
    QPDFObjectHandle stream_dict,
    bool is_root_metadata,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    std::unique_ptr<Pipeline> to_delete;
    if (encp->encrypted) {
        decryptStream(
            encp, file, qpdf_for_warning, pipeline, og, stream_dict, is_root_metadata, to_delete);
    }

    bool attempted_finish = false;
    try {
        auto buf = file->read(length, offset);
        if (buf.size() != length) {
            throw qpdf_for_warning.m->c.damagedPDF(
                *file,
                "",
                offset + QIntC::to_offset(buf.size()),
                "unexpected EOF reading stream data");
        }
        pipeline->write(buf.data(), length);
        attempted_finish = true;
        pipeline->finish();
        return true;
    } catch (QPDFExc& e) {
        if (!suppress_warnings) {
            qpdf_for_warning.warn(e);
        }
    } catch (std::exception& e) {
        if (!suppress_warnings) {
            QTC::TC("qpdf", "QPDF decoding error warning");
            qpdf_for_warning.warn(
                // line-break
                qpdf_for_warning.m->c.damagedPDF(
                    *file,
                    "",
                    file->getLastOffset(),
                    ("error decoding stream data for object " + og.unparse(' ') + ": " +
                     e.what())));
            if (will_retry) {
                qpdf_for_warning.warn(
                    // line-break
                    qpdf_for_warning.m->c.damagedPDF(
                        *file,
                        "",
                        file->getLastOffset(),
                        "stream will be re-processed without filtering to avoid data loss"));
            }
        }
    }
    if (!attempted_finish) {
        try {
            pipeline->finish();
        } catch (std::exception&) {
            // ignore
        }
    }
    return false;
}

bool
QPDF::pipeStreamData(
    QPDFObjGen og,
    qpdf_offset_t offset,
    size_t length,
    QPDFObjectHandle stream_dict,
    bool is_root_metadata,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    return pipeStreamData(
        m->encp,
        m->file,
        *this,
        og,
        offset,
        length,
        stream_dict,
        is_root_metadata,
        pipeline,
        suppress_warnings,
        will_retry);
}

// Throw a generic exception when we lack context for something more specific. New code should not
// use this.
void
Common::stopOnError(std::string const& message)
{
    throw damagedPDF("", message);
}

// Return an exception of type qpdf_e_damaged_pdf.
QPDFExc
Common::damagedPDF(
    InputSource& input, std::string const& object, qpdf_offset_t offset, std::string const& message)
{
    return {qpdf_e_damaged_pdf, input.getName(), object, offset, message, true};
}

// Return an exception of type qpdf_e_damaged_pdf.  The object is taken from
// m->last_object_description.
QPDFExc
Common::damagedPDF(InputSource& input, qpdf_offset_t offset, std::string const& message) const
{
    return damagedPDF(input, m->last_object_description, offset, message);
}

// Return an exception of type qpdf_e_damaged_pdf.  The filename is taken from m->file.
QPDFExc
Common::damagedPDF(
    std::string const& object, qpdf_offset_t offset, std::string const& message) const
{
    return {qpdf_e_damaged_pdf, m->file->getName(), object, offset, message, true};
}

// Return an exception of type qpdf_e_damaged_pdf.  The filename is taken from m->file and the
// offset from .m->file->getLastOffset().
QPDFExc
Common::damagedPDF(std::string const& object, std::string const& message) const
{
    return damagedPDF(object, m->file->getLastOffset(), message);
}

// Return an exception of type qpdf_e_damaged_pdf. The filename is taken from m->file and the object
// from .m->last_object_description.
QPDFExc
Common::damagedPDF(qpdf_offset_t offset, std::string const& message) const
{
    return damagedPDF(m->last_object_description, offset, message);
}

// Return an exception of type qpdf_e_damaged_pdf.  The filename is taken from m->file, the object
// from m->last_object_description and the offset from m->file->getLastOffset().
QPDFExc
Common::damagedPDF(std::string const& message) const
{
    return damagedPDF(m->last_object_description, m->file->getLastOffset(), message);
}

bool
QPDF::everCalledGetAllPages() const
{
    return m->pages.ever_called_get_all_pages();
}

bool
QPDF::everPushedInheritedAttributesToPages() const
{
    return m->pages.ever_pushed_inherited_attributes_to_pages();
}

void
QPDF::removeSecurityRestrictions()
{
    auto root = getRoot();
    root.removeKey("/Perms");
    auto acroform = root.getKey("/AcroForm");
    if (acroform.isDictionary() && acroform.hasKey("/SigFlags")) {
        acroform.replaceKey("/SigFlags", QPDFObjectHandle::newInteger(0));
    }
}
