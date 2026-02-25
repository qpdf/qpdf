#include <qpdf/QPDFNamedDestinationObjectHelper.hh>
#include <qpdf/QPDFObjGen.hh>
#include <set>

class QPDFNamedDestinationObjectHelper::Members
{
  public:
    Members(QPDFObjectHandle oh) :
        oh(oh),
        resolved(false),
        cached_result(QPDFObjectHandle::newNull())
    {
    }

    QPDFExplicitDestinationObjectHelper
    unwrap()
    {
        if (resolved) {
            return cached_result;
        }

        std::set<QPDFObjGen> visited;
        QPDFObjectHandle current = oh;
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

        cached_result = QPDFExplicitDestinationObjectHelper(current);
        resolved = true;
        return cached_result;
    }

    bool
    hasValidWrapper() const
    {
        if (oh.isArray()) {
            return true;
        }
        if (oh.isDictionary() && oh.hasKey("/D")) {
            return oh.getKey("/D").isArray();
        }
        return false;
    }

    bool
    isNull() const
    {
        return oh.isNull();
    }

  private:
    QPDFObjectHandle oh;
    mutable bool resolved;
    mutable QPDFExplicitDestinationObjectHelper cached_result;
};

QPDFNamedDestinationObjectHelper::QPDFNamedDestinationObjectHelper() :
    QPDFNamedDestinationObjectHelper(QPDFObjectHandle::newNull())
{
}

QPDFNamedDestinationObjectHelper::QPDFNamedDestinationObjectHelper(QPDFObjectHandle o) :
    QPDFObjectHelper(o),
    m(std::make_shared<Members>(o))
{
}

QPDFExplicitDestinationObjectHelper
QPDFNamedDestinationObjectHelper::unwrap() const
{
    return m->unwrap();
}

bool
QPDFNamedDestinationObjectHelper::hasValidWrapper() const
{
    return m->hasValidWrapper();
}

bool
QPDFNamedDestinationObjectHelper::isNull() const
{
    return m->isNull();
}
