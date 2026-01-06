#include <qpdf/QPDFFormFieldObjectHelper.hh>

#include <qpdf/AcroForm.hh>

#include <qpdf/Pipeline_private.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDF_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <cstdlib>

#include <memory>

using namespace qpdf;

using FormNode = qpdf::impl::FormNode;

const QPDFObjectHandle FormNode::null_oh;

class QPDFFormFieldObjectHelper::Members: public FormNode
{
  public:
    Members(QPDFObjectHandle const& oh) :
        FormNode(oh)
    {
    }
};

QPDFFormFieldObjectHelper::QPDFFormFieldObjectHelper(QPDFObjectHandle o) :
    QPDFObjectHelper(o),
    m(std::make_shared<Members>(oh()))
{
}

QPDFFormFieldObjectHelper::QPDFFormFieldObjectHelper() :
    QPDFObjectHelper(Null::temp()),
    m(std::make_shared<Members>(QPDFObjectHandle()))
{
}

bool
QPDFFormFieldObjectHelper::isNull()
{
    return m->null();
}

QPDFFormFieldObjectHelper
QPDFFormFieldObjectHelper::getParent()
{
    return {Null::if_null(m->Parent().oh())};
}

QPDFFormFieldObjectHelper
QPDFFormFieldObjectHelper::getTopLevelField(bool* is_different)
{
    return Null::if_null(m->root_field(is_different).oh());
}

FormNode
FormNode::root_field(bool* is_different)
{
    if (is_different) {
        *is_different = false;
    }
    if (!obj) {
        return {};
    }
    auto rf = *this;
    size_t depth = 0; // Don't bother with loop detection until depth becomes suspicious
    QPDFObjGen::set seen;
    while (rf.Parent() && (++depth < 10 || seen.add(rf))) {
        rf = rf.Parent();
        if (is_different) {
            *is_different = true;
        }
    }
    return rf;
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getInheritableFieldValue(std::string const& name)
{
    return Null::if_null(m->inheritable_value<QPDFObjectHandle>(name));
}

QPDFObjectHandle const&
FormNode::inherited(std::string const& name, bool acroform) const
{
    if (!obj) {
        return null_oh;
    }
    auto node = *this;
    QPDFObjGen::set seen;
    size_t depth = 0; // Don't bother with loop detection until depth becomes suspicious
    while (node.Parent() && (++depth < 10 || seen.add(node))) {
        node = node.Parent();
        if (auto const& result = node[name]) {
            return {result};
        }
    }
    return acroform ? from_AcroForm(name) : null_oh;
}

std::string
QPDFFormFieldObjectHelper::getInheritableFieldValueAsString(std::string const& name)
{
    return m->inheritable_string(name);
}

std::string
FormNode::inheritable_string(std::string const& name) const
{
    if (auto fv = inheritable_value<String>(name)) {
        return fv.utf8_value();
    }
    return {};
}

std::string
QPDFFormFieldObjectHelper::getInheritableFieldValueAsName(std::string const& name)
{
    if (auto fv = m->inheritable_value<Name>(name)) {
        return fv;
    }
    return {};
}

std::string
QPDFFormFieldObjectHelper::getFieldType()
{
    if (auto ft = m->FT()) {
        return ft;
    }
    return {};
}

std::string
QPDFFormFieldObjectHelper::getFullyQualifiedName()
{
    return m->fully_qualified_name();
}

std::string
FormNode::fully_qualified_name() const
{
    std::string result;
    auto node = *this;
    QPDFObjGen::set seen;
    size_t depth = 0; // Don't bother with loop detection until depth becomes suspicious
    while (node && (++depth < 10 || seen.add(node))) {
        if (auto T = node.T()) {
            if (!result.empty()) {
                result.insert(0, 1, '.');
            }
            result.insert(0, T.utf8_value());
        }
        node = node.Parent();
    }
    return result;
}

std::string
QPDFFormFieldObjectHelper::getPartialName()
{
    return m->partial_name();
}

std::string
FormNode::partial_name() const
{
    if (auto pn = T()) {
        return pn.utf8_value();
    }
    return {};
}

std::string
QPDFFormFieldObjectHelper::getAlternativeName()
{
    return m->alternative_name();
}

std::string
FormNode::alternative_name() const
{
    if (auto an = TU()) {
        return an.utf8_value();
    }
    return fully_qualified_name();
}

std::string
QPDFFormFieldObjectHelper::getMappingName()
{
    return m->mapping_name();
}

std::string
FormNode::mapping_name() const
{
    if (auto mn = TM()) {
        return mn.utf8_value();
    }
    return alternative_name();
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getValue()
{
    return Null::if_null(m->V<QPDFObjectHandle>());
}

std::string
QPDFFormFieldObjectHelper::getValueAsString()
{
    return m->value();
}

std::string
FormNode::value() const
{
    return inheritable_string("/V");
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getDefaultValue()
{
    return Null::if_null(m->DV());
}

std::string
QPDFFormFieldObjectHelper::getDefaultValueAsString()
{
    return m->default_value();
}

std::string
FormNode::default_value() const
{
    return inheritable_string("/DV");
}

QPDFObjectHandle
QPDFFormFieldObjectHelper::getDefaultResources()
{
    return Null::if_null(m->getDefaultResources());
}

QPDFObjectHandle
FormNode::getDefaultResources()
{
    return from_AcroForm("/DR");
}

std::string
QPDFFormFieldObjectHelper::getDefaultAppearance()
{
    return m->default_appearance();
}

std::string
FormNode::default_appearance() const
{
    if (auto DA = inheritable_value<String>("/DA")) {
        return DA.utf8_value();
    }
    if (String DA = from_AcroForm("/DA")) {
        return DA.utf8_value();
    }
    return {};
}

int
QPDFFormFieldObjectHelper::getQuadding()
{
    return m->getQuadding();
}

int
FormNode::getQuadding()
{
    auto fv = inheritable_value<QPDFObjectHandle>("/Q");
    bool looked_in_acroform = false;
    if (!fv.isInteger()) {
        fv = from_AcroForm("/Q");
        looked_in_acroform = true;
    }
    if (fv.isInteger()) {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper Q present", looked_in_acroform ? 0 : 1);
        return QIntC::to_int(fv.getIntValue());
    }
    return 0;
}

int
QPDFFormFieldObjectHelper::getFlags()
{
    return m->getFlags();
}

int
FormNode::getFlags()
{
    auto f = inheritable_value<QPDFObjectHandle>("/Ff");
    return f.isInteger() ? f.getIntValueAsInt() : 0;
}

bool
QPDFFormFieldObjectHelper::isText()
{
    return m->isText();
}

bool
FormNode::isText()
{
    return FT() == "/Tx";
}

bool
QPDFFormFieldObjectHelper::isCheckbox()
{
    return m->isCheckbox();
}

bool
FormNode::isCheckbox()
{
    return FT() == "/Btn" && (getFlags() & (ff_btn_radio | ff_btn_pushbutton)) == 0;
}

bool
QPDFFormFieldObjectHelper::isChecked()
{
    return m->isChecked();
}

bool
FormNode::isChecked()
{
    return isCheckbox() && V<Name>() != "/Off";
}

bool
QPDFFormFieldObjectHelper::isRadioButton()
{
    return m->isRadioButton();
}

bool
FormNode::isRadioButton()
{
    return FT() == "/Btn" && (getFlags() & ff_btn_radio) == ff_btn_radio;
}

bool
QPDFFormFieldObjectHelper::isPushbutton()
{
    return m->isPushbutton();
}

bool
FormNode::isPushbutton()
{
    return FT() == "/Btn" && (getFlags() & ff_btn_pushbutton) == ff_btn_pushbutton;
}

bool
QPDFFormFieldObjectHelper::isChoice()
{
    return m->isChoice();
}

bool
FormNode::isChoice()
{
    return FT() == "/Ch";
}

std::vector<std::string>
QPDFFormFieldObjectHelper::getChoices()
{
    return m->getChoices();
}

std::vector<std::string>
FormNode::getChoices()
{
    if (!isChoice()) {
        return {};
    }
    std::vector<std::string> result;
    for (auto const& item: inheritable_value<Array>("/Opt")) {
        if (item.isString()) {
            result.emplace_back(item.getUTF8Value());
        } else if (item.size() == 2) {
            auto display = item.getArrayItem(1);
            if (display.isString()) {
                result.emplace_back(display.getUTF8Value());
            }
        }
    }
    return result;
}

void
QPDFFormFieldObjectHelper::setFieldAttribute(std::string const& key, QPDFObjectHandle value)
{
    m->setFieldAttribute(key, value);
}

void
FormNode::setFieldAttribute(std::string const& key, QPDFObjectHandle value)
{
    replace(key, value);
}

void
FormNode::setFieldAttribute(std::string const& key, Name const& value)
{
    replace(key, value);
}

void
QPDFFormFieldObjectHelper::setFieldAttribute(std::string const& key, std::string const& utf8_value)
{
    m->setFieldAttribute(key, utf8_value);
}

void
FormNode::setFieldAttribute(std::string const& key, std::string const& utf8_value)
{
    replace(key, String::utf16(utf8_value));
}

void
QPDFFormFieldObjectHelper::setV(QPDFObjectHandle value, bool need_appearances)
{
    m->setV(value, need_appearances);
}

void
FormNode::setV(QPDFObjectHandle value, bool need_appearances)
{
    if (FT() == "/Btn") {
        Name name = value;
        if (isCheckbox()) {
            if (!name) {
                warn("ignoring attempt to set a checkbox field to a value whose type is not name");
                return;
            }
            // Accept any value other than /Off to mean checked. Files have been seen that use
            // /1 or other values.
            setCheckBoxValue(name != "/Off");
            return;
        }
        if (isRadioButton()) {
            if (!name) {
                warn(
                    "ignoring attempt to set a radio button field to an object that is not a name");
                return;
            }
            setRadioButtonValue(name);
            return;
        }
        if (isPushbutton()) {
            warn("ignoring attempt set the value of a pushbutton field");
        }
        return;
    }
    if (value.isString()) {
        setFieldAttribute("/V", QPDFObjectHandle::newUnicodeString(value.getUTF8Value()));
    } else {
        setFieldAttribute("/V", value);
    }
    if (need_appearances) {
        QPDF& qpdf = oh().getQPDF(
            "QPDFFormFieldObjectHelper::setV called with need_appearances = "
            "true on an object that is not associated with an owning QPDF");
        qpdf.doc().acroform().setNeedAppearances(true);
    }
}

void
QPDFFormFieldObjectHelper::setV(std::string const& utf8_value, bool need_appearances)
{
    m->setV(utf8_value, need_appearances);
}

void
FormNode::setV(std::string const& utf8_value, bool need_appearances)
{
    setV(QPDFObjectHandle::newUnicodeString(utf8_value), need_appearances);
}

void
FormNode::setRadioButtonValue(Name const& name)
{
    qpdf_expect(name);
    // Set the value of a radio button field. This has the following specific behavior:
    // * If this is a node without /Kids, assume this is a individual radio button widget and call
    // itself on the parent
    // * If this is a radio button field with children, set /V to the given value. Then, for each
    //   child, if the child has the specified value as one of its keys in the /N subdictionary of
    //   its /AP (i.e. its normal appearance stream dictionary), set /AS to name; otherwise, if /Off
    //   is a member, set /AS to /Off.
    auto kids = Kids();
    if (!kids) {
        // This is most likely one of the individual buttons. Try calling on the parent.
        auto parent = Parent();
        if (parent.Kids()) {
            parent.setRadioButtonValue(name);
            return;
        }
    }
    if (!isRadioButton() || !kids) {
        warn("don't know how to set the value of this field as a radio button");
        return;
    }
    replace("/V", name);
    for (FormNode kid: kids) {
        auto ap = kid.AP();
        QPDFObjectHandle annot;
        if (!ap) {
            // The widget may be below. If there is more than one, just find the first one.
            for (FormNode grandkid: kid.Kids()) {
                ap = grandkid.AP();
                if (ap) {
                    annot = grandkid;
                    break;
                }
            }
        } else {
            annot = kid;
        }
        if (!annot) {
            warn("unable to set the value of this radio button");
            continue;
        }
        if (ap["/N"].contains(name.value())) {
            annot.replace("/AS", name);
        } else {
            annot.replace("/AS", Name("/Off"));
        }
    }
}

void
FormNode::setCheckBoxValue(bool value)
{
    auto ap = AP();
    QPDFObjectHandle annot;
    if (ap) {
        annot = oh();
    } else {
        // The widget may be below. If there is more than one, just find the first one.
        for (FormNode kid: Kids()) {
            ap = kid.AP();
            if (ap) {
                annot = kid;
                break;
            }
        }
    }
    std::string on_value;
    if (value) {
        // Set the "on" value to the first value in the appearance stream's normal state dictionary
        // that isn't /Off. If not found, fall back to /Yes.
        if (ap) {
            for (auto const& item: Dictionary(ap["/N"])) {
                if (item.first != "/Off") {
                    on_value = item.first;
                    break;
                }
            }
        }
        if (on_value.empty()) {
            on_value = "/Yes";
        }
    }

    // Set /AS to the on value or /Off in addition to setting /V.
    auto name = Name(value ? on_value : "/Off");
    setFieldAttribute("/V", name);
    if (!annot) {
        warn("unable to set the value of this checkbox");
        return;
    }
    annot.replace("/AS", name);
}

void
QPDFFormFieldObjectHelper::generateAppearance(QPDFAnnotationObjectHelper& aoh)
{
    m->generateAppearance(aoh);
}

void
FormNode::generateAppearance(QPDFAnnotationObjectHelper& aoh)
{
    // Ignore field types we don't know how to generate appearances for. Button fields don't really
    // need them -- see code in QPDFAcroFormDocumentHelper::generateAppearancesIfNeeded.
    auto ft = FT();
    if (ft == "/Tx" || ft == "/Ch") {
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
        enum { st_top, st_bmc, st_emc, st_end } state{st_top};
        bool replaced{false};
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
    bbox(bbox)
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
    if (!replaced) {
        QTC::TC("qpdf", "QPDFFormFieldObjectHelper replaced BMC at EOF");
        write("/Tx BMC\n");
        writeAppearance();
    }
}

void
ValueSetter::writeAppearance()
{
    replaced = true;

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
            if (wanted_first < 0) {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper list first too low");
                wanted_last -= wanted_first;
                wanted_first = 0;
            }
            if (wanted_last >= QIntC::to_int(nopt)) {
                QTC::TC("qpdf", "QPDFFormFieldObjectHelper list last too high");
                auto diff = wanted_last - QIntC::to_int(nopt) + 1;
                wanted_first = std::max(0, wanted_first - diff);
                wanted_last -= diff;
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
    class TfFinder final: public QPDFObjectHandle::TokenFilter
    {
      public:
        TfFinder() = default;
        ~TfFinder() final = default;

        void
        handleToken(QPDFTokenizer::Token const& token) final
        {
            auto ttype = token.getType();
            auto const& value = token.getValue();
            DA.emplace_back(token.getRawValue());
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
                        // These ranges are arbitrary but keep us from doing insane things or
                        // suffering from over/underflow
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
        getTf() const
        {
            return tf;
        }
        std::string
        getFontName() const
        {
            return font_name;
        }

        std::string
        getDA()
        {
            std::string result;
            int i = -1;
            for (auto const& cur: DA) {
                if (++i == tf_idx) {
                    double delta = strtod(cur.c_str(), nullptr) - tf;
                    if (delta > 0.001 || delta < -0.001) {
                        // tf doesn't match the font size passed to Tf, so substitute.
                        QTC::TC("qpdf", "QPDFFormFieldObjectHelper fallback Tf");
                        result += QUtil::double_to_string(tf);
                        continue;
                    }
                }
                result += cur;
            }
            return result;
        }

      private:
        double tf{11.0};
        int tf_idx{-1};
        std::string font_name;
        double last_num{0.0};
        int last_num_idx{-1};
        std::string last_name;
        std::vector<std::string> DA;
    };
} // namespace

void
FormNode::generateTextAppearance(QPDFAnnotationObjectHelper& aoh)
{
    no_ci_warn_if(
        !Dictionary(aoh), // There is no guarantee that aoh is a dictionary
        "cannot generate appearance for non-dictionary annotation" //
    );
    Stream AS = aoh.getAppearanceStream("/N"); // getAppearanceStream returns a stream or null.
    if (!AS) {
        QPDFObjectHandle::Rectangle rect = aoh.getRect(); // may silently be invalid / all zeros
        QPDFObjectHandle::Rectangle bbox(0, 0, rect.urx - rect.llx, rect.ury - rect.lly);
        auto* pdf = qpdf();
        no_ci_stop_damaged_if(!pdf, "unable to get owning QPDF for appearance generation");
        AS = pdf->newStream("/Tx BMC\nEMC\n");
        AS.replaceDict(Dictionary(
            {{"/BBox", QPDFObjectHandle::newFromRectangle(bbox)},
             {"/Resources", Dictionary({{"/ProcSet", Array({Name("/PDF"), Name("/Text")})}})},
             {"/Type", Name("/XObject")},
             {"/Subtype", Name("/Form")}}));
        if (auto ap = AP()) {
            ap.replace("/N", AS);
        } else {
            aoh.replace("/AP", Dictionary({{"/N", AS}}));
        }
    }

    if (AS.obj_sp().use_count() > 3) {
        // Ensures that the appearance stream is not shared by copying it if the threshold of 3 is
        // exceeded. The threshold is based on the current implementation details:
        // - One reference from the local variable AS
        // - One reference from the appearance dictionary (/AP)
        // - One reference from the object table
        // If use_count() is greater than 3, it means the appearance stream is shared elsewhere,
        // and updating it could have unintended side effects. This threshold may need to be updated
        // if the internal reference counting changes in the future.
        //
        // There is currently no explicit CI test for this code. It has been manually tested by
        // running it through CI with a threshold of 0, unconditionally copying streams.
        auto data = AS.getStreamData(qpdf_dl_all);
        AS = AS.copy();
        AS.replaceStreamData(std::move(data), Null::temp(), Null::temp());
        if (Dictionary AP = aoh.getAppearanceDictionary()) {
            AP.replace("/N", AS);
        } else {
            aoh.replace("/AP", Dictionary({{"/N", AS}}));
            // aoh is a dictionary, so insertion will succeed. No need to check by retrieving it.
        }
    }
    QPDFObjectHandle bbox_obj = AS.getDict()["/BBox"];
    if (!bbox_obj.isRectangle()) {
        aoh.warn("unable to get appearance stream bounding box");
        return;
    }
    QPDFObjectHandle::Rectangle bbox = bbox_obj.getArrayAsRectangle();
    std::string DA = default_appearance();
    std::string V = value();

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
        Dictionary resources = AS.getDict()["/Resources"];
        Dictionary font = resources["/Font"][font_name];
        if (!font) {
            font = getDefaultResources()["/Font"][font_name];
            if (resources) {
                if (resources.indirect()) {
                    resources = resources.qpdf()->makeIndirectObject(resources.copy());
                    AS.getDict().replace("/Resources", resources);
                }
                // Use mergeResources to force /Font to be local
                QPDFObjectHandle res = resources;
                res.mergeResources(Dictionary({{"/Font", Dictionary::empty()}}));
                res.getKey("/Font").replace(font_name, font);
            }
        }

        if (Name Encoding = font["/Encoding"]) {
            if (Encoding == "/WinAnsiEncoding") {
                encoder = &QUtil::utf8_to_win_ansi;
            } else if (Encoding == "/MacRomanEncoding") {
                encoder = &QUtil::utf8_to_mac_roman;
            }
        }
    }

    V = (*encoder)(V, '?');

    std::vector<std::string> opt;
    if (isChoice() && (getFlags() & ff_ch_combo) == 0) {
        opt = getChoices();
        for (auto& o: opt) {
            o = (*encoder)(o, '?');
        }
    }

    std::string result;
    pl::String pl(result);
    ValueSetter vs(DA, V, opt, tf, bbox);
    Pl_QPDFTokenizer vs_tok("", &vs, &pl);
    vs_tok.writeString(AS.getStreamData(qpdf_dl_all));
    vs_tok.finish();
    AS.replaceStreamData(std::move(result), Null::temp(), Null::temp());
}
