#include <qpdf/QPDFNamedDestinationObjectHelper.hh>
#include <qpdf/QPDFObjGen.hh>
#include <set>

QPDFNamedDestinationObjectHelper::QPDFNamedDestinationObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh)
{
}

QPDFExplicitDestinationObjectHelper
QPDFNamedDestinationObjectHelper::unwrap() const
{
    std::set<QPDFObjGen> visited;
    QPDFObjectHandle current = oh();
    int depth = 0;
    const int MAX_DEPTH = 64; // arbitrary limit

    for (; depth < MAX_DEPTH; ++depth) {
        if (current.isIndirect()) {
            QPDFObjGen og = current.getObjGen();
            if (visited.count(og)) {
                return QPDFExplicitDestinationObjectHelper(QPDFObjectHandle::newNull());
            }
            visited.insert(og);
        }

        if (current.isDictionary() && current.hasKey("/D")) {
            current = current.getKey("/D");
        } else {
            break;
        }
    }

    if (depth >= MAX_DEPTH || !current.isArray()) {
        return QPDFExplicitDestinationObjectHelper(QPDFObjectHandle::newNull());
    }

    return QPDFExplicitDestinationObjectHelper(current);
}

bool
QPDFNamedDestinationObjectHelper::strictly_compliant() const
{
    if (oh().isArray()) {
        return true;
    }
    if (oh().isDictionary() && oh().hasKey("/D")) {
        return oh().getKey("/D").isArray();
    }
    return false;
}
