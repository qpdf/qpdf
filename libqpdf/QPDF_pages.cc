#include <qpdf/QPDF.hh>

#include <assert.h>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFExc.hh>

std::vector<QPDFObjectHandle> const&
QPDF::getAllPages()
{
    if (this->all_pages.empty())
    {
	getAllPagesInternal(
	    this->trailer.getKey("/Root").getKey("/Pages"), this->all_pages);
    }
    return this->all_pages;
}

void
QPDF::getAllPagesInternal(QPDFObjectHandle cur_pages,
			  std::vector<QPDFObjectHandle>& result)
{
    std::string type = cur_pages.getKey("/Type").getName();
    if (type == "/Pages")
    {
	QPDFObjectHandle kids = cur_pages.getKey("/Kids");
	int n = kids.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    getAllPagesInternal(kids.getArrayItem(i), result);
	}
    }
    else if (type == "/Page")
    {
	result.push_back(cur_pages);
    }
    else
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      this->last_object_description,
		      this->file->getLastOffset(),
		      ": invalid Type in page tree");
    }
}

// FIXXX here down

void
QPDF::clearPagesCache()
{
    this->all_pages.clear();
    this->pageobj_to_pages_pos.clear();
}

void
QPDF::flattenPagesTree()
{
    clearPagesCache();

    // FIXME: more specific method, we don't want to generate the extra stuff.
    // We also need cheap fixup after addPage/removePage.

    // no compressed objects to be produced here...
    std::map<int, int> object_stream_data;
    optimize(object_stream_data); // push down inheritance

    std::vector<QPDFObjectHandle> kids = this->getAllPages();
    QPDFObjectHandle pages = this->trailer.getKey("/Root").getKey("/Pages");

    const int len = kids.size();
    for (int pos = 0; pos < len; ++pos)
    {
        // populate pageobj_to_pages_pos
        ObjGen og(kids[pos].getObjectID(), kids[pos].getGeneration());
        if (! this->pageobj_to_pages_pos.insert(std::make_pair(og, pos)).second)
        {
            // insert failed: duplicate entry found
            *out_stream << "WARNING: duplicate page reference found, "
                        << "but currently not fully supported." << std::endl;
        }

        // fix parent links
        kids[pos].replaceKey("/Parent", pages);
    }

    pages.replaceKey("/Kids", QPDFObjectHandle::newArray(kids));
    // /Count has not changed
    assert(pages.getKey("/Count").getIntValue() == len);
}

int
QPDF::findPage(int objid, int generation)
{
    if (this->pageobj_to_pages_pos.empty())
    {
        flattenPagesTree();
    }
    std::map<ObjGen, int>::iterator it =
        this->pageobj_to_pages_pos.find(ObjGen(objid, generation));
    if (it != this->pageobj_to_pages_pos.end())
    {
        return (*it).second;
    }
    return -1; // throw?
}

int
QPDF::findPage(QPDFObjectHandle const& pageoh)
{
    if (!pageoh.isInitialized())
    {
        return -1;
        // TODO? throw
    }
    return findPage(pageoh.getObjectID(), pageoh.getGeneration());
}

void
QPDF::addPage(QPDFObjectHandle newpage, bool first)
{
    if (this->pageobj_to_pages_pos.empty())
    {
        flattenPagesTree();
    }

    newpage.assertPageObject(); // FIXME: currently private

    QPDFObjectHandle pages = this->trailer.getKey("/Root").getKey("/Pages");
    QPDFObjectHandle kids = pages.getKey("/Kids");

    newpage.replaceKey("/Parent", pages);
    if (first)
    {
        kids.insertItem(0, newpage);
    }
    else
    {
        kids.appendItem(newpage);
    }
    pages.replaceKey("/Count",
                     QPDFObjectHandle::newInteger(kids.getArrayNItems()));

    // FIXME: this is overkill, but cache is now stale
    clearPagesCache();
}

void
QPDF::addPageAt(QPDFObjectHandle newpage, bool before,
                QPDFObjectHandle const &refpage)
{
    int refpos = findPage(refpage); // also ensures flat /Pages
    if (refpos == -1)
    {
        throw "Could not find refpage";
    }

    newpage.assertPageObject();

    QPDFObjectHandle pages = this->trailer.getKey("/Root").getKey("/Pages");
    QPDFObjectHandle kids = pages.getKey("/Kids");

    if (! before)
    {
        ++refpos;
    }

    newpage.replaceKey("/Parent", pages);
    kids.insertItem(refpos, newpage);
    pages.replaceKey("/Count",
                     QPDFObjectHandle::newInteger(kids.getArrayNItems()));

    // FIXME: this is overkill, but cache is now stale
    clearPagesCache();
}

void
QPDF::removePage(QPDFObjectHandle const& pageoh)
{
    int pos = findPage(pageoh); // also ensures flat /Pages
    if (pos == -1)
    {
        throw "Can't remove non-existing page";
    }

    QPDFObjectHandle pages = this->trailer.getKey("/Root").getKey("/Pages");
    QPDFObjectHandle kids = pages.getKey("/Kids");

    kids.eraseItem(pos);
    pages.replaceKey("/Count",
                     QPDFObjectHandle::newInteger(kids.getArrayNItems()));

    // FIXME: this is overkill, but cache is now stale
    clearPagesCache();
}
