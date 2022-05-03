#include <qpdf/Pipeline.hh>

#include <cstring>
#include <stdexcept>

Pipeline::Pipeline(char const* identifier, Pipeline* next) :
    identifier(identifier),
    next(next)
{
}

Pipeline*
Pipeline::getNext(bool allow_null)
{
    if ((this->next == 0) && (!allow_null)) {
        throw std::logic_error(
            this->identifier +
            ": Pipeline::getNext() called on pipeline with no next");
    }
    return this->next;
}

std::string
Pipeline::getIdentifier() const
{
    return this->identifier;
}
