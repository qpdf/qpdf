#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDF.hh>

QPDFPageObjectHelper::Members::~Members()
{
}

QPDFPageObjectHelper::Members::Members()
{
}

QPDFPageObjectHelper::QPDFPageObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh)
{
}

std::map<std::string, QPDFObjectHandle>
QPDFPageObjectHelper::getPageImages()
{
    return this->oh.getPageImages();
}

std::vector<QPDFAnnotationObjectHelper>
QPDFPageObjectHelper::getAnnotations(std::string const& only_subtype)
{
    std::vector<QPDFAnnotationObjectHelper> result;
    QPDFObjectHandle annots = this->oh.getKey("/Annots");
    if (annots.isArray())
    {
        size_t nannots = annots.getArrayNItems();
        for (size_t i = 0; i < nannots; ++i)
        {
            QPDFObjectHandle annot = annots.getArrayItem(i);
            if (only_subtype.empty() ||
                (annot.isDictionary() &&
                 annot.getKey("/Subtype").isName() &&
                 (only_subtype == annot.getKey("/Subtype").getName())))
            {
                result.push_back(QPDFAnnotationObjectHelper(annot));
            }
        }
    }
    return result;
}

std::vector<QPDFObjectHandle>
QPDFPageObjectHelper::getPageContents()
{
    return this->oh.getPageContents();
}

void
QPDFPageObjectHelper::addPageContents(QPDFObjectHandle contents, bool first)
{
    this->oh.addPageContents(contents, first);
}

void
QPDFPageObjectHelper::rotatePage(int angle, bool relative)
{
    this->oh.rotatePage(angle, relative);
}

void
QPDFPageObjectHelper::coalesceContentStreams()
{
    this->oh.coalesceContentStreams();
}

void
QPDFPageObjectHelper::parsePageContents(
    QPDFObjectHandle::ParserCallbacks* callbacks)
{
    this->oh.parsePageContents(callbacks);
}

void
QPDFPageObjectHelper::filterPageContents(
    QPDFObjectHandle::TokenFilter* filter,
    Pipeline* next)
{
    this->oh.filterPageContents(filter, next);
}

void
QPDFPageObjectHelper::pipePageContents(Pipeline* p)
{
    this->oh.pipePageContents(p);
}

void
QPDFPageObjectHelper::addContentTokenFilter(
    PointerHolder<QPDFObjectHandle::TokenFilter> token_filter)
{
    this->oh.addContentTokenFilter(token_filter);
}

class NameWatcher: public QPDFObjectHandle::TokenFilter
{
  public:
    virtual ~NameWatcher()
    {
    }
    virtual void handleToken(QPDFTokenizer::Token const&);
    std::set<std::string> names;
};

void
NameWatcher::handleToken(QPDFTokenizer::Token const& token)
{
    if (token.getType() == QPDFTokenizer::tt_name)
    {
        // Create a name object and get its name. This canonicalizes
        // the representation of the name
        this->names.insert(
            QPDFObjectHandle::newName(token.getValue()).getName());
    }
    writeToken(token);
}

void
QPDFPageObjectHelper::removeUnreferencedResources()
{
    NameWatcher nw;
    try
    {
        filterPageContents(&nw);
    }
    catch (std::exception& e)
    {
        this->oh.warnIfPossible(
            std::string("Unable to parse content stream: ") + e.what() +
            "; not attempting to remove unreferenced objects from this page");
        return;
    }
    // Walk through /Font and /XObject dictionaries, removing any
    // resources that are not referenced. We must make copies of
    // resource dictionaries down into the dictionaries are mutating
    // to prevent mutating one dictionary from having the side effect
    // of mutating the one it was copied from.
    std::vector<std::string> to_filter;
    to_filter.push_back("/Font");
    to_filter.push_back("/XObject");
    QPDFObjectHandle resources = this->oh.getKey("/Resources");
    if (resources.isDictionary())
    {
        resources = resources.shallowCopy();
        this->oh.replaceKey("/Resources", resources);
    }
    for (std::vector<std::string>::iterator d_iter = to_filter.begin();
         d_iter != to_filter.end(); ++d_iter)
    {
        QPDFObjectHandle dict = resources.getKey(*d_iter);
        if (! dict.isDictionary())
        {
            continue;
        }
        dict = dict.shallowCopy();
        resources.replaceKey(*d_iter, dict);
        std::set<std::string> keys = dict.getKeys();
        for (std::set<std::string>::iterator k_iter = keys.begin();
             k_iter != keys.end(); ++k_iter)
        {
            if (! nw.names.count(*k_iter))
            {
                dict.removeKey(*k_iter);
            }
        }
    }
}

QPDFPageObjectHelper
QPDFPageObjectHelper::shallowCopyPage()
{
    QPDF* qpdf = this->oh.getOwningQPDF();
    if (! qpdf)
    {
        throw std::runtime_error(
            "QPDFPageObjectHelper::shallowCopyPage"
            " called with a direct objet");
    }
    QPDFObjectHandle new_page = this->oh.shallowCopy();
    return QPDFPageObjectHelper(qpdf->makeIndirectObject(new_page));
}
