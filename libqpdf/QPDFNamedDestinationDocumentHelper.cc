#include <qpdf/QPDF.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/QPDFNamedDestinationDocumentHelper.hh>

class QPDFNamedDestinationDocumentHelper::Members
{
  public:
    Members() = default;
};

QPDFNamedDestinationDocumentHelper::QPDFNamedDestinationDocumentHelper(QPDF& qpdf) :
    QPDFDocumentHelper(qpdf),
    m(new Members())
{
}

QPDFNamedDestinationDocumentHelper::~QPDFNamedDestinationDocumentHelper() = default;

QPDFNamedDestinationObjectHelper
QPDFNamedDestinationDocumentHelper::lookup(QPDFObjectHandle dest)
{
    if (dest.isName()) {
        return lookup_name(dest.getName());
    } else if (dest.isString()) {
        return lookup_string(dest.getUTF8Value());
    }
    return QPDFNamedDestinationObjectHelper(QPDFObjectHandle::newNull());
}

QPDFNamedDestinationObjectHelper
QPDFNamedDestinationDocumentHelper::lookup(std::string const& name)
{
    auto result = lookup_name(name);
    if (result.is_null()) {
        result = lookup_string(name);
    }
    return result;
}

QPDFNamedDestinationObjectHelper
QPDFNamedDestinationDocumentHelper::lookup_name(std::string const& name)
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
QPDFNamedDestinationDocumentHelper::lookup_string(std::string const& name)
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
QPDFNamedDestinationDocumentHelper::for_each(
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
QPDFNamedDestinationDocumentHelper::find_page_index(QPDFExplicitDestinationObjectHelper const& dest)
{
    QPDFObjectHandle dest_array = dest.get_explicit_array();
    if (dest_array.isNull() || dest_array.empty() || dest.is_remote()) {
        return -1;
    }
    QPDFObjectHandle page_obj = dest_array.getArrayItem(0);
    return page_obj.isPageObject() ? qpdf.findPage(page_obj) : -1;
}
