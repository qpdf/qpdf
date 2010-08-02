#include <qpdf/QPDF_Stream.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_RC4.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_ASCII85Decoder.hh>
#include <qpdf/Pl_ASCIIHexDecoder.hh>
#include <qpdf/Pl_LZWDecoder.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>

#include <stdexcept>

QPDF_Stream::QPDF_Stream(QPDF* qpdf, int objid, int generation,
			 QPDFObjectHandle stream_dict,
			 off_t offset, int length) :
    qpdf(qpdf),
    objid(objid),
    generation(generation),
    stream_dict(stream_dict),
    offset(offset),
    length(length)
{
    if (! stream_dict.isDictionary())
    {
	throw std::logic_error(
	    "stream object instantiated with non-dictionary "
	    "object for dictionary");
    }
}

QPDF_Stream::~QPDF_Stream()
{
}

std::string
QPDF_Stream::unparse()
{
    // Unparse stream objects as indirect references
    return QUtil::int_to_string(this->objid) + " " +
	QUtil::int_to_string(this->generation) + " R";
}

QPDFObjectHandle
QPDF_Stream::getDict() const
{
    return this->stream_dict;
}

PointerHolder<Buffer>
QPDF_Stream::getStreamData()
{
    Pl_Buffer buf("stream data buffer");
    if (! pipeStreamData(&buf, true, false, false))
    {
	throw std::logic_error("getStreamData called on unfilterable stream");
    }
    return buf.getBuffer();
}

bool
QPDF_Stream::filterable(std::vector<std::string>& filters,
			int& predictor, int& columns,
			bool& early_code_change)
{
    // Initialize values to their defaults as per the PDF spec
    predictor = 1;
    columns = 0;
    early_code_change = true;

    bool filterable = true;

    // See if we can support any decode parameters that are specified.

    QPDFObjectHandle decode_obj =
	this->stream_dict.getKey("/DecodeParms");
    if (decode_obj.isNull())
    {
	// no problem
    }
    else if (decode_obj.isDictionary())
    {
	std::set<std::string> keys = decode_obj.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    std::string const& key = *iter;
	    if (key == "/Predictor")
	    {
		QPDFObjectHandle predictor_obj = decode_obj.getKey(key);
		if (predictor_obj.isInteger())
		{
		    predictor = predictor_obj.getIntValue();
		    if (! ((predictor == 1) || (predictor == 12)))
		    {
			filterable = false;
		    }
		}
		else
		{
		    filterable = false;
		}
	    }
	    else if (key == "/EarlyChange")
	    {
		QPDFObjectHandle earlychange_obj = decode_obj.getKey(key);
		if (earlychange_obj.isInteger())
		{
		    int earlychange = earlychange_obj.getIntValue();
		    early_code_change = (earlychange == 1);
		    if (! ((earlychange == 0) || (earlychange == 1)))
		    {
			filterable = false;
		    }
		}
		else
		{
		    filterable = false;
		}
	    }
	    else if (key == "/Columns")
	    {
		QPDFObjectHandle columns_obj = decode_obj.getKey(key);
		if (columns_obj.isInteger())
		{
		    columns = columns_obj.getIntValue();
		}
		else
		{
		    filterable = false;
		}
	    }
	    else if (((key == "/Type") || (key == "/Name")) &&
		     decode_obj.getKey("/Type").isName() &&
		     (decode_obj.getKey("/Type").getName() ==
		      "/CryptFilterDecodeParms"))
	    {
		// we handle this in decryptStream
	    }
	    else
	    {
		filterable = false;
	    }
	}
    }
    else
    {
	// Ignore for now -- some filter types, like CCITTFaxDecode,
	// use types other than dictionary for this.
	QTC::TC("qpdf", "QPDF_Stream ignore non-dictionary DecodeParms");

	filterable = false;
    }

    if ((predictor > 1) && (columns == 0))
    {
	// invalid
	filterable = false;
    }

    if (! filterable)
    {
	return false;
    }

    // Check filters

    QPDFObjectHandle filter_obj = this->stream_dict.getKey("/Filter");
    bool filters_okay = true;

    if (filter_obj.isNull())
    {
	// No filters
    }
    else if (filter_obj.isName())
    {
	// One filter
	filters.push_back(filter_obj.getName());
    }
    else if (filter_obj.isArray())
    {
	// Potentially multiple filters
	int n = filter_obj.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    QPDFObjectHandle item = filter_obj.getArrayItem(i);
	    if (item.isName())
	    {
		filters.push_back(item.getName());
	    }
	    else
	    {
		filters_okay = false;
	    }
	}
    }
    else
    {
	filters_okay = false;
    }

    if (! filters_okay)
    {
	QTC::TC("qpdf", "QPDF_Stream invalid filter");
	throw QPDFExc(qpdf_e_damaged_pdf, qpdf->getFilename(),
		      "", this->offset,
		      "stream filter type is not name or array");
    }

    // `filters' now contains a list of filters to be applied in
    // order.  See which ones we can support.

    for (std::vector<std::string>::iterator iter = filters.begin();
	 iter != filters.end(); ++iter)
    {
	std::string const& filter = *iter;
	if (! ((filter == "/Crypt") ||
	       (filter == "/FlateDecode") ||
	       (filter == "/LZWDecode") ||
	       (filter == "/ASCII85Decode") ||
	       (filter == "/ASCIIHexDecode")))
	{
	    filterable = false;
	}
    }

    return filterable;
}

bool
QPDF_Stream::pipeStreamData(Pipeline* pipeline, bool filter,
			    bool normalize, bool compress)
{
    std::vector<std::string> filters;
    int predictor = 1;
    int columns = 0;
    bool early_code_change = true;
    if (filter)
    {
	filter = filterable(filters, predictor, columns, early_code_change);
    }

    if (pipeline == 0)
    {
	QTC::TC("qpdf", "QPDF_Stream pipeStreamData with null pipeline");
	return filter;
    }

    // Construct the pipeline in reverse order.  Force pipelines we
    // create to be deleted when this function finishes.
    std::vector<PointerHolder<Pipeline> > to_delete;

    if (filter)
    {
	if (compress)
	{
	    pipeline = new Pl_Flate("compress object stream", pipeline,
				    Pl_Flate::a_deflate);
	    to_delete.push_back(pipeline);
	}

	if (normalize)
	{
	    pipeline = new Pl_QPDFTokenizer("normalizer", pipeline);
	    to_delete.push_back(pipeline);
	}

	for (std::vector<std::string>::reverse_iterator iter = filters.rbegin();
	     iter != filters.rend(); ++iter)
	{
	    std::string const& filter = *iter;
	    if (filter == "/Crypt")
	    {
		// Ignore -- handled by pipeStreamData
	    }
	    else if (filter == "/FlateDecode")
	    {
		if (predictor == 12)
		{
		    QTC::TC("qpdf", "QPDF_Stream PNG filter");
		    pipeline = new Pl_PNGFilter(
			"png decode", pipeline, Pl_PNGFilter::a_decode,
			columns, 0 /* not used */);
		    to_delete.push_back(pipeline);
		}

		pipeline = new Pl_Flate("stream inflate",
					pipeline, Pl_Flate::a_inflate);
		to_delete.push_back(pipeline);
	    }
	    else if (filter == "/ASCII85Decode")
	    {
		pipeline = new Pl_ASCII85Decoder("ascii85 decode", pipeline);
		to_delete.push_back(pipeline);
	    }
	    else if (filter == "/ASCIIHexDecode")
	    {
		pipeline = new Pl_ASCIIHexDecoder("asciiHex decode", pipeline);
		to_delete.push_back(pipeline);
	    }
	    else if (filter == "/LZWDecode")
	    {
		pipeline = new Pl_LZWDecoder("lzw decode", pipeline,
					     early_code_change);
		to_delete.push_back(pipeline);
	    }
	    else
	    {
		throw std::logic_error(
		    "INTERNAL ERROR: QPDFStream: unknown filter "
		    "encountered after check");
	    }
	}
    }

    if (this->stream_data.getPointer())
    {
	QTC::TC("qpdf", "QPDF_Stream pipe replaced stream data");
	Buffer& b = *(this->stream_data.getPointer());
	pipeline->write(b.getBuffer(), b.getSize());
	pipeline->finish();
    }
    else
    {
	QTC::TC("qpdf", "QPDF_Stream pipe original stream data");
	QPDF::Pipe::pipeStreamData(this->qpdf, this->objid, this->generation,
				   this->offset, this->length,
				   this->stream_dict, pipeline);
    }

    return filter;
}

void
QPDF_Stream::replaceStreamData(PointerHolder<Buffer> data,
			       QPDFObjectHandle filter,
			       QPDFObjectHandle decode_parms)
{
    this->stream_data = data;
    this->stream_dict.replaceOrRemoveKey("/Filter", filter);
    this->stream_dict.replaceOrRemoveKey("/DecodeParms", decode_parms);
    this->stream_dict.replaceKey("/Length",
				 QPDFObjectHandle::newInteger(
				     data.getPointer()->getSize()));
}
