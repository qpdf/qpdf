#include <qpdf/QPDFPageLabelDocumentHelper.hh>

#include <qpdf/QTC.hh>

class QPDFPageLabelDocumentHelper::Members
{
  public:
    Members() = default;
    Members(Members const&) = delete;
    ~Members() = default;

    std::unique_ptr<QPDFNumberTreeObjectHelper> labels;
};

QPDFPageLabelDocumentHelper::QPDFPageLabelDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(std::make_shared<Members>())
{
    QPDFObjectHandle root = qpdf.getRoot();
    if (root.hasKey("/PageLabels")) {
        m->labels =
            std::make_unique<QPDFNumberTreeObjectHelper>(root.getKey("/PageLabels"), this->qpdf);
    }
}

bool
QPDFPageLabelDocumentHelper::hasPageLabels()
{
    return m->labels != nullptr;
}

QPDFObjectHandle
QPDFPageLabelDocumentHelper::getLabelForPage(long long page_idx)
{
    if (!hasPageLabels()) {
        return QPDFObjectHandle::newNull();
    }
    QPDFNumberTreeObjectHelper::numtree_number offset = 0;
    QPDFObjectHandle label;
    if (!m->labels->findObjectAtOrBelow(page_idx, label, offset) || !label.isDictionary()) {
        return QPDFObjectHandle::newNull();
    }
    QPDFObjectHandle S = label.getKey("/S");   // type (D, R, r, A, a)
    QPDFObjectHandle P = label.getKey("/P");   // prefix
    QPDFObjectHandle St = label.getKey("/St"); // starting number
    long long start = 1;
    if (St.isInteger()) {
        start = St.getIntValue();
    }
    QIntC::range_check(start, offset);
    start += offset;
    auto result = QPDFObjectHandle::newDictionary();
    result.replaceKey("/S", S);
    result.replaceKey("/P", P);
    result.replaceKey("/St", QPDFObjectHandle::newInteger(start));
    return result;
}

void
QPDFPageLabelDocumentHelper::getLabelsForPageRange(
    long long start_idx,
    long long end_idx,
    long long new_start_idx,
    std::vector<QPDFObjectHandle>& new_labels)
{
    // Start off with a suitable label for the first page. For every remaining page, if that page
    // has an explicit entry, copy it. Otherwise, let the subsequent page just sequence from the
    // prior entry. If there is no entry for the first page, fabricate one that would match how the
    // page would look in a new file in which it also didn't have an explicit label.
    QPDFObjectHandle label = getLabelForPage(start_idx);
    if (label.isNull()) {
        label = QPDFObjectHandle::newDictionary();
        label.replaceKey("/St", QPDFObjectHandle::newInteger(1 + new_start_idx));
    }
    // See if the new label is redundant based on the previous entry in the vector. If so, don't add
    // it.
    size_t size = new_labels.size();
    bool skip_first = false;
    if (size >= 2) {
        QPDFObjectHandle last = new_labels.at(size - 1);
        QPDFObjectHandle last_idx = new_labels.at(size - 2);
        if (last_idx.isInteger() && last.isDictionary() &&
            (label.getKey("/S").unparse() == last.getKey("/S").unparse()) &&
            (label.getKey("/P").unparse() == last.getKey("/P").unparse()) &&
            label.getKey("/St").isInteger() && last.getKey("/St").isInteger()) {
            long long int st_delta =
                label.getKey("/St").getIntValue() - last.getKey("/St").getIntValue();
            long long int idx_delta = new_start_idx - last_idx.getIntValue();
            if (st_delta == idx_delta) {
                skip_first = true;
            }
        }
    }
    if (!skip_first) {
        new_labels.emplace_back(QPDFObjectHandle::newInteger(new_start_idx));
        new_labels.emplace_back(label);
    }

    long long int idx_offset = new_start_idx - start_idx;
    for (long long i = start_idx + 1; i <= end_idx; ++i) {
        if (m->labels->hasIndex(i) && (label = getLabelForPage(i)).isDictionary()) {
            new_labels.emplace_back(QPDFObjectHandle::newInteger(i + idx_offset));
            new_labels.emplace_back(label);
        }
    }
}

QPDFObjectHandle
QPDFPageLabelDocumentHelper::pageLabelDict(
    qpdf_page_label_e label_type, int start_num, std::string_view prefix)
{
    auto num = QPDFObjectHandle::newDictionary();
    switch (label_type) {
    case pl_none:
        break;
    case pl_digits:
        num.replaceKey("/S", "/D"_qpdf);
        break;
    case pl_alpha_lower:
        num.replaceKey("/S", "/a"_qpdf);
        break;
    case pl_alpha_upper:
        num.replaceKey("/S", "/A"_qpdf);
        break;
    case pl_roman_lower:
        num.replaceKey("/S", "/r"_qpdf);
        break;
    case pl_roman_upper:
        num.replaceKey("/S", "/R"_qpdf);
        break;
    }
    if (!prefix.empty()) {
        num.replaceKey("/P", QPDFObjectHandle::newUnicodeString(std::string(prefix)));
    }
    if (start_num != 1) {
        num.replaceKey("/St", QPDFObjectHandle::newInteger(start_num));
    }
    return num;
}
