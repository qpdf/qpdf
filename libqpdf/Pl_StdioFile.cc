#include <qpdf/qpdf-config.h>  // include first for large file support
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QUtil.hh>
#include <stdexcept>
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
Pl_StdioFile::write(unsigned char* buf, size_t len)
{
    size_t so_far = 0;
    while (len > 0)
    {
	so_far = fwrite(buf, 1, len, this->file);
	if (so_far == 0)
	{
	    QUtil::throw_system_error(
		this->identifier + ": Pl_StdioFile::write");
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
    if ((fflush(this->file) == -1) &&
        (errno == EBADF))
    {
	throw std::logic_error(
	    this->identifier +
	    ": Pl_StdioFile::finish: stream already closed");
    }
}
