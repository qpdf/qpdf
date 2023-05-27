#include <qpdf/QPDFFormFieldObjectHelper.hh>

#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <cstdlib>

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

QPDFFormFieldObjectHelper
QPDFFormFieldObjectHelper::getTopLevelField(bool* is_different)
{
    auto top_field = this->oh;
    QPDFObjGen::set seen;
    while (seen.add(top_field) && !top_field.getKeyIfDict("/Parent").isNull()) {
        top_field = top_field.getKey("/Parent");
        if (is_different) {
            *is_different = true;
        }
    }
    return {top_field};
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getFieldFromAcroForm(std::string const& name)
{
    QPDFObjectHandle result = QPDFObjectHandle::newNull();
    // Fields are supposed to be indirect, so this should work.
    QPDF* q = this->oh.getOwningQPDF();
    if (!q) {
        return result;
    }
    auto acroform = q->getRoot().getKey("/AcroForm");
    if (!acroform.isDictionary()) {
        return result;
    }
    return acroform.getKey(name);
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getInheritableFieldValue(std::string const& name)
{
    QPDFObjectHandle node = this->oh;
    if (!node.isDictionary()) {
        return QPDFObjectHandle::newNull();
    }
    QPDFObjectHandle result(node.getKey(name));
    if (result.isNull()) {
        QPDFObjGen::set seen;
        while (seen.add(node) && node.hasKey("/Parent")) {
            node = node.getKey("/Parent");
            result = node.getKey(name);
            if (!result.isNull()) {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper non-trivial inheritance");
                return result;
            }
        }
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getInheritableFieldValueAsString(std::string const& name)
{
    QPDFObjectHandle fv = getInheritableFieldValue(name);
    std::string result;
    if (fv.isString()) {
        result = fv.getUTF8Value();
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getInheritableFieldValueAsName(std::string const& name)
{
    QPDFObjectHandle fv = getInheritableFieldValue(name);
    std::string result;
    if (fv.isName()) {
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
    QPDFObjGen::set seen;
    while (!node.isNull() && seen.add(node)) {
        if (node.getKey("/T").isString()) {
            if (!result.empty()) {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper non-trivial qualified name");
                result = "." + result;
            }
            result = node.getKey("/T").getUTF8Value() + result;
        }
        node = node.getKey("/Parent");
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getPartialName()
{
    std::string result;
    if (this->oh.getKey("/T").isString()) {
        result = this->oh.getKey("/T").getUTF8Value();
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getAlternativeName()
{
    if (this->oh.getKey("/TU").isString()) {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper TU present");
        return this->oh.getKey("/TU").getUTF8Value();
    }
    QTC::TC("qpdf", "QPDFFormFieldObjectHelper TU absent");
    return getFullyQualifiedName();
}

std::string
QPDFFormFieldObjectHelper::getMappingName()
{
    if (this->oh.getKey("/TM").isString()) {
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

QPDFObjectHandle
QPDFFormFieldObjectHelper::getDefaultResources()
{
    return getFieldFromAcroForm("/DR");
}

std::string
QPDFFormFieldObjectHelper::getDefaultAppearance()
{
    auto value = getInheritableFieldValue("/DA");
    bool looked_in_acroform = false;
    if (!value.isString()) {
        value = getFieldFromAcroForm("/DA");
        looked_in_acroform = true;
    }
    std::string result;
    if (value.isString()) {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper DA present", looked_in_acroform ? 0 : 1);
        result = value.getUTF8Value();
    }
    return result;
}

int
QPDFFormFieldObjectHelper::getQuadding()
{
    QPDFObjectHandle fv = getInheritableFieldValue("/Q");
    bool looked_in_acroform = false;
    if (!fv.isInteger()) {
        fv = getFieldFromAcroForm("/Q");
        looked_in_acroform = true;
    }
    int result = 0;
    if (fv.isInteger()) {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper Q present", looked_in_acroform ? 0 : 1);
        result = QIntC::to_int(fv.getIntValue());
    }
    return result;
}

int
QPDFFormFieldObjectHelper::getFlags()
{
    QPDFObjectHandle f = getInheritableFieldValue("/Ff");
    return f.isInteger() ? f.getIntValueAsInt() : 0;
}

bool
QPDFFormFieldObjectHelper::isText()
{
    return (getFieldType() == "/Tx");
}

bool
QPDFFormFieldObjectHelper::isCheckbox()
{
    return ((getFieldType() == "/Btn") && ((getFlags() & (ff_btn_radio | ff_btn_pushbutton)) == 0));
}

bool
QPDFFormFieldObjectHelper::isRadioButton()
{
    return ((getFieldType() == "/Btn") && ((getFlags() & ff_btn_radio) == ff_btn_radio));
}

bool
QPDFFormFieldObjectHelper::isPushbutton()
{
    return ((getFieldType() == "/Btn") && ((getFlags() & ff_btn_pushbutton) == ff_btn_pushbutton));
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
    if (!isChoice()) {
        return result;
    }
    QPDFObjectHandle opt = getInheritableFieldValue("/Opt");
    if (opt.isArray()) {
        int n = opt.getArrayNItems();
        for (int i = 0; i < n; ++i) {
            QPDFObjectHandle item = opt.getArrayItem(i);
            if (item.isString()) {
                result.push_back(item.getUTF8Value());
            }
        }
    }
    return result;
}

void
QPDFFormFieldObjectHelper::setFieldAttribute(std::string const& key, QPDFObjectHandle value)
{
    this->oh.replaceKey(key, value);
}

void
QPDFFormFieldObjectHelper::setFieldAttribute(std::string const& key, std::string const& utf8_value)
{
    this->oh.replaceKey(key, QPDFObjectHandle::newUnicodeString(utf8_value));
}

void
QPDFFormFieldObjectHelper::setV(QPDFObjectHandle value, bool need_appearances)
{
    if (getFieldType() == "/Btn") {
        if (isCheckbox()) {
            bool okay = false;
            if (value.isName()) {
                std::string name = value.getName();
                if ((name == "/Yes") || (name == "/Off")) {
                    okay = true;
                    setCheckBoxValue((name == "/Yes"));
                }
            }
            if (!okay) {
                this->oh.warnIfPossible("ignoring attempt to set a checkbox field to a value of "
                                        "other than /Yes or /Off");
            }
        } else if (isRadioButton()) {
            if (value.isName()) {
                setRadioButtonValue(value);
            } else {
                this->oh.warnIfPossible(
                    "ignoring attempt to set a radio button field to an object that is not a name");
            }
        } else if (isPushbutton()) {
            this->oh.warnIfPossible("ignoring attempt set the value of a pushbutton field");
        }
        return;
    }
    if (value.isString()) {
        setFieldAttribute("/V", QPDFObjectHandle::newUnicodeString(value.getUTF8Value()));
    } else {
        setFieldAttribute("/V", value);
    }
    if (need_appearances) {
        QPDF& qpdf =
            this->oh.getQPDF("QPDFFormFieldObjectHelper::setV called with need_appearances = "
                             "true on an object that is not associated with an owning QPDF");
        QPDFAcroFormDocumentHelper(qpdf).setNeedAppearances(true);
    }
}

void
QPDFFormFieldObjectHelper::setV(std::string const& utf8_value, bool need_appearances)
{
    setV(QPDFObjectHandle::newUnicodeString(utf8_value), need_appearances);
}

void
QPDFFormFieldObjectHelper::setRadioButtonValue(QPDFObjectHandle name)
{
    // Set the value of a radio button field. This has the following specific behavior:
    // * If this is a radio button field that has a parent that is also a radio button field and has
    //   no explicit /V, call itself on the parent
    // * If this is a radio button field with children, set /V to the given value. Then, for each
    //   child, if the child has the specified value as one of its keys in the /N subdictionary of
    //   its /AP (i.e. its normal appearance stream dictionary), set /AS to name; otherwise, if /Off
    //   is a member, set /AS to /Off.
    // Note that we never turn on /NeedAppearances when setting a radio button field.
    QPDFObjectHandle parent = this->oh.getKey("/Parent");
    if (parent.isDictionary() && parent.getKey("/Parent").isNull()) {
        QPDFFormFieldObjectHelper ph(parent);
        if (ph.isRadioButton()) {
            // This is most likely one of the individual buttons. Try calling on the parent.
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper set parent radio button");
            ph.setRadioButtonValue(name);
            return;
        }
    }

    QPDFObjectHandle kids = this->oh.getKey("/Kids");
    if (!(isRadioButton() && parent.isNull() && kids.isArray())) {
        this->oh.warnIfPossible("don't know how to set the value"
                                " of this field as a radio button");
        return;
    }
    setFieldAttribute("/V", name);
    int nkids = kids.getArrayNItems();
    for (int i = 0; i < nkids; ++i) {
        QPDFObjectHandle kid = kids.getArrayItem(i);
        QPDFObjectHandle AP = kid.getKey("/AP");
        QPDFObjectHandle annot;
        if (AP.isNull()) {
            // The widget may be below. If there is more than one, just find the first one.
            QPDFObjectHandle grandkids = kid.getKey("/Kids");
            if (grandkids.isArray()) {
                int ngrandkids = grandkids.getArrayNItems();
                for (int j = 0; j < ngrandkids; ++j) {
                    QPDFObjectHandle grandkid = grandkids.getArrayItem(j);
                    AP = grandkid.getKey("/AP");
                    if (!AP.isNull()) {
                        QTC::TC("qpdf", "QPDFFormFieldObjectHelper radio button grandkid");
                        annot = grandkid;
                        break;
                    }
                }
            }
        } else {
            annot = kid;
        }
        if (!annot.isInitialized()) {
            QTC::TC("qpdf", "QPDFObjectHandle broken radio button");
            this->oh.warnIfPossible("unable to set the value of this radio button");
            continue;
        }
        if (AP.isDictionary() && AP.getKey("/N").isDictionary() &&
            AP.getKey("/N").hasKey(name.getName())) {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper turn on radio button");
            annot.replaceKey("/AS", name);
        } else {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper turn off radio button");
            annot.replaceKey("/AS", QPDFObjectHandle::newName("/Off"));
        }
    }
}

void
QPDFFormFieldObjectHelper::setCheckBoxValue(bool value)
{
    // Set /AS to /Yes or /Off in addition to setting /V.
    QPDFObjectHandle name = QPDFObjectHandle::newName(value ? "/Yes" : "/Off");
    setFieldAttribute("/V", name);
    QPDFObjectHandle AP = this->oh.getKey("/AP");
    QPDFObjectHandle annot;
    if (AP.isNull()) {
        // The widget may be below. If there is more than one, just
        // find the first one.
        QPDFObjectHandle kids = this->oh.getKey("/Kids");
        if (kids.isArray()) {
            int nkids = kids.getArrayNItems();
            for (int i = 0; i < nkids; ++i) {
                QPDFObjectHandle kid = kids.getArrayItem(i);
                AP = kid.getKey("/AP");
                if (!AP.isNull()) {
                    QTC::TC("qpdf", "QPDFFormFieldObjectHelper checkbox kid widget");
                    annot = kid;
                    break;
                }
            }
        }
    } else {
        annot = this->oh;
    }
    if (!annot.isInitialized()) {
        QTC::TC("qpdf", "QPDFObjectHandle broken checkbox");
        this->oh.warnIfPossible("unable to set the value of this checkbox");
        return;
    }
    QTC::TC("qpdf", "QPDFFormFieldObjectHelper set checkbox AS");
    annot.replaceKey("/AS", name);
}

void
QPDFFormFieldObjectHelper::generateAppearance(QPDFAnnotationObjectHelper& aoh)
{
    std::string ft = getFieldType();
    // Ignore field types we don't know how to generate appearances for. Button fields don't really
    // need them -- see code in QPDFAcroFormDocumentHelper::generateAppearancesIfNeeded.
    if ((ft == "/Tx") || (ft == "/Ch")) {
        generateTextAppearance(aoh);
    }
}

namespace
{
    class ValueSetter: public QPDFObjectHandle::TokenFilter
    {
      public:
        ValueSetter(
            std::string const& DA,
            std::string const& V,
            std::vector<std::string> const& opt,
            double tf,
            QPDFObjectHandle::Rectangle const& bbox);
        ~ValueSetter() override = default;
        void handleToken(QPDFTokenizer::Token const&) override;
        void handleEOF() override;
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
} // namespace

ValueSetter::ValueSetter(
    std::string const& DA,
    std::string const& V,
    std::vector<std::string> const& opt,
    double tf,
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
    switch (state) {
    case st_top:
        writeToken(token);
        if (token.isWord("BMC")) {
            state = st_bmc;
        }
        break;

    case st_bmc:
        if ((ttype == QPDFTokenizer::tt_space) || (ttype == QPDFTokenizer::tt_comment)) {
            writeToken(token);
        } else {
            state = st_emc;
        }
        // fall through to emc

    case st_emc:
        if (token.isWord("EMC")) {
            do_replace = true;
            state = st_end;
        }
        break;

    case st_end:
        writeToken(token);
        break;
    }
    if (do_replace) {
        writeAppearance();
    }
}

void
ValueSetter::handleEOF()
{
    if (!this->replaced) {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper replaced BMC at EOF");
        write("/Tx BMC\n");
        writeAppearance();
    }
}

void
ValueSetter::writeAppearance()
{
    this->replaced = true;

    // This code does not take quadding into consideration because doing so requires font metric
    // information, which we don't have in many cases.

    double tfh = 1.2 * tf;
    int dx = 1;

    // Write one or more lines, centered vertically, possibly with one row highlighted.

    auto max_rows = static_cast<size_t>((bbox.ury - bbox.lly) / tfh);
    bool highlight = false;
    size_t highlight_idx = 0;

    std::vector<std::string> lines;
    if (opt.empty() || (max_rows < 2)) {
        lines.push_back(V);
    } else {
        // Figure out what rows to write
        size_t nopt = opt.size();
        size_t found_idx = 0;
        bool found = false;
        for (found_idx = 0; found_idx < nopt; ++found_idx) {
            if (opt.at(found_idx) == V) {
                found = true;
                break;
            }
        }
        if (found) {
            // Try to make the found item the second one, but adjust for under/overflow.
            int wanted_first = QIntC::to_int(found_idx) - 1;
            int wanted_last = QIntC::to_int(found_idx + max_rows) - 2;
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper list found");
            while (wanted_first < 0) {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper list first too low");
                ++wanted_first;
                ++wanted_last;
            }
            while (wanted_last >= QIntC::to_int(nopt)) {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper list last too high");
                if (wanted_first > 0) {
                    --wanted_first;
                }
                --wanted_last;
            }
            highlight = true;
            highlight_idx = found_idx - QIntC::to_size(wanted_first);
            for (size_t i = QIntC::to_size(wanted_first); i <= QIntC::to_size(wanted_last); ++i) {
                lines.push_back(opt.at(i));
            }
        } else {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper list not found");
            // include our value and the first n-1 rows
            highlight_idx = 0;
            highlight = true;
            lines.push_back(V);
            for (size_t i = 0; ((i < nopt) && (i < (max_rows - 1))); ++i) {
                lines.push_back(opt.at(i));
            }
        }
    }

    // Write the lines centered vertically, highlighting if needed
    size_t nlines = lines.size();
    double dy = bbox.ury - ((bbox.ury - bbox.lly - (static_cast<double>(nlines) * tfh)) / 2.0);
    if (highlight) {
        write(
            "q\n0.85 0.85 0.85 rg\n" + QUtil::double_to_string(bbox.llx) + " " +
            QUtil::double_to_string(
                bbox.lly + dy - (tfh * (static_cast<double>(highlight_idx + 1)))) +
            " " + QUtil::double_to_string(bbox.urx - bbox.llx) + " " +
            QUtil::double_to_string(tfh) + " re f\nQ\n");
    }
    dy -= tf;
    write("q\nBT\n" + DA + "\n");
    for (size_t i = 0; i < nlines; ++i) {
        // We could adjust Tm to translate to the beginning the first line, set TL to tfh, and use
        // T* for each subsequent line, but doing this would require extracting any Tm from DA,
        // which doesn't seem really worth the effort.
        if (i == 0) {
            write(
                QUtil::double_to_string(bbox.llx + static_cast<double>(dx)) + " " +
                QUtil::double_to_string(bbox.lly + static_cast<double>(dy)) + " Td\n");
        } else {
            write("0 " + QUtil::double_to_string(-tfh) + " Td\n");
        }
        write(QPDFObjectHandle::newString(lines.at(i)).unparse() + " Tj\n");
    }
    write("ET\nQ\nEMC");
}

namespace
{
    class TfFinder: public QPDFObjectHandle::TokenFilter
    {
      public:
        TfFinder();
        ~TfFinder() override
        {
        }
        void handleToken(QPDFTokenizer::Token const&) override;
        double getTf();
        std::string getFontName();
        std::string getDA();

      private:
        double tf;
        int tf_idx;
        std::string font_name;
        double last_num;
        int last_num_idx;
        std::string last_name;
        std::vector<std::string> DA;
    };
} // namespace

TfFinder::TfFinder() :
    tf(11.0),
    tf_idx(-1),
    last_num(0.0),
    last_num_idx(-1)
{
}

void
TfFinder::handleToken(QPDFTokenizer::Token const& token)
{
    QPDFTokenizer::token_type_e ttype = token.getType();
    std::string value = token.getValue();
    DA.push_back(token.getRawValue());
    switch (ttype) {
    case QPDFTokenizer::tt_integer:
    case QPDFTokenizer::tt_real:
        last_num = strtod(value.c_str(), nullptr);
        last_num_idx = QIntC::to_int(DA.size() - 1);
        break;

    case QPDFTokenizer::tt_name:
        last_name = value;
        break;

    case QPDFTokenizer::tt_word:
        if (token.isWord("Tf")) {
            if ((last_num > 1.0) && (last_num < 1000.0)) {
                // These ranges are arbitrary but keep us from doing insane things or suffering from
                // over/underflow
                tf = last_num;
            }
            tf_idx = last_num_idx;
            font_name = last_name;
        }
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
TfFinder::getDA()
{
    std::string result;
    size_t n = this->DA.size();
    for (size_t i = 0; i < n; ++i) {
        std::string cur = this->DA.at(i);
        if (QIntC::to_int(i) == tf_idx) {
            double delta = strtod(cur.c_str(), nullptr) - this->tf;
            if ((delta > 0.001) || (delta < -0.001)) {
                // tf doesn't match the font size passed to Tf, so substitute.
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper fallback Tf");
                cur = QUtil::double_to_string(tf);
            }
        }
        result += cur;
    }
    return result;
}

std::string
TfFinder::getFontName()
{
    return this->font_name;
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getFontFromResource(QPDFObjectHandle resources, std::string const& name)
{
    QPDFObjectHandle result;
    if (resources.isDictionary() && resources.getKey("/Font").isDictionary() &&
        resources.getKey("/Font").hasKey(name)) {
        result = resources.getKey("/Font").getKey(name);
    }
    return result;
}

void
QPDFFormFieldObjectHelper::generateTextAppearance(QPDFAnnotationObjectHelper& aoh)
{
    QPDFObjectHandle AS = aoh.getAppearanceStream("/N");
    if (AS.isNull()) {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper create AS from scratch");
        QPDFObjectHandle::Rectangle rect = aoh.getRect();
        QPDFObjectHandle::Rectangle bbox(0, 0, rect.urx - rect.llx, rect.ury - rect.lly);
        QPDFObjectHandle dict =
            QPDFObjectHandle::parse("<< /Resources << /ProcSet [ /PDF /Text ] >>"
                                    " /Type /XObject /Subtype /Form >>");
        dict.replaceKey("/BBox", QPDFObjectHandle::newFromRectangle(bbox));
        AS = QPDFObjectHandle::newStream(this->oh.getOwningQPDF(), "/Tx BMC\nEMC\n");
        AS.replaceDict(dict);
        QPDFObjectHandle AP = aoh.getAppearanceDictionary();
        if (AP.isNull()) {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper create AP from scratch");
            aoh.getObjectHandle().replaceKey("/AP", QPDFObjectHandle::newDictionary());
            AP = aoh.getAppearanceDictionary();
        }
        AP.replaceKey("/N", AS);
    }
    if (!AS.isStream()) {
        aoh.getObjectHandle().warnIfPossible("unable to get normal appearance stream for update");
        return;
    }
    QPDFObjectHandle bbox_obj = AS.getDict().getKey("/BBox");
    if (!bbox_obj.isRectangle()) {
        aoh.getObjectHandle().warnIfPossible("unable to get appearance stream bounding box");
        return;
    }
    QPDFObjectHandle::Rectangle bbox = bbox_obj.getArrayAsRectangle();
    std::string DA = getDefaultAppearance();
    std::string V = getValueAsString();
    std::vector<std::string> opt;
    if (isChoice() && ((getFlags() & ff_ch_combo) == 0)) {
        opt = getChoices();
    }

    TfFinder tff;
    Pl_QPDFTokenizer tok("tf", &tff);
    tok.writeString(DA);
    tok.finish();
    double tf = tff.getTf();
    DA = tff.getDA();

    std::string (*encoder)(std::string const&, char) = &QUtil::utf8_to_ascii;
    std::string font_name = tff.getFontName();
    if (!font_name.empty()) {
        // See if the font is encoded with something we know about.
        QPDFObjectHandle resources = AS.getDict().getKey("/Resources");
        QPDFObjectHandle font = getFontFromResource(resources, font_name);
        bool found_font_in_dr = false;
        if (!font.isInitialized()) {
            QPDFObjectHandle dr = getDefaultResources();
            font = getFontFromResource(dr, font_name);
            found_font_in_dr = font.isDictionary();
        }
        if (found_font_in_dr && resources.isDictionary()) {
            QTC::TC("qpdf", "QPDFFormFieldObjectHelper get font from /DR");
            if (resources.isIndirect()) {
                resources = resources.getQPDF().makeIndirectObject(resources.shallowCopy());
                AS.getDict().replaceKey("/Resources", resources);
            }
            // Use mergeResources to force /Font to be local
            resources.mergeResources("<< /Font << >> >>"_qpdf);
            resources.getKey("/Font").replaceKey(font_name, font);
        }

        if (font.isDictionary() && font.getKey("/Encoding").isName()) {
            std::string encoding = font.getKey("/Encoding").getName();
            if (encoding == "/WinAnsiEncoding") {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper WinAnsi");
                encoder = &QUtil::utf8_to_win_ansi;
            } else if (encoding == "/MacRomanEncoding") {
                encoder = &QUtil::utf8_to_mac_roman;
            }
        }
    }

    V = (*encoder)(V, '?');
    for (size_t i = 0; i < opt.size(); ++i) {
        opt.at(i) = (*encoder)(opt.at(i), '?');
    }

    AS.addTokenFilter(
        std::shared_ptr<QPDFObjectHandle::TokenFilter>(new ValueSetter(DA, V, opt, tf, bbox)));
}
