
#include <qpdf/Pl_StdioFile.hh>

#include <errno.h>

Pl_StdioFile::Pl_StdioFile(char const* identifier, FILE* f) :
    Pipeline(identifier, 0),
    file(f)
{
}

Pl_StdioFile::~Pl_StdioFile()
{
}

void
Pl_StdioFile::write(unsigned char* buf, int len)
{
    size_t so_far = 0;
    while (len > 0)
    {
	so_far = fwrite(buf, 1, len, this->file);
	if (so_far == 0)
	{
	    throw QEXC::System(this->identifier + ": Pl_StdioFile::write",
			       errno);
	}
	else
	{
	    buf += so_far;
	    len -= so_far;
	}
    }
}

void
Pl_StdioFile::finish()
{
    if (fileno(this->file) != -1)
    {
	fflush(this->file);
    }
    else
    {
	throw QEXC::Internal(this->identifier +
			     ": Pl_StdioFile::finish: stream already closed");
    }
}
