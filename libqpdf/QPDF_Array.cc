#include <qpdf/QPDF_Array.hh>
#include <stdexcept>

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle> const& items) :
    items(items)
{
}

QPDF_Array::~QPDF_Array()
{
}

std::string
QPDF_Array::unparse()
{
    std::string result = "[ ";
    for (std::vector<QPDFObjectHandle>::iterator iter = this->items.begin();
	 iter != this->items.end(); ++iter)
    {
	result += (*iter).unparse();
	result += " ";
    }
    result += "]";
    return result;
}

int
QPDF_Array::getNItems() const
{
    return this->items.size();
}

QPDFObjectHandle
QPDF_Array::getItem(int n) const
{
    if ((n < 0) || (n >= (int)this->items.size()))
    {
	throw std::logic_error(
	    "INTERNAL ERROR: bounds array accessing QPDF_Array element");
    }
    return this->items[n];
}

void
QPDF_Array::setItem(int n, QPDFObjectHandle const& oh)
{
    // Call getItem for bounds checking
    (void) getItem(n);
    this->items[n] = oh;
}
