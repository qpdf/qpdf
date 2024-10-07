#ifndef QPDF_OBJECTS_HH
#define QPDF_OBJECTS_HH

#include <qpdf/QPDF.hh>

// The Objects class is responsible for keeping track of all objects belonging to a QPDF instance,
// including loading it from an input source when required.
class QPDF::Objects
{
  public:
    Objects(QPDF& qpdf, QPDF::Members* m)
    {
    }

    std::map<QPDFObjGen, ObjCache> obj_cache;
}; // Objects

#endif // QPDF_OBJECTS_HH
