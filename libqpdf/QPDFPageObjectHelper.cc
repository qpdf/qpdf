#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/Pl_Concatenate.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFMatrix.hh>
#include <qpdf/QIntC.hh>

class ContentProvider: public QPDFObjectHandle::StreamDataProvider
{
  public:
    ContentProvider(QPDFObjectHandle from_page) :
        from_page(from_page)
    {
    }
    virtual ~ContentProvider()
    {
    }
    virtual void provideStreamData(int objid, int generation,
                                   Pipeline* pipeline);

  private:
    QPDFObjectHandle from_page;
};

void
ContentProvider::provideStreamData(int, int, Pipeline* p)
{
    Pl_Concatenate concat("concatenate", p);
    std::string description = "contents from page object " +
        QUtil::int_to_string(from_page.getObjectID()) + " " +
        QUtil::int_to_string(from_page.getGeneration());
    std::string all_description;
    from_page.getKey("/Contents").pipeContentStreams(
        &concat, description, all_description);
    concat.manualFinish();
}

class InlineImageTracker: public QPDFObjectHandle::TokenFilter
{
  public:
    InlineImageTracker(QPDF*, size_t min_size, QPDFObjectHandle resources);
    virtual ~InlineImageTracker()
    {
    }
    virtual void handleToken(QPDFTokenizer::Token const&);
    QPDFObjectHandle convertIIDict(QPDFObjectHandle odict);

    QPDF* qpdf;
    size_t min_size;
    QPDFObjectHandle resources;
    std::string dict_str;
    std::string bi_str;
    int min_suffix;
    bool any_images;
    enum { st_top, st_bi } state;
};

InlineImageTracker::InlineImageTracker(QPDF* qpdf, size_t min_size,
                                       QPDFObjectHandle resources) :
    qpdf(qpdf),
    min_size(min_size),
    resources(resources),
    min_suffix(1),
    any_images(false),
    state(st_top)
{
}

QPDFObjectHandle
InlineImageTracker::convertIIDict(QPDFObjectHandle odict)
{
    QPDFObjectHandle dict = QPDFObjectHandle::newDictionary();
    dict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    dict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    std::set<std::string> keys = odict.getKeys();
    for (auto key: keys)
    {
        QPDFObjectHandle value = odict.getKey(key);
        if (key == "/BPC")
        {
            key = "/BitsPerComponent";
        }
        else if (key == "/CS")
        {
            key = "/ColorSpace";
        }
        else if (key == "/D")
        {
            key = "/Decode";
        }
        else if (key == "/DP")
        {
            key = "/DecodeParms";
        }
        else if (key == "/F")
        {
            key = "/Filter";
        }
        else if (key == "/H")
        {
            key = "/Height";
        }
        else if (key == "/IM")
        {
            key = "/ImageMask";
        }
        else if (key == "/I")
        {
            key = "/Interpolate";
        }
        else if (key == "/W")
        {
            key = "/Width";
        }

        if (key == "/ColorSpace")
        {
            if (value.isName())
            {
                std::string name = value.getName();
                if (name == "/G")
                {
                    name = "/DeviceGray";
                }
                else if (name == "/RGB")
                {
                    name = "/DeviceRGB";
                }
                else if (name == "/CMYK")
                {
                    name = "/DeviceCMYK";
                }
                else if (name == "/I")
                {
                    name = "/Indexed";
                }
                else
                {
                    // This is a key in the page's /Resources ->
                    // /ColorSpace dictionary. We need to look it up
                    // and use its value as the color space for the
                    // image.
                    QPDFObjectHandle colorspace =
                        resources.getKey("/ColorSpace");
                    if (colorspace.isDictionary() && colorspace.hasKey(name))
                    {
                        QTC::TC("qpdf", "QPDFPageObjectHelper colorspace lookup");
                        value = colorspace.getKey(name);
                    }
                    else
                    {
                        resources.warnIfPossible(
                            "unable to resolve colorspace " + name);
                    }
                    name.clear();
                }
                if (! name.empty())
                {
                    value = QPDFObjectHandle::newName(name);
                }
            }
        }
        else if (key == "/Filter")
        {
            std::vector<QPDFObjectHandle> filters;
            if (value.isName())
            {
                filters.push_back(value);
            }
            else if (value.isArray())
            {
                filters = value.getArrayAsVector();
            }
            for (auto& iter: filters)
            {
                std::string name;
                if (iter.isName())
                {
                    name = iter.getName();
                }
                if (name == "/AHx")
                {
                    name = "/ASCIIHexDecode";
                }
                else if (name == "/A85")
                {
                    name = "/ASCII85Decode";
                }
                else if (name == "/LZW")
                {
                    name = "/LZWDecode";
                }
                else if (name == "/Fl")
                {
                    name = "/FlateDecode";
                }
                else if (name == "/RL")
                {
                    name = "/RunLengthDecode";
                }
                else if (name == "/CCF")
                {
                    name = "/CCITTFaxDecode";
                }
                else if (name == "/DCT")
                {
                    name = "/DCTDecode";
                }
                else
                {
                    name.clear();
                }
                if (! name.empty())
                {
                    iter = QPDFObjectHandle::newName(name);
                }
            }
            if (value.isName() && (filters.size() == 1))
            {
                value = filters.at(0);
            }
            else if (value.isArray())
            {
                value = QPDFObjectHandle::newArray(filters);
            }
        }
        dict.replaceKey(key, value);
    }
    return dict;
}

void
InlineImageTracker::handleToken(QPDFTokenizer::Token const& token)
{
    if (state == st_bi)
    {
        if (token.getType() == QPDFTokenizer::tt_inline_image)
        {
            std::string image_data(token.getValue());
            size_t len = image_data.length();
            if (len >= this->min_size)
            {
                QTC::TC("qpdf", "QPDFPageObjectHelper externalize inline image");
                Pl_Buffer b("image_data");
                b.write(QUtil::unsigned_char_pointer(image_data), len);
                b.finish();
                QPDFObjectHandle dict =
                    convertIIDict(QPDFObjectHandle::parse(dict_str));
                dict.replaceKey(
                    "/Length",
                    QPDFObjectHandle::newInteger(QIntC::to_longlong(len)));
                std::string name = resources.getUniqueResourceName(
                    "/IIm", this->min_suffix);
                QPDFObjectHandle image = QPDFObjectHandle::newStream(
                    this->qpdf, b.getBuffer());
                image.replaceDict(dict);
                resources.getKey("/XObject").replaceKey(name, image);
                write(name);
                write(" Do\n");
                any_images = true;
            }
            else
            {
                QTC::TC("qpdf", "QPDFPageObjectHelper keep inline image");
                write(bi_str);
                writeToken(token);
                state = st_top;
            }
        }
        else if (token == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "ID"))
        {
            bi_str += token.getValue();
            dict_str += " >>";
        }
        else if (token == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "EI"))
        {
            state = st_top;
        }
        else
        {
            bi_str += token.getValue();
            dict_str += token.getValue();
        }
    }
    else if (token == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "BI"))
    {
        bi_str = token.getValue();
        dict_str = "<< ";
        state = st_bi;
    }
    else
    {
        writeToken(token);
    }
}

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

QPDFObjectHandle
QPDFPageObjectHelper::getAttribute(std::string const& name,
                                   bool copy_if_shared)
{
    QPDFObjectHandle result;
    QPDFObjectHandle dict;
    bool is_form_xobject = this->oh.isFormXObject();
    bool inherited = false;
    if (is_form_xobject)
    {
        dict = this->oh.getDict();
        result = dict.getKey(name);
    }
    else
    {
        dict = this->oh;
        bool inheritable = ((name == "/MediaBox") || (name == "/CropBox") ||
                            (name == "/Resources") || (name == "/Rotate"));

        QPDFObjectHandle node = dict;
        result = node.getKey(name);
        std::set<QPDFObjGen> seen;
        while (inheritable && result.isNull() && node.hasKey("/Parent"))
        {
            seen.insert(node.getObjGen());
            node = node.getKey("/Parent");
            if (seen.count(node.getObjGen()))
            {
                break;
            }
            result = node.getKey(name);
            if (! result.isNull())
            {
                QTC::TC("qpdf", "QPDFPageObjectHelper non-trivial inheritance");
                inherited = true;
            }
        }
    }
    if (copy_if_shared && (inherited || result.isIndirect()))
    {
        QTC::TC("qpdf", "QPDFPageObjectHelper copy shared attribute",
                is_form_xobject ? 0 : 1);
        result = result.shallowCopy();
        dict.replaceKey(name, result);
    }
    return result;
}

QPDFObjectHandle
QPDFPageObjectHelper::getTrimBox(bool copy_if_shared)
{
    QPDFObjectHandle result = getAttribute("/TrimBox", copy_if_shared);
    if (result.isNull())
    {
        result = getCropBox(copy_if_shared);
    }
    return result;
}

QPDFObjectHandle
QPDFPageObjectHelper::getCropBox(bool copy_if_shared)
{
    QPDFObjectHandle result = getAttribute("/CropBox", copy_if_shared);
    if (result.isNull())
    {
        result = getMediaBox();
    }
    return result;
}

QPDFObjectHandle
QPDFPageObjectHelper::getMediaBox(bool copy_if_shared)
{
    return getAttribute("/MediaBox", copy_if_shared);
}

void
QPDFPageObjectHelper::forEachXObject(
    bool recursive,
    std::function<void(QPDFObjectHandle& obj,
                       QPDFObjectHandle& xobj_dict,
                       std::string const& key)> action,
    std::function<bool(QPDFObjectHandle)> selector)
{
    QTC::TC("qpdf", "QPDFPageObjectHelper::forEachXObject",
            recursive
            ? (this->oh.isFormXObject() ? 0 : 1)
            : (this->oh.isFormXObject() ? 2 : 3));
    std::set<QPDFObjGen> seen;
    std::list<QPDFPageObjectHelper> queue;
    queue.push_back(*this);
    while (! queue.empty())
    {
        QPDFPageObjectHelper ph = queue.front();
        queue.pop_front();
        QPDFObjGen og = ph.oh.getObjGen();
        if (seen.count(og))
        {
            continue;
        }
        seen.insert(og);
        QPDFObjectHandle resources = ph.getAttribute("/Resources", false);
        if (resources.isDictionary() && resources.hasKey("/XObject"))
        {
            QPDFObjectHandle xobj_dict = resources.getKey("/XObject");
            for (auto const& key: xobj_dict.getKeys())
            {
                QPDFObjectHandle obj = xobj_dict.getKey(key);
                if ((! selector) || selector(obj))
                {
                    action(obj, xobj_dict, key);
                }
                if (recursive && obj.isFormXObject())
                {
                    queue.push_back(QPDFPageObjectHelper(obj));
                }
            }
	}
    }
}

void
QPDFPageObjectHelper::forEachImage(
    bool recursive,
    std::function<void(QPDFObjectHandle& obj,
                       QPDFObjectHandle& xobj_dict,
                       std::string const& key)> action)
{
    forEachXObject(recursive, action,
                   [](QPDFObjectHandle obj) { return obj.isImage(); });
}

void
QPDFPageObjectHelper::forEachFormXObject(
    bool recursive,
    std::function<void(QPDFObjectHandle& obj,
                       QPDFObjectHandle& xobj_dict,
                       std::string const& key)> action)
{
    forEachXObject(recursive, action,
                   [](QPDFObjectHandle obj) { return obj.isFormXObject(); });
}

std::map<std::string, QPDFObjectHandle>
QPDFPageObjectHelper::getPageImages()
{
    return getImages();
}

std::map<std::string, QPDFObjectHandle>
QPDFPageObjectHelper::getImages()
{
    std::map<std::string, QPDFObjectHandle> result;
    forEachImage(false, [&result](QPDFObjectHandle& obj,
                                  QPDFObjectHandle&,
                                  std::string const& key) {
        result[key] = obj;
    });
    return result;
}

std::map<std::string, QPDFObjectHandle>
QPDFPageObjectHelper::getFormXObjects()
{
    std::map<std::string, QPDFObjectHandle> result;
    forEachFormXObject(false, [&result](QPDFObjectHandle& obj,
                                        QPDFObjectHandle&,
                                        std::string const& key) {
        result[key] = obj;
    });
    return result;
}

void
QPDFPageObjectHelper::externalizeInlineImages(size_t min_size)
{
    externalizeInlineImages(min_size, false);
}

void
QPDFPageObjectHelper::externalizeInlineImages(size_t min_size, bool shallow)
{
    if (shallow)
    {
        QPDFObjectHandle resources = getAttribute("/Resources", true);
        // Calling mergeResources also ensures that /XObject becomes
        // direct and is not shared with other pages.
        resources.mergeResources(
            QPDFObjectHandle::parse("<< /XObject << >> >>"));
        InlineImageTracker iit(this->oh.getOwningQPDF(), min_size, resources);
        Pl_Buffer b("new page content");
        bool filtered = false;
        try
        {
            filterContents(&iit, &b);
            filtered = true;
        }
        catch (std::exception& e)
        {
            this->oh.warnIfPossible(
                std::string("Unable to filter content stream: ") + e.what() +
                "; not attempting to externalize inline images"
                " from this stream");
        }
        if (filtered && iit.any_images)
        {
            if (this->oh.isFormXObject())
            {
                this->oh.replaceStreamData(
                    b.getBuffer(),
                    QPDFObjectHandle::newNull(),
                    QPDFObjectHandle::newNull());
            }
            else
            {
                this->oh.replaceKey(
                    "/Contents",
                    QPDFObjectHandle::newStream(
                        this->oh.getOwningQPDF(), b.getBuffer()));
            }
        }
    }
    else
    {
        externalizeInlineImages(min_size, true);
        forEachFormXObject(
            true,
            [min_size](QPDFObjectHandle& obj,
                       QPDFObjectHandle&, std::string const&) {
                QPDFPageObjectHelper(obj).externalizeInlineImages(
                    min_size, true);
            });
    }
}

std::vector<QPDFAnnotationObjectHelper>
QPDFPageObjectHelper::getAnnotations(std::string const& only_subtype)
{
    std::vector<QPDFAnnotationObjectHelper> result;
    QPDFObjectHandle annots = this->oh.getKey("/Annots");
    if (annots.isArray())
    {
        int nannots = annots.getArrayNItems();
        for (int i = 0; i < nannots; ++i)
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
    parseContents(callbacks);
}

void
QPDFPageObjectHelper::parseContents(
    QPDFObjectHandle::ParserCallbacks* callbacks)
{
    if (this->oh.isFormXObject())
    {
        this->oh.parseAsContents(callbacks);
    }
    else
    {
        this->oh.parsePageContents(callbacks);
    }
}

void
QPDFPageObjectHelper::filterPageContents(
    QPDFObjectHandle::TokenFilter* filter,
    Pipeline* next)
{
    return filterContents(filter, next);
}

void
QPDFPageObjectHelper::filterContents(
    QPDFObjectHandle::TokenFilter* filter,
    Pipeline* next)
{
    if (this->oh.isFormXObject())
    {
        this->oh.filterAsContents(filter, next);
    }
    else
    {
        this->oh.filterPageContents(filter, next);
    }
}

void
QPDFPageObjectHelper::pipePageContents(Pipeline* p)
{
    pipeContents(p);
}

void
QPDFPageObjectHelper::pipeContents(Pipeline* p)
{
    if (this->oh.isFormXObject())
    {
        this->oh.pipeStreamData(p, 0, qpdf_dl_specialized);
    }
    else
    {
        this->oh.pipePageContents(p);
    }
}

void
QPDFPageObjectHelper::addContentTokenFilter(
    PointerHolder<QPDFObjectHandle::TokenFilter> token_filter)
{
    if (this->oh.isFormXObject())
    {
        this->oh.addTokenFilter(token_filter);
    }
    else
    {
        this->oh.addContentTokenFilter(token_filter);
    }
}

class NameWatcher: public QPDFObjectHandle::TokenFilter
{
  public:
    NameWatcher() :
        saw_bad(false)
    {
    }
    virtual ~NameWatcher()
    {
    }
    virtual void handleToken(QPDFTokenizer::Token const&);
    std::set<std::string> names;
    bool saw_bad;
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
    else if (token.getType() == QPDFTokenizer::tt_bad)
    {
        saw_bad = true;
    }
    writeToken(token);
}

void
QPDFPageObjectHelper::removeUnreferencedResourcesHelper(
    QPDFPageObjectHelper ph, std::set<QPDFObjGen>& seen)
{
    if (seen.count(ph.oh.getObjGen()))
    {
        return;
    }
    seen.insert(ph.oh.getObjGen());
    NameWatcher nw;
    try
    {
        ph.filterContents(&nw);
    }
    catch (std::exception& e)
    {
        ph.oh.warnIfPossible(
            std::string("Unable to parse content stream: ") + e.what() +
            "; not attempting to remove unreferenced objects from this page");
        return;
    }
    if (nw.saw_bad)
    {
        QTC::TC("qpdf", "QPDFPageObjectHelper bad token finding names");
        ph.oh.warnIfPossible(
            "Bad token found while scanning content stream; "
            "not attempting to remove unreferenced objects from this page");
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
    QPDFObjectHandle resources = ph.getAttribute("/Resources", true);
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
            QPDFObjectHandle resource = dict.getKey(*k_iter);
            if (resource.isFormXObject())
            {
                QTC::TC("qpdf", "QPDFPageObjectHelper filter form xobject");
                removeUnreferencedResourcesHelper(
                    QPDFPageObjectHelper(resource), seen);
            }
        }
    }
}

void
QPDFPageObjectHelper::removeUnreferencedResources()
{
    std::set<QPDFObjGen> seen;
    removeUnreferencedResourcesHelper(*this, seen);
}

QPDFPageObjectHelper
QPDFPageObjectHelper::shallowCopyPage()
{
    QPDF* qpdf = this->oh.getOwningQPDF();
    if (! qpdf)
    {
        throw std::runtime_error(
            "QPDFPageObjectHelper::shallowCopyPage"
            " called with a direct object");
    }
    QPDFObjectHandle new_page = this->oh.shallowCopy();
    return QPDFPageObjectHelper(qpdf->makeIndirectObject(new_page));
}

QPDFObjectHandle::Matrix
QPDFPageObjectHelper::getMatrixForTransformations(bool invert)
{
    QPDFObjectHandle::Matrix matrix(1, 0, 0, 1, 0, 0);
    QPDFObjectHandle bbox = getTrimBox(false);
    if (! bbox.isRectangle())
    {
        return matrix;
    }
    QPDFObjectHandle rotate_obj = getAttribute("/Rotate", false);
    QPDFObjectHandle scale_obj = getAttribute("/UserUnit", false);
    if (! (rotate_obj.isNull() && scale_obj.isNull()))
    {
        QPDFObjectHandle::Rectangle rect = bbox.getArrayAsRectangle();
        double width = rect.urx - rect.llx;
        double height = rect.ury - rect.lly;
        double scale = (scale_obj.isNumber()
                        ? scale_obj.getNumericValue()
                        : 1.0);
        int rotate = (rotate_obj.isInteger()
                      ? rotate_obj.getIntValueAsInt()
                      : 0);
        if (invert)
        {
            if (scale == 0.0)
            {
                return matrix;
            }
            scale = 1.0 / scale;
            rotate = 360 - rotate;
        }

        // Ignore invalid rotation angle
        switch (rotate)
        {
          case 90:
            matrix = QPDFObjectHandle::Matrix(
                0, -scale, scale, 0, 0, width * scale);
            break;
          case 180:
            matrix = QPDFObjectHandle::Matrix(
                -scale, 0, 0, -scale, width * scale, height * scale);
            break;
          case 270:
            matrix = QPDFObjectHandle::Matrix(
                0, scale, -scale, 0, height * scale, 0);
            break;
          default:
            matrix = QPDFObjectHandle::Matrix(
                scale, 0, 0, scale, 0, 0);
            break;
        }
    }
    return matrix;
}

QPDFObjectHandle
QPDFPageObjectHelper::getFormXObjectForPage(bool handle_transformations)
{
    QPDF* qpdf = this->oh.getOwningQPDF();
    if (! qpdf)
    {
        throw std::runtime_error(
            "QPDFPageObjectHelper::getFormXObjectForPage"
            " called with a direct object");
    }
    QPDFObjectHandle result = QPDFObjectHandle::newStream(qpdf);
    QPDFObjectHandle newdict = result.getDict();
    newdict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    newdict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
    newdict.replaceKey("/Resources",
                       getAttribute("/Resources", false).shallowCopy());
    newdict.replaceKey("/Group",
                       getAttribute("/Group", false).shallowCopy());
    QPDFObjectHandle bbox = getTrimBox(false).shallowCopy();
    if (! bbox.isRectangle())
    {
        this->oh.warnIfPossible(
            "bounding box is invalid; form"
            " XObject created from page will not work");
    }
    newdict.replaceKey("/BBox", bbox);
    PointerHolder<QPDFObjectHandle::StreamDataProvider> provider =
        new ContentProvider(this->oh);
    result.replaceStreamData(
        provider, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
    QPDFObjectHandle rotate_obj = getAttribute("/Rotate", false);
    QPDFObjectHandle scale_obj = getAttribute("/UserUnit", false);
    if (handle_transformations &&
        (! (rotate_obj.isNull() && scale_obj.isNull())))
    {
        newdict.replaceKey("/Matrix",
                           QPDFObjectHandle::newArray(
                               getMatrixForTransformations()));
    }

    return result;
}

std::string
QPDFPageObjectHelper::placeFormXObject(
    QPDFObjectHandle fo, std::string const& name,
    QPDFObjectHandle::Rectangle rect,
    bool invert_transformations,
    bool allow_shrink, bool allow_expand)
{
    // Calculate the transformation matrix that will place the given
    // form XObject fully inside the given rectangle, center and
    // shrinking or expanding as needed if requested.

    // When rendering a form XObject, the transformation in the
    // graphics state (cm) is applied first (of course -- when it is
    // applied, the PDF interpreter doesn't even know we're going to
    // be drawing a form XObject yet), and then the object's matrix
    // (M) is applied. The resulting matrix, when applied to the form
    // XObject's bounding box, will generate a new rectangle. We want
    // to create a transformation matrix that make the form XObject's
    // bounding box land in exactly the right spot.

    QPDFObjectHandle fdict = fo.getDict();
    QPDFObjectHandle bbox_obj = fdict.getKey("/BBox");
    if (! bbox_obj.isRectangle())
    {
        return "";
    }

    QPDFMatrix wmatrix;         // work matrix
    QPDFMatrix tmatrix;         // "to" matrix
    QPDFMatrix fmatrix;         // "from" matrix
    if (invert_transformations)
    {
        // tmatrix inverts scaling and rotation of the destination
        // page. Applying this matrix allows the overlaid form
        // XObject's to be absolute rather than relative to properties
        // of the destination page. tmatrix is part of the computed
        // transformation matrix.
        tmatrix = QPDFMatrix(getMatrixForTransformations(true));
        wmatrix.concat(tmatrix);
    }
    if (fdict.getKey("/Matrix").isMatrix())
    {
        // fmatrix is the transformation matrix that is applied to the
        // form XObject itself. We need this for calculations, but we
        // don't explicitly use it in the final result because the PDF
        // rendering system automatically applies this last before
        // drawing the form XObject.
        fmatrix = QPDFMatrix(fdict.getKey("/Matrix").getArrayAsMatrix());
        wmatrix.concat(fmatrix);
    }

    // The current wmatrix handles transformation from the form
    // xobject and, if requested, the destination page. Next, we have
    // to adjust this for scale and position.

    // Step 1: figure out what scale factor we need to make the form
    // XObject's bounding box fit within the destination rectangle.

    // Transform bounding box
    QPDFObjectHandle::Rectangle bbox = bbox_obj.getArrayAsRectangle();
    QPDFObjectHandle::Rectangle T = wmatrix.transformRectangle(bbox);

    // Calculate a scale factor, if needed. Shrink or expand if needed
    // and allowed.
    if ((T.urx == T.llx) || (T.ury == T.lly))
    {
        // avoid division by zero
        return "";
    }
    double rect_w = rect.urx - rect.llx;
    double rect_h = rect.ury - rect.lly;
    double t_w = T.urx - T.llx;
    double t_h = T.ury - T.lly;
    double xscale = rect_w / t_w;
    double yscale = rect_h / t_h;
    double scale = (xscale < yscale ? xscale : yscale);
    if (scale > 1.0)
    {
        if (! allow_expand)
        {
            scale = 1.0;
        }
    }
    else if (scale < 1.0)
    {
        if (! allow_shrink)
        {
            scale = 1.0;
        }
    }

    // Step 2: figure out what translation is required to get the
    // rectangle to the right spot: centered within the destination.
    wmatrix = QPDFMatrix();
    wmatrix.scale(scale, scale);
    wmatrix.concat(tmatrix);
    wmatrix.concat(fmatrix);

    T = wmatrix.transformRectangle(bbox);
    double t_cx = (T.llx + T.urx) / 2.0;
    double t_cy = (T.lly + T.ury) / 2.0;
    double r_cx = (rect.llx + rect.urx) / 2.0;
    double r_cy = (rect.lly + rect.ury) / 2.0;
    double tx = r_cx - t_cx;
    double ty = r_cy - t_cy;

    // Now we can calculate the final matrix. The final matrix does
    // not include fmatrix because that is applied automatically by
    // the PDF interpreter.
    QPDFMatrix cm;
    cm.translate(tx, ty);
    cm.scale(scale, scale);
    cm.concat(tmatrix);
    return (
        "q\n" +
        cm.unparse() + " cm\n" +
        name + " Do\n" +
        "Q\n");
}

void
QPDFPageObjectHelper::flattenRotation()
{
    QPDF* qpdf = this->oh.getOwningQPDF();
    if (! qpdf)
    {
        throw std::runtime_error(
            "QPDFPageObjectHelper::flattenRotation"
            " called with a direct object");
    }

    auto rotate_oh = this->oh.getKey("/Rotate");
    int rotate = 0;
    if (rotate_oh.isInteger())
    {
        rotate = rotate_oh.getIntValueAsInt();
    }
    if (! ((rotate == 90) || (rotate == 180) || (rotate == 270)))
    {
        return;
    }
    auto mediabox = this->oh.getKey("/MediaBox");
    if (! mediabox.isRectangle())
    {
        return;
    }
    auto media_rect = mediabox.getArrayAsRectangle();

    std::vector<std::string> boxes = {
        "/MediaBox", "/CropBox", "/BleedBox", "/TrimBox", "/ArtBox",
    };
    for (auto const& boxkey: boxes)
    {
        auto box = this->oh.getKey(boxkey);
        if (! box.isRectangle())
        {
            continue;
        }
        auto rect = box.getArrayAsRectangle();
        decltype(rect) new_rect;

        // How far are the edges of our rectangle from the edges
        // of the media box?
        auto left_x   = rect.llx - media_rect.llx;
        auto right_x  = media_rect.urx - rect.urx;
        auto bottom_y = rect.lly - media_rect.lly;
        auto top_y    = media_rect.ury - rect.ury;

        // Rotating the page 180 degrees does not change
        // /MediaBox. Rotating 90 or 270 degrees reverses llx and
        // lly and also reverse urx and ury. For all the other
        // boxes, we want the corners to be the correct distance
        // away from the corners of the mediabox.
        switch (rotate)
        {
          case 90:
            new_rect.llx = media_rect.lly + bottom_y;
            new_rect.urx = media_rect.ury - top_y;
            new_rect.lly = media_rect.llx + right_x;
            new_rect.ury = media_rect.urx - left_x;
            break;

          case 180:
            new_rect.llx = media_rect.llx + right_x;
            new_rect.urx = media_rect.urx - left_x;
            new_rect.lly = media_rect.lly + top_y;
            new_rect.ury = media_rect.ury - bottom_y;
            break;

          case 270:
            new_rect.llx = media_rect.lly + top_y;
            new_rect.urx = media_rect.ury - bottom_y;
            new_rect.lly = media_rect.llx + left_x;
            new_rect.ury = media_rect.urx - right_x;
            break;

          default:
            // ignore
            break;
        }

        this->oh.replaceKey(
            boxkey, QPDFObjectHandle::newFromRectangle(new_rect));
    }

    // When we rotate the page, pivot about the point 0, 0 and then
    // translate so the page is visible with the origin point being
    // the same offset from the lower left corner of the media box.
    // These calculations have been verified empirically with various
    // PDF readers.
    QPDFObjectHandle::Matrix cm;
    cm.e = 0.0;
    cm.f = 0.0;
    switch (rotate)
    {
      case 90:
        cm.b = -1;
        cm.c = 1;
        cm.f = media_rect.urx + media_rect.llx;
        break;

      case 180:
        cm.a = -1;
        cm.d = -1;
        cm.e = media_rect.urx + media_rect.llx;
        cm.f = media_rect.ury + media_rect.lly;
        break;

      case 270:
        cm.b = 1;
        cm.c = -1;
        cm.e = media_rect.ury + media_rect.lly;
        break;

      default:
        break;
    }
    std::string cm_str =
        std::string("q\n") +
        QUtil::double_to_string(cm.a, 2) + " " +
        QUtil::double_to_string(cm.b, 2) + " " +
        QUtil::double_to_string(cm.c, 2) + " " +
        QUtil::double_to_string(cm.d, 2) + " " +
        QUtil::double_to_string(cm.e, 2) + " " +
        QUtil::double_to_string(cm.f, 2) + " cm\n";
    this->oh.addPageContents(
        QPDFObjectHandle::newStream(qpdf, cm_str), true);
    this->oh.addPageContents(
        QPDFObjectHandle::newStream(qpdf, "\nQ\n"), false);
    this->oh.removeKey("/Rotate");
}
