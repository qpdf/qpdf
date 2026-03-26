#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QUtil.hh>

QPDFPageDocumentHelper::QPDFPageDocumentHelper(QPDF& pdf) :
    QPDFDocumentHelper(pdf)
{
}

void
QPDFPageDocumentHelper::copyPages(std::vector<QPDFPageObjectHelper> const& pages,
                                  std::vector<int> const& range,
                                  QPDF& dest_pdf,
                                  bool preserve_oc_properties)
{
    if (pages.empty())
    {
        return;
    }

    // Copy the pages
    for (int idx: range)
    {
        if (idx < 0 || static_cast<size_t>(idx) >= pages.size())
        {
            throw std::runtime_error("Invalid page range index");
        }
        
        QPDFPageObjectHelper const& page = pages.at(idx);
        QPDFObjectHandle page_dict = page.getObjectHandle();
        
        // Copy page object to destination
        QPDFObjectHandle new_page = dest_pdf.makeIndirectObject(page_dict.shallowCopy());
        
        // Add page to destination document
        dest_pdf.addPage(new_page, false);
    }
    
    // Preserve OCProperties if requested and source document has them
    if (preserve_oc_properties)
    {
        copyOCProperties(dest_pdf);
    }
}

void
QPDFPageDocumentHelper::copyOCProperties(QPDF& dest_pdf)
{
    QPDFObjectHandle catalog = pdf.getRoot();
    QPDFObjectHandle dest_catalog = dest_pdf.getRoot();
    
    // Check if source document has OCProperties
    if (catalog.hasKey("/OCProperties"))
    {
        QPDFObjectHandle oc_props = catalog.getKey("/OCProperties");
        
        // Only copy if destination doesn't already have OCProperties
        if (!dest_catalog.hasKey("/OCProperties"))
        {
            // Make a deep copy of the OCProperties dictionary
            QPDFObjectHandle oc_props_copy = dest_pdf.makeIndirectObject(oc_props.shallowCopy());
            dest_catalog.replaceKey("/OCProperties", oc_props_copy);
        }
    }
}

std::vector<QPDFPageObjectHelper>
QPDFPageDocumentHelper::getAllPages()
{
    std::vector<QPDFPageObjectHelper> result;
    
    QPDFObjectHandle catalog = pdf.getRoot();
    if (!catalog.hasKey("/Pages"))
    {
        return result;
    }
    
    QPDFObjectHandle pages_node = catalog.getKey("/Pages");
    if (!pages_node.isDictionary())
    {
        return result;
    }
    
    // Get the Kids array from the Pages node
    if (!pages_node.hasKey("/Kids"))
    {
        return result;
    }
    
    QPDFObjectHandle kids = pages_node.getKey("/Kids");
    if (!kids.isArray())
    {
        return result;
    }
    
    // Recursively collect all page objects
    collectPageObjects(kids, result);
    
    return result;
}

void
QPDFPageDocumentHelper::collectPageObjects(QPDFObjectHandle node,
                                           std::vector<QPDFPageObjectHelper>& pages)
{
    if (node.isArray())
    {
        // This is a Kids array - iterate through its elements
        for (int i = 0; i < node.getArrayNItems(); ++i)
        {
            QPDFObjectHandle kid = node.getArrayItem(i);
            collectPageObjects(kid, pages);
        }
    }
    else if (node.isDictionary())
    {
        // Check if this is a Page or Pages node
        if (node.hasKey("/Type"))
        {
            QPDFObjectHandle type = node.getKey("/Type");
            if (type.isName() && type.getName() == "/Page")
            {
                // This is a Page object - add it to our collection
                pages.emplace_back(node);
            }
            else if (type.isName() && type.getName() == "/Pages" && node.hasKey("/Kids"))
            {
                // This is a Pages node - recurse into its Kids
                collectPageObjects(node.getKey("/Kids"), pages);
            }
        }
    }
}