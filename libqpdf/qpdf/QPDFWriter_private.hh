#ifndef QPDFWRITER_PRIVATE_HH
#define QPDFWRITER_PRIVATE_HH

#include <qpdf/QPDFWriter.hh>

#include <qpdf/ObjTable.hh>
#include <qpdf/Pipeline_private.hh>

// This file is intended for inclusion by QPDFWriter, QPDF, QPDF_optimization and QPDF_linearization
// only.

struct QPDFWriter::Object
{
    int renumber{0};
    int gen{0};
    int object_stream{0};
};

struct QPDFWriter::NewObject
{
    QPDFXRefEntry xref;
    qpdf_offset_t length{0};
};

class QPDFWriter::ObjTable: public ::ObjTable<QPDFWriter::Object>
{
    friend class QPDFWriter;

  public:
    bool
    getStreamsEmpty() const noexcept
    {
        return streams_empty;
    }

  private:
    // For performance, set by QPDFWriter rather than tracked by ObjTable.
    bool streams_empty{false};
};

class QPDFWriter::NewObjTable: public ::ObjTable<QPDFWriter::NewObject>
{
    friend class QPDFWriter;
};

#endif // QPDFWRITER_PRIVATE_HH
