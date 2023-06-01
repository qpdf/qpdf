#include <qpdf/Pl_Concatenate.hh>

Pl_Concatenate::Pl_Concatenate(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next)
{
}

Pl_Concatenate::~Pl_Concatenate() // NOLINT (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
}

void
Pl_Concatenate::write(unsigned char const* data, size_t len)
{
    getNext()->write(data, len);
}

void
Pl_Concatenate::finish()
{
}

void
Pl_Concatenate::manualFinish()
{
    getNext()->finish();
}
