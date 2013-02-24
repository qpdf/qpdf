#include <qpdf/Pl_PNGFilter.hh>
#include <stdexcept>
#include <string.h>

Pl_PNGFilter::Pl_PNGFilter(char const* identifier, Pipeline* next,
			   action_e action, unsigned int columns,
			   unsigned int bytes_per_pixel) :
    Pipeline(identifier, next),
    action(action),
    columns(columns),
    cur_row(0),
    prev_row(0),
    buf1(0),
    buf2(0),
    pos(0)
{
    this->buf1 = new unsigned char[columns + 1];
    this->buf2 = new unsigned char[columns + 1];
    this->cur_row = buf1;

    // number of bytes per incoming row
    this->incoming = (action == a_encode ? columns : columns + 1);
}

Pl_PNGFilter::~Pl_PNGFilter()
{
    delete [] buf1;
    delete [] buf2;
}

void
Pl_PNGFilter::write(unsigned char* data, size_t len)
{
    size_t left = this->incoming - this->pos;
    size_t offset = 0;
    while (len >= left)
    {
	// finish off current row
	memcpy(this->cur_row + this->pos, data + offset, left);
	offset += left;
	len -= left;

	processRow();

	// Swap rows
	unsigned char* t = this->prev_row;
	this->prev_row = this->cur_row;
	this->cur_row = t ? t : this->buf2;
	memset(this->cur_row, 0, this->columns + 1);
	left = this->incoming;
	this->pos = 0;
    }
    if (len)
    {
	memcpy(this->cur_row + this->pos, data + offset, len);
    }
    this->pos += len;
}

void
Pl_PNGFilter::processRow()
{
    if (this->action == a_encode)
    {
	encodeRow();
    }
    else
    {
	decodeRow();
    }
}

void
Pl_PNGFilter::decodeRow()
{
    int filter = this->cur_row[0];
    if (this->prev_row)
    {
	switch (filter)
	{
	  case 0:			// none
	    break;

	  case 1:			// sub
	    throw std::logic_error("sub filter not implemented");
	    break;

	  case 2:			// up
	    for (unsigned int i = 1; i <= this->columns; ++i)
	    {
		this->cur_row[i] += this->prev_row[i];
	    }
	    break;

	  case 3:			// average
	    throw std::logic_error("average filter not implemented");
	    break;

	  case 4:			// Paeth
	    throw std::logic_error("Paeth filter not implemented");
	    break;

	  default:
	    // ignore
	    break;
	}
    }

    getNext()->write(this->cur_row + 1, this->columns);
}

void
Pl_PNGFilter::encodeRow()
{
    // For now, hard-code to using UP filter.
    unsigned char ch = 2;
    getNext()->write(&ch, 1);
    if (this->prev_row)
    {
	for (unsigned int i = 0; i < this->columns; ++i)
	{
	    ch = this->cur_row[i] - this->prev_row[i];
	    getNext()->write(&ch, 1);
	}
    }
    else
    {
	getNext()->write(this->cur_row, this->columns);
    }
}

void
Pl_PNGFilter::finish()
{
    if (this->pos)
    {
	// write partial row
	processRow();
    }
    this->prev_row = 0;
    this->cur_row = buf1;
    this->pos = 0;
    memset(this->cur_row, 0, this->columns + 1);

    getNext()->finish();
}
