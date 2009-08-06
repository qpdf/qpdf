

#include <qpdf/Pipeline.hh>

DLL_EXPORT
Pipeline::Pipeline(char const* identifier, Pipeline* next) :
    identifier(identifier),
    next(next)
{
}

DLL_EXPORT
Pipeline::~Pipeline()
{
}

Pipeline*
Pipeline::getNext(bool allow_null)
{
    if ((next == 0) && (! allow_null))
    {
	throw Exception(
	    this->identifier +
	    ": Pipeline::getNext() called on pipeline with no next");
    }
    return this->next;
}
