#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>

#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDF_private.hh>

using namespace qpdf;

// File attachments are stored in the /EmbeddedFiles (name tree) key of the /Names dictionary from
// the document catalog. Each entry points to a /FileSpec, which in turn points to one more Embedded
// File Streams. Note that file specs can appear in other places as well, such as file attachment
// annotations, among others.
//
// root -> /Names -> /EmbeddedFiles = name tree
// filename -> filespec
// <<
//   /Desc ()
//   /EF <<
//     /F x 0 R
//     /UF x 0 R
//   >>
//   /F (name)
//   /UF (name)
//   /Type /Filespec
// >>
// x 0 obj
// <<
//   /Type /EmbeddedFile
//   /DL filesize % not in spec?
//   /Params <<
//     /CheckSum <md5>
//     /CreationDate (D:yyyymmddhhmmss{-hh'mm'|+hh'mm'|Z})
//     /ModDate (D:yyyymmddhhmmss-hh'mm')
//     /Size filesize
//     /Subtype /mime#2ftype
//   >>
// >>

class QPDFEmbeddedFileDocumentHelper::Members
{
  public:
    Members() = default;
    Members(Members const&) = delete;
    ~Members() = default;

    std::unique_ptr<QPDFNameTreeObjectHelper> embedded_files;
};

QPDFEmbeddedFileDocumentHelper::QPDFEmbeddedFileDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(std::make_shared<Members>())
{
    validate();
}

QPDFEmbeddedFileDocumentHelper&
QPDFEmbeddedFileDocumentHelper::get(QPDF& qpdf)
{
    return qpdf.doc().embedded_files();
}

void
QPDFEmbeddedFileDocumentHelper::validate(bool repair)
{
    m->embedded_files.reset();
    auto names = qpdf.getRoot().getKey("/Names");
    if (names.isDictionary()) {
        auto embedded_files = names.getKey("/EmbeddedFiles");
        if (embedded_files.isDictionary()) {
            m->embedded_files = std::make_unique<QPDFNameTreeObjectHelper>(
                embedded_files,
                qpdf,
                [](QPDFObjectHandle const& o) -> bool { return o.isDictionary(); },
                true);
            m->embedded_files->validate(repair);
        }
    }
}

bool
QPDFEmbeddedFileDocumentHelper::hasEmbeddedFiles() const
{
    return (m->embedded_files != nullptr);
}

void
QPDFEmbeddedFileDocumentHelper::initEmbeddedFiles()
{
    if (hasEmbeddedFiles()) {
        return;
    }
    auto root = qpdf.getRoot();
    auto names = root.getKey("/Names");
    if (!names.isDictionary()) {
        names = root.replaceKeyAndGetNew("/Names", QPDFObjectHandle::newDictionary());
    }
    auto embedded_files = names.getKey("/EmbeddedFiles");
    if (!embedded_files.isDictionary()) {
        auto nth = QPDFNameTreeObjectHelper::newEmpty(qpdf);
        names.replaceKey("/EmbeddedFiles", nth.getObjectHandle());
        m->embedded_files = std::make_unique<QPDFNameTreeObjectHelper>(
            nth, qpdf, [](QPDFObjectHandle const& o) -> bool { return o.isDictionary(); }, true);
    }
}

std::shared_ptr<QPDFFileSpecObjectHelper>
QPDFEmbeddedFileDocumentHelper::getEmbeddedFile(std::string const& name)
{
    std::shared_ptr<QPDFFileSpecObjectHelper> result;
    if (m->embedded_files) {
        auto i = m->embedded_files->find(name);
        if (i != m->embedded_files->end()) {
            result = std::make_shared<QPDFFileSpecObjectHelper>(i->second);
        }
    }
    return result;
}

std::map<std::string, std::shared_ptr<QPDFFileSpecObjectHelper>>
QPDFEmbeddedFileDocumentHelper::getEmbeddedFiles()
{
    std::map<std::string, std::shared_ptr<QPDFFileSpecObjectHelper>> result;
    if (m->embedded_files) {
        for (auto const& i: *(m->embedded_files)) {
            result[i.first] = std::make_shared<QPDFFileSpecObjectHelper>(i.second);
        }
    }
    return result;
}

// A file spec may also be referenced from the /AF (associated files) array of the document catalog.
// When an attachment is removed or replaced, its file spec must be dropped from that array as well.
// Otherwise the array keeps a reference to a null object, or keeps the old file spec (and its
// embedded file stream) alive in the output file.
static void
remove_from_associated_files(QPDF& qpdf, QPDFObjectHandle const& fs)
{
    if (!fs.isIndirect()) {
        return;
    }
    auto root = qpdf.getRoot();
    auto af = root.getKey("/AF");
    if (!af.isArray()) {
        return;
    }
    for (int i = af.getArrayNItems() - 1; i >= 0; --i) {
        auto item = af.getArrayItem(i);
        if (item.isIndirect() && item.getObjGen() == fs.getObjGen()) {
            af.eraseItem(i);
        }
    }
    if (af.getArrayNItems() == 0) {
        root.removeKey("/AF");
    }
}

void
QPDFEmbeddedFileDocumentHelper::replaceEmbeddedFile(
    std::string const& name, QPDFFileSpecObjectHelper const& fs)
{
    initEmbeddedFiles();
    auto iter = m->embedded_files->find(name);
    if (iter != m->embedded_files->end() &&
        !(iter->second.isIndirect() && fs.getObjectHandle().isIndirect() &&
          iter->second.getObjGen() == fs.getObjectHandle().getObjGen())) {
        remove_from_associated_files(qpdf, iter->second);
    }
    m->embedded_files->insert(name, fs.getObjectHandle());
}

bool
QPDFEmbeddedFileDocumentHelper::removeEmbeddedFile(std::string const& name)
{
    if (!hasEmbeddedFiles()) {
        return false;
    }
    auto iter = m->embedded_files->find(name);
    if (iter == m->embedded_files->end()) {
        return false;
    }
    if (iter->second.indirect()) {
        remove_from_associated_files(qpdf, iter->second);
        qpdf.replaceObject(iter->second, Null());
    }
    iter.remove();

    return true;
}
