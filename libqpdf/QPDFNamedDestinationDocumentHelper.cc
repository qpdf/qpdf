#include <qpdf/QPDFDocumentHelper.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/QPDFNamedDestinationDocumentHelper.hh>
#include <qpdf/QPDFNamedDestinationObjectHelper.hh>

#include <qpdf/DLL.h>

class QPDFNamedDestinationDocumentHelper::Members
{
  public:
    Members(QPDF& qpdf) :
        qpdf(qpdf)
    {
    }
    QPDF& qpdf;
};

QPDFNamedDestinationDocumentHelper::QPDFNamedDestinationDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(std::make_shared<Members>(qpdf))
{
}

QPDFNamedDestinationObjectHelper
QPDFNamedDestinationDocumentHelper::lookup(QPDFObjectHandle dest)
{
    if (dest.isName()) {
        return lookupName(dest.getName());
    } else if (dest.isString()) {
        return lookupString(dest.getUTF8Value());
    }
    return QPDFNamedDestinationObjectHelper(QPDFObjectHandle::newNull());
}

QPDFNamedDestinationObjectHelper
QPDFNamedDestinationDocumentHelper::lookup(std::string const& name)
{
    auto result = lookupName(name);
    if (result.isNull()) {
        result = lookupString(name);
    }
    return result;
}

QPDFNamedDestinationObjectHelper
QPDFNamedDestinationDocumentHelper::lookupName(std::string const& name)
{
    auto root = qpdf.getRoot();

    // Legacy /Dests
    auto dests = root.getKey("/Dests");
    if (dests.isDictionary() && dests.hasKey(name)) {
        return QPDFNamedDestinationObjectHelper(dests.getKey(name));
    }
    return QPDFNamedDestinationObjectHelper(QPDFObjectHandle::newNull());
}

QPDFNamedDestinationObjectHelper
QPDFNamedDestinationDocumentHelper::lookupString(std::string const& name)
{
    auto root = qpdf.getRoot();

    // Modern /Names/Dests
    auto names = root.getKey("/Names");
    if (names.isDictionary()) {
        auto n_dests = names.getKey("/Dests");
        if (n_dests.isDictionary() || n_dests.isArray()) {
            QPDFNameTreeObjectHelper tree(n_dests, qpdf);
            QPDFObjectHandle found;
            if (tree.findObject(name, found)) {
                return QPDFNamedDestinationObjectHelper(found);
            }
        }
    }

    return QPDFNamedDestinationObjectHelper(QPDFObjectHandle::newNull());
}

void
QPDFNamedDestinationDocumentHelper::forEach(
    std::function<void(std::string const&, QPDFNamedDestinationObjectHelper const&, Kind)> callback)
{
    auto root = qpdf.getRoot();

    // Legacy /Dests
    auto dests = root.getKey("/Dests");
    if (dests.isDictionary()) {
        for (auto const& key: dests.getKeys()) {
            callback(key, QPDFNamedDestinationObjectHelper(dests.getKey(key)), Kind::NAME);
        }
    }

    // Modern /Names/Dests
    auto names = root.getKey("/Names");
    if (names.isDictionary()) {
        auto n_dests = names.getKey("/Dests");
        if (n_dests.isDictionary() || n_dests.isArray()) {
            QPDFNameTreeObjectHelper tree(n_dests, qpdf);
            for (auto const& i: tree) {
                callback(i.first, QPDFNamedDestinationObjectHelper(i.second), Kind::STRING);
            }
        }
    }
}

int
QPDFNamedDestinationDocumentHelper::findPageIndex(
    QPDFExplicitDestinationObjectHelper const& dest) const
{
    QPDFObjectHandle dest_array = dest.getExplicitArray();
    if (dest_array.isNull() || dest_array.empty() || dest.isRemote()) {
        return -1;
    }
    QPDFObjectHandle page_obj = dest_array.getArrayItem(0);
    return page_obj.isPageObject() ? qpdf.findPage(page_obj) : -1;
}
