#include <qpdf/Pl_Discard.hh>

// Exercised in md5 test suite

// Pl_Discard does not use the member pattern as thee is no prospect of it ever requiring data
// members.

Pl_Discard::Pl_Discard() :
    Pipeline("discard", nullptr)
{
}

// Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
Pl_Discard::~Pl_Discard() = default;

void
Pl_Discard::write(unsigned char const* buf, size_t len)
{
}

void
Pl_Discard::finish()
{
}
