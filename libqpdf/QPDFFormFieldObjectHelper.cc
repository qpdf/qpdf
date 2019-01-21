#include <qpdf/QPDFFormFieldObjectHelper.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <stdlib.h>

QPDFFormFieldObjectHelper::Members::~Members()
{
}

QPDFFormFieldObjectHelper::Members::Members()
{
}

QPDFFormFieldObjectHelper::QPDFFormFieldObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh),
    m(new Members())
{
}

QPDFFormFieldObjectHelper::QPDFFormFieldObjectHelper() :
    QPDFObjectHelper(QPDFObjectHandle::newNull()),
    m(new Members())
{
}

bool
QPDFFormFieldObjectHelper::isNull()
{
    return this->oh.isNull();
}

QPDFFormFieldObjectHelper
QPDFFormFieldObjectHelper::getParent()
{
    return this->oh.getKey("/Parent"); // may be null
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getInheritableFieldValue(std::string const& name)
{
    QPDFObjectHandle node = this->oh;
    QPDFObjectHandle result(node.getKey(name));
    std::set<QPDFObjGen> seen;
    while (result.isNull() && node.hasKey("/Parent"))
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
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper non-trivial inheritance");
        }
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getInheritableFieldValueAsString(
    std::string const& name)
{
    QPDFObjectHandle fv = getInheritableFieldValue(name);
    std::string result;
    if (fv.isString())
    {
        result = fv.getUTF8Value();
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getInheritableFieldValueAsName(
    std::string const& name)
{
    QPDFObjectHandle fv = getInheritableFieldValue(name);
    std::string result;
    if (fv.isName())
    {
        result = fv.getName();
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getFieldType()
{
    return getInheritableFieldValueAsName("/FT");
}

std::string
QPDFFormFieldObjectHelper::getFullyQualifiedName()
{
    std::string result;
    QPDFObjectHandle node = this->oh;
    std::set<QPDFObjGen> seen;
    while ((! node.isNull()) && (seen.count(node.getObjGen()) == 0))
    {
        if (node.getKey("/T").isString())
        {
            if (! result.empty())
            {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper non-trivial qualified name");
                result = "." + result;
            }
            result = node.getKey("/T").getUTF8Value() + result;
        }
        seen.insert(node.getObjGen());
        node = node.getKey("/Parent");
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getPartialName()
{
    std::string result;
    if (this->oh.getKey("/T").isString())
    {
        result = this->oh.getKey("/T").getUTF8Value();
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getAlternativeName()
{
    if (this->oh.getKey("/TU").isString())
    {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper TU present");
        return this->oh.getKey("/TU").getUTF8Value();
    }
    QTC::TC("qpdf", "QPDFFormFieldObjectHelper TU absent");
    return getFullyQualifiedName();
}

std::string
QPDFFormFieldObjectHelper::getMappingName()
{
    if (this->oh.getKey("/TM").isString())
    {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper TM present");
        return this->oh.getKey("/TM").getUTF8Value();
    }
    QTC::TC("qpdf", "QPDFFormFieldObjectHelper TM absent");
    return getAlternativeName();
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getValue()
{
    return getInheritableFieldValue("/V");
}

std::string
QPDFFormFieldObjectHelper::getValueAsString()
{
    return getInheritableFieldValueAsString("/V");
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getDefaultValue()
{
    return getInheritableFieldValue("/DV");
}

std::string
QPDFFormFieldObjectHelper::getDefaultValueAsString()
{
    return getInheritableFieldValueAsString("/DV");
}

std::string
QPDFFormFieldObjectHelper::getDefaultAppearance()
{
    return getInheritableFieldValueAsString("/DA");
}

int
QPDFFormFieldObjectHelper::getQuadding()
{
    int result = 0;
    QPDFObjectHandle fv = getInheritableFieldValue("/Q");
    if (fv.isInteger())
    {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper Q present");
        result = static_cast<int>(fv.getIntValue());
    }
    return result;
}

int
QPDFFormFieldObjectHelper::getFlags()
{
    QPDFObjectHandle f = getInheritableFieldValue("/Ff");
    return f.isInteger() ? f.getIntValue() : 0;
}

bool
QPDFFormFieldObjectHelper::isText()
{
    return (getFieldType() == "/Tx");
}

bool
QPDFFormFieldObjectHelper::isCheckbox()
{
    return ((getFieldType() == "/Btn") &&
            ((getFlags() & (ff_btn_radio | ff_btn_pushbutton)) == 0));
}

bool
QPDFFormFieldObjectHelper::isRadioButton()
{
    return ((getFieldType() == "/Btn") &&
            ((getFlags() & ff_btn_radio) == ff_btn_radio));
}

bool
QPDFFormFieldObjectHelper::isPushbutton()
{
    return ((getFieldType() == "/Btn") &&
            ((getFlags() & ff_btn_pushbutton) == ff_btn_pushbutton));
}

bool
QPDFFormFieldObjectHelper::isChoice()
{
    return (getFieldType() == "/Ch");
}

std::vector<std::string>
QPDFFormFieldObjectHelper::getChoices()
{
    std::vector<std::string> result;
    if (! isChoice())
    {
        return result;
    }
    QPDFObjectHandle opt = getInheritableFieldValue("/Opt");
    if (opt.isArray())
    {
        size_t n = opt.getArrayNItems();
        for (size_t i = 0; i < n; ++i)
        {
            QPDFObjectHandle item = opt.getArrayItem(i);
            if (item.isString())
            {
                result.push_back(item.getUTF8Value());
            }
        }
    }
    return result;
}

void
QPDFFormFieldObjectHelper::setFieldAttribute(
    std::string const& key, QPDFObjectHandle value)
{
    this->oh.replaceKey(key, value);
}

void
QPDFFormFieldObjectHelper::setFieldAttribute(
    std::string const& key, std::string const& utf8_value)
{
    this->oh.replaceKey(key, QPDFObjectHandle::newUnicodeString(utf8_value));
}

void
QPDFFormFieldObjectHelper::setV(
    QPDFObjectHandle value, bool need_appearances)
{
    if (getFieldType() == "/Btn")
    {
        if (isCheckbox())
        {
            bool okay = false;
            if (value.isName())
            {
                std::string name = value.getName();
                if ((name == "/Yes") || (name == "/Off"))
                {
                    okay = true;
                    setCheckBoxValue((name == "/Yes"));
                }
            }
            if (! okay)
            {
                this->oh.warnIfPossible(
                    "ignoring attempt to set a checkbox field to a"
                    " value of other than /Yes or /Off");
            }
        }
        else if (isRadioButton())
        {
            if (value.isName())
            {
                setRadioButtonValue(value);
            }
            else
            {
                this->oh.warnIfPossible(
                    "ignoring attempt to set a radio button field to"
                    " an object that is not a name");
            }
        }
        else if (isPushbutton())
        {
            this->oh.warnIfPossible(
                "ignoring attempt set the value of a pushbutton field");
        }
        return;
    }
    if (value.isString())
    {
        setFieldAttribute(
            "/V", QPDFObjectHandle::newUnicodeString(value.getUTF8Value()));
    }
    else
    {
        setFieldAttribute("/V", value);
    }
    if (need_appearances)
    {
        QPDF* qpdf = this->oh.getOwningQPDF();
        if (! qpdf)
        {
            throw std::logic_error(
                "QPDFFormFieldObjectHelper::setV called with"
                " need_appearances = true on an object that is"
                " not associated with an owning QPDF");
        }
        QPDFAcroFormDocumentHelper(*qpdf).setNeedAppearances(true);
    }
}

void
QPDFFormFieldObjectHelper::setV(
    std::string const& utf8_value, bool need_appearances)
{
    setV(QPDFObjectHandle::newUnicodeString(utf8_value),
         need_appearances);
}

void
QPDFFormFieldObjectHelper::setRadioButtonValue(QPDFObjectHandle name)
{
    // Set the value of a radio button field. This has the following
    // specific behavior:
    // * If this is a radio button field that has a parent that is
    //   also a radio button field and has no explicit /V, call itself
    //   on the parent
    // * If this is a radio button field with children, set /V to the
    //   given value. Then, for each child, if the child has the
    //   specified value as one of its keys in the /N subdictionary of
    //   its /AP (i.e. its normal appearance stream dictionary), set
    //   /AS to name; otherwise, if /Off is a member, set /AS to /Off.
    // Note that we never turn on /NeedAppearances when setting a
    // radio button field.
    QPDFObjectHandle parent = this->oh.getKey("/Parent");
    if (parent.isDictionary() && parent.getKey("/Parent").isNull())
    {
        QPDFFormFieldObjectHelper ph(parent);
        if (ph.isRadioButton())
        {
            // This is most likely one of the individual buttons. Try
            // calling on the parent.
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper set parent radio button");
            ph.setRadioButtonValue(name);
            return;
        }
    }

    QPDFObjectHandle kids = this->oh.getKey("/Kids");
    if (! (isRadioButton() && parent.isNull() && kids.isArray()))
    {
        this->oh.warnIfPossible("don't know how to set the value"
                                " of this field as a radio button");
        return;
    }
    setFieldAttribute("/V", name);
    int nkids = kids.getArrayNItems();
    for (int i = 0; i < nkids; ++i)
    {
        QPDFObjectHandle kid = kids.getArrayItem(i);
        QPDFObjectHandle AP = kid.getKey("/AP");
        QPDFObjectHandle annot;
        if (AP.isNull())
        {
            // The widget may be below. If there is more than one,
            // just find the first one.
            QPDFObjectHandle grandkids = kid.getKey("/Kids");
            if (grandkids.isArray())
            {
                int ngrandkids = grandkids.getArrayNItems();
                for (int j = 0; j < ngrandkids; ++j)
                {
                    QPDFObjectHandle grandkid = grandkids.getArrayItem(j);
                    AP = grandkid.getKey("/AP");
                    if (! AP.isNull())
                    {
                        QTC::TC("qpdf", "QPDFFormFieldObjectHelper radio button grandkid widget");
                        annot = grandkid;
                        break;
                    }
                }
            }
        }
        else
        {
            annot = kid;
        }
        if (! annot.isInitialized())
        {
            QTC::TC("qpdf", "QPDFObjectHandle broken radio button");
            this->oh.warnIfPossible(
                "unable to set the value of this radio button");
            continue;
        }
        if (AP.isDictionary() &&
            AP.getKey("/N").isDictionary() &&
            AP.getKey("/N").hasKey(name.getName()))
        {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper turn on radio button");
            annot.replaceKey("/AS", name);
        }
        else
        {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper turn off radio button");
            annot.replaceKey("/AS", QPDFObjectHandle::newName("/Off"));
        }
    }
}

void
QPDFFormFieldObjectHelper::setCheckBoxValue(bool value)
{
    // Set /AS to /Yes or /Off in addition to setting /V.
    QPDFObjectHandle name =
        QPDFObjectHandle::newName(value ? "/Yes" : "/Off");
    setFieldAttribute("/V", name);
    QPDFObjectHandle AP = this->oh.getKey("/AP");
    QPDFObjectHandle annot;
    if (AP.isNull())
    {
        // The widget may be below. If there is more than one, just
        // find the first one.
        QPDFObjectHandle kids = this->oh.getKey("/Kids");
        if (kids.isArray())
        {
            int nkids = kids.getArrayNItems();
            for (int i = 0; i < nkids; ++i)
            {
                QPDFObjectHandle kid = kids.getArrayItem(i);
                AP = kid.getKey("/AP");
                if (! AP.isNull())
                {
                    QTC::TC("qpdf", "QPDFFormFieldObjectHelper checkbox kid widget");
                    annot = kid;
                    break;
                }
            }
        }
    }
    else
    {
        annot = this->oh;
    }
    if (! annot.isInitialized())
    {
        QTC::TC("qpdf", "QPDFObjectHandle broken checkbox");
        this->oh.warnIfPossible(
            "unable to set the value of this checkbox");
        return;
    }
    QTC::TC("qpdf", "QPDFFormFieldObjectHelper set checkbox AS");
    annot.replaceKey("/AS", name);
}

void
QPDFFormFieldObjectHelper::generateAppearance(QPDFAnnotationObjectHelper& aoh)
{
    std::string ft = getFieldType();
    // Ignore field types we don't know how to generate appearances
    // for. Button fields don't really need them -- see code in
    // QPDFAcroFormDocumentHelper::generateAppearancesIfNeeded.
    if ((ft == "/Tx") || (ft == "/Ch"))
    {
        generateTextAppearance(aoh);
    }
}

class ValueSetter: public QPDFObjectHandle::TokenFilter
{
  public:
    ValueSetter(std::string const& DA, std::string const& V,
                std::vector<std::string> const& opt, double tf,
                QPDFObjectHandle::Rectangle const& bbox);
    virtual ~ValueSetter()
    {
    }
    virtual void handleToken(QPDFTokenizer::Token const&);
    virtual void handleEOF();
    void writeAppearance();

  private:
    std::string DA;
    std::string V;
    std::vector<std::string> opt;
    double tf;
    QPDFObjectHandle::Rectangle bbox;
    enum { st_top, st_bmc, st_emc, st_end } state;
    bool replaced;
};

ValueSetter::ValueSetter(std::string const& DA, std::string const& V,
                         std::vector<std::string> const& opt, double tf,
                         QPDFObjectHandle::Rectangle const& bbox) :
    DA(DA),
    V(V),
    opt(opt),
    tf(tf),
    bbox(bbox),
    state(st_top),
    replaced(false)
{
}

void
ValueSetter::handleToken(QPDFTokenizer::Token const& token)
{
    QPDFTokenizer::token_type_e ttype = token.getType();
    std::string value = token.getValue();
    bool do_replace = false;
    switch (state)
    {
        case st_top:
          writeToken(token);
          if ((ttype == QPDFTokenizer::tt_word) && (value == "BMC"))
          {
              state = st_bmc;
          }
          break;

      case st_bmc:
        if ((ttype == QPDFTokenizer::tt_space) ||
            (ttype == QPDFTokenizer::tt_comment))
        {
            writeToken(token);
        }
        else
        {
            state = st_emc;
        }
        // fall through to emc

      case st_emc:
        if ((ttype == QPDFTokenizer::tt_word) && (value == "EMC"))
        {
            do_replace = true;
            state = st_end;
        }
        break;

      case st_end:
        writeToken(token);
        break;
    }
    if (do_replace)
    {
        writeAppearance();
    }
}

void
ValueSetter::handleEOF()
{
    if (! this->replaced)
    {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper replaced BMC at EOF");
        write("/Tx BMC\n");
        writeAppearance();
    }
}

void
ValueSetter::writeAppearance()
{
    this->replaced = true;

    // This code does not take quadding into consideration because
    // doing so requires font metric information, which we don't
    // have in many cases.

    double tfh = 1.2 * tf;
    int dx = 1;

    // Write one or more lines, centered vertically, possibly with
    // one row highlighted.

    size_t max_rows = static_cast<size_t>((bbox.ury - bbox.lly) / tfh);
    bool highlight = false;
    size_t highlight_idx = 0;

    std::vector<std::string> lines;
    if (opt.empty() || (max_rows < 2))
    {
        lines.push_back(V);
    }
    else
    {
        // Figure out what rows to write
        size_t nopt = opt.size();
        size_t found_idx = 0;
        bool found = false;
        for (found_idx = 0; found_idx < nopt; ++found_idx)
        {
            if (opt.at(found_idx) == V)
            {
                found = true;
                break;
            }
        }
        if (found)
        {
            // Try to make the found item the second one, but
            // adjust for under/overflow.
            int wanted_first = found_idx - 1;
            int wanted_last = found_idx + max_rows - 2;
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper list found");
            while (wanted_first < 0)
            {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper list first too low");
                ++wanted_first;
                ++wanted_last;
            }
            while (wanted_last >= static_cast<int>(nopt))
            {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper list last too high");
                if (wanted_first > 0)
                {
                    --wanted_first;
                }
                --wanted_last;
            }
            highlight = true;
            highlight_idx = found_idx - wanted_first;
            for (int i = wanted_first; i <= wanted_last; ++i)
            {
                lines.push_back(opt.at(i));
            }
        }
        else
        {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper list not found");
            // include our value and the first n-1 rows
            highlight_idx = 0;
            highlight = true;
            lines.push_back(V);
            for (size_t i = 0; ((i < nopt) && (i < (max_rows - 1))); ++i)
            {
                lines.push_back(opt.at(i));
            }
        }
    }

    // Write the lines centered vertically, highlighting if needed
    size_t nlines = lines.size();
    double dy = bbox.ury - ((bbox.ury - bbox.lly - (nlines * tfh)) / 2.0);
    write(DA + "\nq\n");
    if (highlight)
    {
        write("q\n0.85 0.85 0.85 rg\n" +
              QUtil::int_to_string(bbox.llx) + " " +
              QUtil::double_to_string(bbox.lly + dy -
                                      (tfh * (highlight_idx + 1))) + " " +
              QUtil::int_to_string(bbox.urx - bbox.llx) + " " +
              QUtil::double_to_string(tfh) +
              " re f\nQ\n");
    }
    dy += 0.2 * tf;
    for (size_t i = 0; i < nlines; ++i)
    {
        dy -= tfh;
        write("BT\n" +
              QUtil::int_to_string(bbox.llx + dx) + " " +
              QUtil::double_to_string(bbox.lly + dy) + " Td\n" +
              QPDFObjectHandle::newString(lines.at(i)).unparse() +
              " Tj\nET\n");
    }
    write("Q\nEMC");
}

class TfFinder: public QPDFObjectHandle::TokenFilter
{
  public:
    TfFinder();
    virtual ~TfFinder()
    {
    }
    virtual void handleToken(QPDFTokenizer::Token const&);
    double getTf();
    std::string getFontName();

  private:
    double tf;
    std::string font_name;
    double last_num;
    std::string last_name;
};

TfFinder::TfFinder() :
    tf(11.0),
    last_num(0.0)
{
}

void
TfFinder::handleToken(QPDFTokenizer::Token const& token)
{
    QPDFTokenizer::token_type_e ttype = token.getType();
    std::string value = token.getValue();
    switch (ttype)
    {
      case QPDFTokenizer::tt_integer:
      case QPDFTokenizer::tt_real:
        last_num = strtod(value.c_str(), 0);
        break;

      case QPDFTokenizer::tt_name:
        last_name = value;
        break;

      case QPDFTokenizer::tt_word:
        if ((value == "Tf") &&
            (last_num > 1.0) &&
            (last_num < 1000.0))
        {
            // These ranges are arbitrary but keep us from doing
            // insane things or suffering from over/underflow
            tf = last_num;
        }
        font_name = last_name;
        break;

      default:
        break;
    }
}

double
TfFinder::getTf()
{
    return this->tf;
}

std::string
TfFinder::getFontName()
{
    return this->font_name;
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getFontFromResource(
    QPDFObjectHandle resources, std::string const& name)
{
    QPDFObjectHandle result;
    if (resources.isDictionary() &&
        resources.getKey("/Font").isDictionary() &&
        resources.getKey("/Font").hasKey(name))
    {
        result = resources.getKey("/Font").getKey(name);
    }
    return result;
}

void
QPDFFormFieldObjectHelper::generateTextAppearance(
    QPDFAnnotationObjectHelper& aoh)
{
    QPDFObjectHandle AS = aoh.getAppearanceStream("/N");
    if (AS.isNull())
    {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper create AS from scratch");
        QPDFObjectHandle::Rectangle rect = aoh.getRect();
        QPDFObjectHandle::Rectangle bbox(
            0, 0, rect.urx - rect.llx, rect.ury - rect.lly);
        QPDFObjectHandle dict = QPDFObjectHandle::parse(
            "<< /Resources << /ProcSet [ /PDF /Text ] >>"
            " /Type /XObject /Subtype /Form >>");
        dict.replaceKey("/BBox", QPDFObjectHandle::newFromRectangle(bbox));
        AS = QPDFObjectHandle::newStream(
            this->oh.getOwningQPDF(), "/Tx BMC\nEMC\n");
        AS.replaceDict(dict);
        QPDFObjectHandle AP = aoh.getAppearanceDictionary();
        if (AP.isNull())
        {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper create AP from scratch");
            aoh.getObjectHandle().replaceKey(
                "/AP", QPDFObjectHandle::newDictionary());
            AP = aoh.getAppearanceDictionary();
        }
        AP.replaceKey("/N", AS);
    }
    if (! AS.isStream())
    {
        aoh.getObjectHandle().warnIfPossible(
            "unable to get normal appearance stream for update");
        return;
    }
    QPDFObjectHandle bbox_obj = AS.getDict().getKey("/BBox");
    if (! bbox_obj.isRectangle())
    {
        aoh.getObjectHandle().warnIfPossible(
            "unable to get appearance stream bounding box");
        return;
    }
    QPDFObjectHandle::Rectangle bbox = bbox_obj.getArrayAsRectangle();
    std::string DA = getDefaultAppearance();
    std::string V = getValueAsString();
    std::vector<std::string> opt;
    if (isChoice() && ((getFlags() & ff_ch_combo) == 0))
    {
        opt = getChoices();
    }

    TfFinder tff;
    Pl_QPDFTokenizer tok("tf", &tff);
    tok.write(QUtil::unsigned_char_pointer(DA.c_str()), DA.length());
    tok.finish();
    double tf = tff.getTf();

    std::string (*encoder)(std::string const&, char) = &QUtil::utf8_to_ascii;
    std::string font_name = tff.getFontName();
    if (! font_name.empty())
    {
        // See if the font is encoded with something we know about.
        QPDFObjectHandle resources = AS.getDict().getKey("/Resources");
        QPDFObjectHandle font = getFontFromResource(resources, font_name);
        if (! font.isInitialized())
        {
            QPDFObjectHandle dr = getInheritableFieldValue("/DR");
            font = getFontFromResource(dr, font_name);
        }
        if (font.isDictionary() &&
            font.getKey("/Encoding").isName())
        {
            std::string encoding = font.getKey("/Encoding").getName();
            if (encoding == "/WinAnsiEncoding")
            {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper WinAnsi");
                encoder = &QUtil::utf8_to_win_ansi;
            }
            else if (encoding == "/MacRomanEncoding")
            {
                encoder = &QUtil::utf8_to_mac_roman;
            }
        }
    }

    V = (*encoder)(V, '?');
    for (size_t i = 0; i < opt.size(); ++i)
    {
        opt.at(i) = (*encoder)(opt.at(i), '?');
    }

    AS.addTokenFilter(new ValueSetter(DA, V, opt, tf, bbox));
}
