#include <qpdf/Pl_Flate.hh>

#include <qpdf/QUtil.hh>

Pl_Flate::Pl_Flate(char const* identifier, Pipeline* next,
		   action_e action, int out_bufsize) :
    Pipeline(identifier, next),
    out_bufsize(out_bufsize),
    action(action),
    initialized(false)
{
    this->outbuf = new unsigned char[out_bufsize];

    zstream.zalloc = (alloc_func)0;
    zstream.zfree = (free_func)0;
    zstream.opaque = (voidpf)0;
    zstream.next_in = 0;
    zstream.avail_in = 0;
    zstream.next_out = this->outbuf;
    zstream.avail_out = out_bufsize;
}

Pl_Flate::~Pl_Flate()
{
    if (this->outbuf)
    {
	delete [] this->outbuf;
	this->outbuf = 0;
    }
}

void
Pl_Flate::write(unsigned char* data, int len)
{
    if (this->outbuf == 0)
    {
	throw std::logic_error(
	    this->identifier +
	    ": Pl_Flate: write() called after finish() called");
    }
    handleData(data, len, Z_NO_FLUSH);
}

void
Pl_Flate::handleData(unsigned char* data, int len, int flush)
{
    this->zstream.next_in = data;
    this->zstream.avail_in = len;

    if (! this->initialized)
    {
	int err = Z_OK;
	if (this->action == a_deflate)
	{
	    err = deflateInit(&this->zstream, Z_DEFAULT_COMPRESSION);
	}
	else
	{
	    err = inflateInit(&this->zstream);
	}
	checkError("Init", err);
	this->initialized = true;
    }

    int err = Z_OK;

    bool done = false;
    while (! done)
    {
	if (action == a_deflate)
	{
	    err = deflate(&this->zstream, flush);
	}
	else
	{
	    err = inflate(&this->zstream, flush);
	}
	switch (err)
	{
	  case Z_BUF_ERROR:
	    // Probably shouldn't be able to happen, but possible as a
	    // boundary condition: if the last call to inflate exactly
	    // filled the output buffer, it's possible that the next
	    // call to inflate could have nothing to do.
	    done = true;
	    break;

	  case Z_STREAM_END:
	    done = true;
	    // fall through

	  case Z_OK:
	    {
		if ((this->zstream.avail_in == 0) &&
		    (this->zstream.avail_out > 0))
		{
		    // There is nothing left to read, and there was
		    // sufficient buffer space to write everything we
		    // needed, so we're done for now.
		    done = true;
		}
		uLong ready = (this->out_bufsize - this->zstream.avail_out);
		if (ready > 0)
		{
		    this->getNext()->write(this->outbuf, ready);
		    this->zstream.next_out = this->outbuf;
		    this->zstream.avail_out = this->out_bufsize;
		}
	    }
	    break;

	  default:
	    this->checkError("data", err);
	    break;
	}
    }
}

void
Pl_Flate::finish()
{
    if (this->outbuf)
    {
	if (this->initialized)
	{
	    unsigned char buf[1];
	    buf[0] = '\0';
	    handleData(buf, 0, Z_FINISH);
	    int err = Z_OK;
	    if (action == a_deflate)
	    {
		err = deflateEnd(&this->zstream);
	    }
	    else
	    {
		err = inflateEnd(&this->zstream);
	    }
	    checkError("End", err);
	}

	delete [] this->outbuf;
	this->outbuf = 0;
    }
    this->getNext()->finish();
}

void
Pl_Flate::checkError(char const* prefix, int error_code)
{
    if (error_code != Z_OK)
    {
	char const* action_str = (action == a_deflate ? "deflate" : "inflate");
	std::string msg =
	    this->identifier + ": " + action_str + ": " + prefix + ": ";

	if (this->zstream.msg)
	{
	    msg += this->zstream.msg;
	}
	else
	{
	    switch (error_code)
	    {
	      case Z_ERRNO:
		msg += "zlib system error";
		break;

	      case Z_STREAM_ERROR:
		msg += "zlib stream error";
		break;

	      case Z_DATA_ERROR:
		msg += "zlib data error";
		break;

	      case Z_MEM_ERROR:
		msg += "zlib memory error";
		break;

	      case Z_BUF_ERROR:
		msg += "zlib buffer error";
		break;

	      case Z_VERSION_ERROR:
		msg += "zlib version error";
		break;

	      default:
		msg += std::string("zlib unknown error (") +
		    QUtil::int_to_string(error_code) + ")";
		break;
	    }
	}

	throw std::runtime_error(msg);
    }
}
