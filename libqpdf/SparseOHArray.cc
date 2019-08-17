#include <qpdf/SparseOHArray.hh>
#include <stdexcept>

SparseOHArray::SparseOHArray() :
    n_elements(0)
{
}

size_t
SparseOHArray::size() const
{
    return this->n_elements;
}

void
SparseOHArray::append(QPDFObjectHandle oh)
{
    if (! oh.isResolvedNull())
    {
        this->elements[this->n_elements] = oh;
    }
    ++this->n_elements;
}

QPDFObjectHandle
SparseOHArray::at(size_t idx) const
{
    if (idx >= this->n_elements)
    {
	throw std::logic_error(
	    "INTERNAL ERROR: bounds error accessing SparseOHArray element");
    }
    std::map<size_t, QPDFObjectHandle>::const_iterator iter =
        this->elements.find(idx);
    if (iter == this->elements.end())
    {
        return QPDFObjectHandle::newNull();
    }
    else
    {
        return (*iter).second;
    }
}

void
SparseOHArray::remove_last()
{
    if (this->n_elements == 0)
    {
	throw std::logic_error(
	    "INTERNAL ERROR: attempt to remove"
            " last item from empty SparseOHArray");
    }
    --this->n_elements;
    this->elements.erase(this->n_elements);
}

void
SparseOHArray::releaseResolved()
{
    for (std::map<size_t, QPDFObjectHandle>::iterator iter =
             this->elements.begin();
	 iter != this->elements.end(); ++iter)
    {
	QPDFObjectHandle::ReleaseResolver::releaseResolved((*iter).second);
    }
}

void
SparseOHArray::setAt(size_t idx, QPDFObjectHandle oh)
{
    if (idx >= this->n_elements)
    {
	throw std::logic_error("bounds error setting item in SparseOHArray");
    }
    if (oh.isResolvedNull())
    {
        this->elements.erase(idx);
    }
    else
    {
        this->elements[idx] = oh;
    }
}

void
SparseOHArray::erase(size_t idx)
{
    if (idx >= this->n_elements)
    {
	throw std::logic_error("bounds error erasing item from SparseOHArray");
    }
    std::map<size_t, QPDFObjectHandle> dest;
    for (std::map<size_t, QPDFObjectHandle>::iterator iter =
             this->elements.begin();
	 iter != this->elements.end(); ++iter)
    {
        if ((*iter).first < idx)
        {
            dest.insert(*iter);
        }
        else if ((*iter).first > idx)
        {
            dest[(*iter).first - 1] = (*iter).second;
        }
    }
    this->elements = dest;
    --this->n_elements;
}

void
SparseOHArray::insert(size_t idx, QPDFObjectHandle oh)
{
    if (idx > this->n_elements)
    {
        throw std::logic_error("bounds error inserting item to SparseOHArray");
    }
    else if (idx == this->n_elements)
    {
        // Allow inserting to the last position
        append(oh);
    }
    else
    {
        std::map<size_t, QPDFObjectHandle> dest;
        for (std::map<size_t, QPDFObjectHandle>::iterator iter =
                 this->elements.begin();
             iter != this->elements.end(); ++iter)
        {
            if ((*iter).first < idx)
            {
                dest.insert(*iter);
            }
            else
            {
                dest[(*iter).first + 1] = (*iter).second;
            }
        }
        this->elements = dest;
        this->elements[idx] = oh;
        ++this->n_elements;
    }
}
