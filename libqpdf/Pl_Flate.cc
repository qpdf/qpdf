#include <qpdf/Pl_Flate.hh>
#include <zlib.h>

#include <qpdf/QUtil.hh>

Pl_Flate::Pl_Flate(char const* identifier, Pipeline* next,
		   action_e action, int out_bufsize) :
    Pipeline(identifier, next),
    out_bufsize(out_bufsize),
    action(action),
    initialized(false)
{
    this->outbuf = new unsigned char[out_bufsize];
    // Indirect through zdata to reach the z_stream so we don't have
    // to include zlib.h in Pl_Flate.hh.  This means people using
    // shared library versions of qpdf don't have to have zlib
    // development files available, which particularly helps in a
    // Windows environment.
    this->zdata = new z_stream;

    z_stream& zstream = *(static_cast<z_stream*>(this->zdata));
    zstream.zalloc = 0;
    zstream.zfree = 0;
    zstream.opaque = 0;
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
    delete static_cast<z_stream*>(this->zdata);
    this->zdata = 0;
}

void
Pl_Flate::write(unsigned char* data, size_t len)
{
    if (this->outbuf == 0)
    {
	throw std::logic_error(
	    this->identifier +
	    ": Pl_Flate: write() called after finish() called");
    }

    // Write in chunks in case len is too big to fit in an int.
    // Assume int is at least 32 bits.
    static size_t const max_bytes = 1 << 30;
    size_t bytes_left = len;
    unsigned char* buf = data;
    while (bytes_left > 0)
    {
	size_t bytes = (bytes_left >= max_bytes ? max_bytes : bytes_left);
        handleData(buf, bytes, Z_NO_FLUSH);
	bytes_left -= bytes;
        buf += bytes;
    }
}

void
Pl_Flate::handleData(unsigned char* data, int len, int flush)
{
    z_stream& zstream = *(static_cast<z_stream*>(this->zdata));
    zstream.next_in = data;
    zstream.avail_in = len;

    if (! this->initialized)
    {
	int err = Z_OK;

        // deflateInit and inflateInit are macros that use old-style
        // casts.
#ifdef __GNUC__
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#       pragma GCC diagnostic push
#       pragma GCC diagnostic ignored "-Wold-style-cast"
# endif
#endif
	if (this->action == a_deflate)
	{
	    err = deflateInit(&zstream, Z_DEFAULT_COMPRESSION);
	}
	else
	{
	    err = inflateInit(&zstream);
	}
#ifdef __GNUC__
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#       pragma GCC diagnostic pop
# endif
#endif

	checkError("Init", err);
	this->initialized = true;
    }

    int err = Z_OK;

    bool done = false;
    while (! done)
    {
	if (action == a_deflate)
	{
	    err = deflate(&zstream, flush);
	}
	else
	{
	    err = inflate(&zstream, flush);
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
		if ((zstream.avail_in == 0) &&
		    (zstream.avail_out > 0))
		{
		    // There is nothing left to read, and there was
		    // sufficient buffer space to write everything we
		    // needed, so we're done for now.
		    done = true;
		}
		uLong ready = (this->out_bufsize - zstream.avail_out);
		if (ready > 0)
		{
		    this->getNext()->write(this->outbuf, ready);
		    zstream.next_out = this->outbuf;
		    zstream.avail_out = this->out_bufsize;
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
	    z_stream& zstream = *(static_cast<z_stream*>(this->zdata));
	    unsigned char buf[1];
	    buf[0] = '\0';
	    handleData(buf, 0, Z_FINISH);
	    int err = Z_OK;
	    if (action == a_deflate)
	    {
		err = deflateEnd(&zstream);
	    }
	    else
	    {
		err = inflateEnd(&zstream);
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
    z_stream& zstream = *(static_cast<z_stream*>(this->zdata));
    if (error_code != Z_OK)
    {
	char const* action_str = (action == a_deflate ? "deflate" : "inflate");
	std::string msg =
	    this->identifier + ": " + action_str + ": " + prefix + ": ";

	if (zstream.msg)
	{
	    msg += zstream.msg;
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
