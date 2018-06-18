#include <qpdf/QPDFPageDocumentHelper.hh>

QPDFPageDocumentHelper::Members::~Members()
{
}

QPDFPageDocumentHelper::Members::Members()
{
}

QPDFPageDocumentHelper::QPDFPageDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf)
{
}

std::vector<QPDFPageObjectHelper>
QPDFPageDocumentHelper::getAllPages()
{
    std::vector<QPDFObjectHandle> const& pages_v = this->qpdf.getAllPages();
    std::vector<QPDFPageObjectHelper> pages;
    for (std::vector<QPDFObjectHandle>::const_iterator iter = pages_v.begin();
         iter != pages_v.end(); ++iter)
    {
        pages.push_back(QPDFPageObjectHelper(*iter));
    }
    return pages;
}

void
QPDFPageDocumentHelper::pushInheritedAttributesToPage()
{
    this->qpdf.pushInheritedAttributesToPage();
}

void
QPDFPageDocumentHelper::addPage(QPDFPageObjectHelper newpage, bool first)
{
    this->qpdf.addPage(newpage.getObjectHandle(), first);
}

void
QPDFPageDocumentHelper::addPageAt(QPDFPageObjectHelper newpage, bool before,
                                  QPDFPageObjectHelper refpage)
{
    this->qpdf.addPageAt(newpage.getObjectHandle(), before,
                         refpage.getObjectHandle());
}

void
QPDFPageDocumentHelper::removePage(QPDFPageObjectHelper page)
{
    this->qpdf.removePage(page.getObjectHandle());
}
