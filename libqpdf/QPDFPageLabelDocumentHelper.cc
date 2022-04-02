#include <qpdf/QPDFPageLabelDocumentHelper.hh>

#include <qpdf/QTC.hh>

QPDFPageLabelDocumentHelper::Members::~Members()
{
}

QPDFPageLabelDocumentHelper::Members::Members()
{
}

QPDFPageLabelDocumentHelper::QPDFPageLabelDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(new Members())
{
    QPDFObjectHandle root = qpdf.getRoot();
    if (root.hasKey("/PageLabels")) {
        this->m->labels = make_pointer_holder<QPDFNumberTreeObjectHelper>(
            root.getKey("/PageLabels"), this->qpdf);
    }
}

bool
QPDFPageLabelDocumentHelper::hasPageLabels()
{
    return 0 != this->m->labels.get();
}

QPDFObjectHandle
QPDFPageLabelDocumentHelper::getLabelForPage(long long page_idx)
{
    QPDFObjectHandle result(QPDFObjectHandle::newNull());
    if (!hasPageLabels()) {
        return result;
    }
    QPDFNumberTreeObjectHelper::numtree_number offset = 0;
    QPDFObjectHandle label;
    if (!this->m->labels->findObjectAtOrBelow(page_idx, label, offset)) {
        return result;
    }
    if (!label.isDictionary()) {
        return result;
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
    result = QPDFObjectHandle::newDictionary();
    result.replaceOrRemoveKey("/S", S);
    result.replaceOrRemoveKey("/P", P);
    result.replaceOrRemoveKey("/St", QPDFObjectHandle::newInteger(start));
    return result;
}

void
QPDFPageLabelDocumentHelper::getLabelsForPageRange(
    long long start_idx,
    long long end_idx,
    long long new_start_idx,
    std::vector<QPDFObjectHandle>& new_labels)
{
    // Start off with a suitable label for the first page. For every
    // remaining page, if that page has an explicit entry, copy it.
    // Otherwise, let the subsequent page just sequence from the prior
    // entry. If there is no entry for the first page, fabricate one
    // that would match how the page would look in a new file in which
    // it also didn't have an explicit label.
    QPDFObjectHandle label = getLabelForPage(start_idx);
    if (label.isNull()) {
        label = QPDFObjectHandle::newDictionary();
        label.replaceKey(
            "/St", QPDFObjectHandle::newInteger(1 + new_start_idx));
    }
    // See if the new label is redundant based on the previous entry
    // in the vector. If so, don't add it.
    size_t size = new_labels.size();
    bool skip_first = false;
    if (size >= 2) {
        QPDFObjectHandle last = new_labels.at(size - 1);
        QPDFObjectHandle last_idx = new_labels.at(size - 2);
        if (last_idx.isInteger() && last.isDictionary() &&
            (label.getKey("/S").unparse() == last.getKey("/S").unparse()) &&
            (label.getKey("/P").unparse() == last.getKey("/P").unparse()) &&
            label.getKey("/St").isInteger() && last.getKey("/St").isInteger()) {
            long long int st_delta = label.getKey("/St").getIntValue() -
                last.getKey("/St").getIntValue();
            long long int idx_delta = new_start_idx - last_idx.getIntValue();
            if (st_delta == idx_delta) {
                QTC::TC("qpdf", "QPDFPageLabelDocumentHelper skip first");
                skip_first = true;
            }
        }
    }
    if (!skip_first) {
        new_labels.push_back(QPDFObjectHandle::newInteger(new_start_idx));
        new_labels.push_back(label);
    }

    long long int idx_offset = new_start_idx - start_idx;
    for (long long i = start_idx + 1; i <= end_idx; ++i) {
        if (this->m->labels->hasIndex(i) &&
            (label = getLabelForPage(i)).isDictionary()) {
            new_labels.push_back(QPDFObjectHandle::newInteger(i + idx_offset));
            new_labels.push_back(label);
        }
    }
}
