#include <qpdf/Pipeline.hh>
#include <stdexcept>

Pipeline::Members::Members(Pipeline* next) :
    next(next)
{
}

Pipeline::Members::~Members()
{
}

Pipeline::Pipeline(char const* identifier, Pipeline* next) :
    identifier(identifier),
    m(new Members(next))
{
}

Pipeline::~Pipeline()
{
}

Pipeline*
Pipeline::getNext(bool allow_null)
{
    if ((this->m->next == 0) && (! allow_null))
    {
	throw std::logic_error(
	    this->identifier +
	    ": Pipeline::getNext() called on pipeline with no next");
    }
    return this->m->next;
}

std::string
Pipeline::getIdentifier() const
{
    return this->identifier;
}
