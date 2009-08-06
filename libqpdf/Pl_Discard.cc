
#include <qpdf/Pl_Discard.hh>

// Exercised in md5 test suite

DLL_EXPORT
Pl_Discard::Pl_Discard() :
    Pipeline("discard", 0)
{
}

DLL_EXPORT
Pl_Discard::~Pl_Discard()
{
}

DLL_EXPORT
void
Pl_Discard::write(unsigned char* buf, int len)
{
}

DLL_EXPORT
void
Pl_Discard::finish()
{
}
