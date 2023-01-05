#include <qpdf/QPDFObjGen.hh>

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObjectHelper.hh>
#include <qpdf/QPDFObject_private.hh>

#include <stdexcept>

// ABI: inline and pass og by value
std::ostream&
operator<<(std::ostream& os, const QPDFObjGen& og)
{
    os << og.obj << "," << og.gen;
    return os;
}

// ABI: inline
std::string
QPDFObjGen::unparse(char separator) const
{
    return std::to_string(obj) + separator + std::to_string(gen);
}

bool
QPDFObjGen::set::add(QPDFObjectHandle const& oh)
{
    if (auto* ptr = oh.getObjectPtr()) {
        return add(ptr->getObjGen());
    } else {
        throw std::logic_error("attempt to retrieve QPDFObjGen from "
                               "uninitialized QPDFObjectHandle");
        return false;
    }
}

bool
QPDFObjGen::set::add(QPDFObjectHelper const& helper)
{
    if (auto* ptr = helper.getObjectHandle().getObjectPtr()) {
        return add(ptr->getObjGen());
    } else {
        throw std::logic_error("attempt to retrieve QPDFObjGen from "
                               "uninitialized QPDFObjectHandle");
        return false;
    }
}

void
QPDFObjGen::set::erase(QPDFObjectHandle const& oh)
{
    if (auto* ptr = oh.getObjectPtr()) {
        erase(ptr->getObjGen());
    } else {
        throw std::logic_error("attempt to retrieve QPDFObjGen from "
                               "uninitialized QPDFObjectHandle");
    }
}

void
QPDFObjGen::set::erase(QPDFObjectHelper const& helper)
{
    if (auto* ptr = helper.getObjectHandle().getObjectPtr()) {
        erase(ptr->getObjGen());
    } else {
        throw std::logic_error("attempt to retrieve QPDFObjGen from "
                               "uninitialized QPDFObjectHandle");
    }
}
