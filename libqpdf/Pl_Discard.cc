#include <qpdf/Pl_Discard.hh>

// Exercised in md5 test suite

Pl_Discard::Pl_Discard() :
    Pipeline("discard", 0)
{
}

Pl_Discard::~Pl_Discard()
{
}

void
Pl_Discard::write(unsigned char* buf, size_t len)
{
}

void
Pl_Discard::finish()
{
}
