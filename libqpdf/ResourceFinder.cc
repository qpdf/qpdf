#include <qpdf/ResourceFinder.hh>

void
ResourceFinder::handleObject(QPDFObjectHandle obj, size_t offset, size_t)
{
    if (obj.isOperator() && !last_name.empty()) {
        static const std::map<std::string, std::string> op_to_rtype{
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
        auto iter = op_to_rtype.find(op);
        if (iter == op_to_rtype.end()) {
            return;
        }
        names.insert(last_name);
        names_by_resource_type[iter->second][last_name].push_back(last_name_offset);
    } else if (obj.isName()) {
        last_name = obj.getName();
        last_name_offset = offset;
    }
}

void
ResourceFinder::handleEOF()
{
}
