#include <qpdf/Pl_MD5.hh>
#include <stdexcept>

DLL_EXPORT
Pl_MD5::Pl_MD5(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    in_progress(false)
{
}

DLL_EXPORT
Pl_MD5::~Pl_MD5()
{
}

DLL_EXPORT
void
Pl_MD5::write(unsigned char* buf, int len)
{
    if (! this->in_progress)
    {
	this->md5.reset();
	this->in_progress = true;
    }
    this->md5.encodeDataIncrementally((char*) buf, len);
    this->getNext()->write(buf, len);
}

DLL_EXPORT
void
Pl_MD5::finish()
{
    this->getNext()->finish();
    this->in_progress = false;
}

DLL_EXPORT
std::string
Pl_MD5::getHexDigest()
{
    if (this->in_progress)
    {
	throw std::logic_error(
	    "digest requested for in-progress MD5 Pipeline");
    }
    return this->md5.unparse();
}
