#include <qpdf/ResourceFinder.hh>

ResourceFinder::ResourceFinder() :
    last_name_offset(0),
    saw_bad(false)
{
}

void
ResourceFinder::handleObject(QPDFObjectHandle obj, size_t offset, size_t)
{
    if (obj.isOperator() && (! this->last_name.empty()))
    {
        static std::map<std::string, std::string> op_to_rtype = {
            {"CS", "/ColorSpace"},
            {"cs", "/ColorSpace"},
            {"gs", "/ExtGState"},
            {"Tf", "/Font"},
            {"SCN", "/Pattern"},
            {"scn", "/Pattern"},
            {"BDC", "/Properties"},
            {"DP", "/Properties"},
            {"sh", "/Shading"},
            {"Do", "/XObject"},
        };
        std::string op = obj.getOperatorValue();
        std::string resource_type;
        auto iter = op_to_rtype.find(op);
        if (iter != op_to_rtype.end())
        {
            resource_type = iter->second;
        }
        if (! resource_type.empty())
        {
            this->names.insert(this->last_name);
            this->names_by_resource_type[
                resource_type][this->last_name].insert(this->last_name_offset);
        }
    }
    else if (obj.isName())
    {
        this->last_name = obj.getName();
        this->last_name_offset = offset;
    }
}

void
ResourceFinder::handleWarning()
{
    this->saw_bad = true;
}

std::set<std::string> const&
ResourceFinder::getNames() const
{
    return this->names;
}

std::map<std::string, std::map<std::string, std::set<size_t>>> const&
ResourceFinder::getNamesByResourceType() const
{
    return this->names_by_resource_type;
}

bool
ResourceFinder::sawBad() const
{
    return this->saw_bad;
}
