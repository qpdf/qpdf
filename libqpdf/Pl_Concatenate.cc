#include <qpdf/Pl_Concatenate.hh>

#include <stdexcept>

Pl_Concatenate::Pl_Concatenate(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next)
{
    if (!next) {
        throw std::logic_error("Attempt to create Pl_Concatenate with nullptr as next");
    }
}

// Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
Pl_Concatenate::~Pl_Concatenate() = default;

void
Pl_Concatenate::write(unsigned char const* data, size_t len)
{
    next()->write(data, len);
}

void
Pl_Concatenate::finish()
{
}

void
Pl_Concatenate::manualFinish()
{
    next()->finish();
}
