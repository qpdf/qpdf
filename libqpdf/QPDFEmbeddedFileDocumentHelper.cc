#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>

// File attachments are stored in the /EmbeddedFiles (name tree) key
// of the /Names dictionary from the document catalog. Each entry
// points to a /FileSpec, which in turn points to one more Embedded
// File Streams. Note that file specs can appear in other places as
// well, such as file attachment annotations, among others.
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

QPDFEmbeddedFileDocumentHelper::QPDFEmbeddedFileDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(new Members())
{
    auto root = qpdf.getRoot();
    auto names = root.getKey("/Names");
    if (names.isDictionary()) {
        auto embedded_files = names.getKey("/EmbeddedFiles");
        if (embedded_files.isDictionary()) {
            this->m->embedded_files =
                std::make_shared<QPDFNameTreeObjectHelper>(
                    embedded_files, qpdf);
        }
    }
}

bool
QPDFEmbeddedFileDocumentHelper::hasEmbeddedFiles() const
{
    return (this->m->embedded_files.get() != nullptr);
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
        names =
            root.replaceKeyAndGet("/Names", QPDFObjectHandle::newDictionary());
    }
    auto embedded_files = names.getKey("/EmbeddedFiles");
    if (!embedded_files.isDictionary()) {
        auto nth = QPDFNameTreeObjectHelper::newEmpty(this->qpdf);
        names.replaceKey("/EmbeddedFiles", nth.getObjectHandle());
        this->m->embedded_files =
            std::make_shared<QPDFNameTreeObjectHelper>(nth);
    }
}

std::shared_ptr<QPDFFileSpecObjectHelper>
QPDFEmbeddedFileDocumentHelper::getEmbeddedFile(std::string const& name)
{
    std::shared_ptr<QPDFFileSpecObjectHelper> result;
    if (this->m->embedded_files) {
        auto i = this->m->embedded_files->find(name);
        if (i != this->m->embedded_files->end()) {
            result = std::make_shared<QPDFFileSpecObjectHelper>(i->second);
        }
    }
    return result;
}

std::map<std::string, std::shared_ptr<QPDFFileSpecObjectHelper>>
QPDFEmbeddedFileDocumentHelper::getEmbeddedFiles()
{
    std::map<std::string, std::shared_ptr<QPDFFileSpecObjectHelper>> result;
    if (this->m->embedded_files) {
        for (auto const& i : *(this->m->embedded_files)) {
            result[i.first] =
                std::make_shared<QPDFFileSpecObjectHelper>(i.second);
        }
    }
    return result;
}

void
QPDFEmbeddedFileDocumentHelper::replaceEmbeddedFile(
    std::string const& name, QPDFFileSpecObjectHelper const& fs)
{
    initEmbeddedFiles();
    this->m->embedded_files->insert(name, fs.getObjectHandle());
}

bool
QPDFEmbeddedFileDocumentHelper::removeEmbeddedFile(std::string const& name)
{
    if (!hasEmbeddedFiles()) {
        return false;
    }
    auto iter = this->m->embedded_files->find(name);
    if (iter == this->m->embedded_files->end()) {
        return false;
    }
    auto oh = iter->second;
    iter.remove();
    if (oh.isIndirect()) {
        this->qpdf.replaceObject(oh.getObjGen(), QPDFObjectHandle::newNull());
    }

    return true;
}
