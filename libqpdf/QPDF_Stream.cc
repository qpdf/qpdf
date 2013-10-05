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
#include <qpdf/Pl_Count.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>

#include <stdexcept>

std::map<std::string, std::string> QPDF_Stream::filter_abbreviations;

QPDF_Stream::QPDF_Stream(QPDF* qpdf, int objid, int generation,
			 QPDFObjectHandle stream_dict,
			 qpdf_offset_t offset, size_t length) :
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

void
QPDF_Stream::setObjGen(int objid, int generation)
{
    if (! ((this->objid == 0) && (this->generation == 0)))
    {
	throw std::logic_error(
	    "attempt to set object ID and generation of a stream"
	    " that already has them");
    }
    this->objid = objid;
    this->generation = generation;
}

std::string
QPDF_Stream::unparse()
{
    // Unparse stream objects as indirect references
    return QUtil::int_to_string(this->objid) + " " +
	QUtil::int_to_string(this->generation) + " R";
}

QPDFObject::object_type_e
QPDF_Stream::getTypeCode() const
{
    return QPDFObject::ot_stream;
}

char const*
QPDF_Stream::getTypeName() const
{
    return "stream";
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
    QTC::TC("qpdf", "QPDF_Stream getStreamData");
    return buf.getBuffer();
}

PointerHolder<Buffer>
QPDF_Stream::getRawStreamData()
{
    Pl_Buffer buf("stream data buffer");
    pipeStreamData(&buf, false, false, false);
    QTC::TC("qpdf", "QPDF_Stream getRawStreamData");
    return buf.getBuffer();
}

bool
QPDF_Stream::understandDecodeParams(
    std::string const& filter, QPDFObjectHandle decode_obj,
    int& predictor, int& columns, bool& early_code_change)
{
    bool filterable = true;
    std::set<std::string> keys = decode_obj.getKeys();
    for (std::set<std::string>::iterator iter = keys.begin();
         iter != keys.end(); ++iter)
    {
        std::string const& key = *iter;
        if ((filter == "/FlateDecode") && (key == "/Predictor"))
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
        else if ((filter == "/LZWDecode") && (key == "/EarlyChange"))
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
        else if ((filter == "/Crypt") &&
                 (((key == "/Type") || (key == "/Name")) &&
                  (decode_obj.getKey("/Type").isNull() ||
                   (decode_obj.getKey("/Type").isName() &&
                    (decode_obj.getKey("/Type").getName() ==
                     "/CryptFilterDecodeParms")))))
        {
            // we handle this in decryptStream
        }
        else
        {
            filterable = false;
        }
    }

    return filterable;
}

bool
QPDF_Stream::filterable(std::vector<std::string>& filters,
			int& predictor, int& columns,
			bool& early_code_change)
{
    if (filter_abbreviations.empty())
    {
	// The PDF specification provides these filter abbreviations
	// for use in inline images, but according to table H.1 in the
	// pre-ISO versions of the PDF specification, Adobe Reader
	// also accepts them for stream filters.
	filter_abbreviations["/AHx"] = "/ASCIIHexDecode";
	filter_abbreviations["/A85"] = "/ASCII85Decode";
	filter_abbreviations["/LZW"] = "/LZWDecode";
	filter_abbreviations["/Fl"] = "/FlateDecode";
	filter_abbreviations["/RL"] = "/RunLengthDecode";
	filter_abbreviations["/CCF"] = "/CCITTFaxDecode";
	filter_abbreviations["/DCT"] = "/DCTDecode";
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

    bool filterable = true;

    for (std::vector<std::string>::iterator iter = filters.begin();
	 iter != filters.end(); ++iter)
    {
	std::string& filter = *iter;

	if (filter_abbreviations.count(filter))
	{
	    QTC::TC("qpdf", "QPDF_Stream expand filter abbreviation");
	    filter = filter_abbreviations[filter];
	}

	if (! ((filter == "/Crypt") ||
	       (filter == "/FlateDecode") ||
	       (filter == "/LZWDecode") ||
	       (filter == "/ASCII85Decode") ||
	       (filter == "/ASCIIHexDecode")))
	{
	    filterable = false;
	}
    }

    if (! filterable)
    {
        return false;
    }

    // `filters' now contains a list of filters to be applied in
    // order.  See which ones we can support.

    // Initialize values to their defaults as per the PDF spec
    predictor = 1;
    columns = 0;
    early_code_change = true;

    // See if we can support any decode parameters that are specified.

    QPDFObjectHandle decode_obj = this->stream_dict.getKey("/DecodeParms");
    std::vector<QPDFObjectHandle> decode_parms;
    if (decode_obj.isArray())
    {
        for (int i = 0; i < decode_obj.getArrayNItems(); ++i)
        {
            decode_parms.push_back(decode_obj.getArrayItem(i));
        }
    }
    else
    {
        for (unsigned int i = 0; i < filters.size(); ++i)
        {
            decode_parms.push_back(decode_obj);
        }
    }

    // Ignore /DecodeParms entirely if /Filters is empty.  At least
    // one case of a file whose /DecodeParms was [ << >> ] when
    // /Filters was empty has been seen in the wild.
    if ((filters.size() != 0) && (decode_parms.size() != filters.size()))
    {
        // We should just issue a warning and treat this as not
        // filterable.
	throw QPDFExc(qpdf_e_damaged_pdf, qpdf->getFilename(),
		      "", this->offset,
		      "stream /DecodeParms length is"
                      " inconsistent with filters");
    }

    for (unsigned int i = 0; i < filters.size(); ++i)
    {
        QPDFObjectHandle decode_item = decode_parms.at(i);
        if (decode_item.isNull())
        {
            // okay
        }
        else if (decode_item.isDictionary())
        {
            if (! understandDecodeParams(
                    filters.at(i), decode_item,
                    predictor, columns, early_code_change))
            {
                filterable = false;
            }
        }
        else
        {
            filterable = false;
        }
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
	pipeline->write(this->stream_data->getBuffer(),
			this->stream_data->getSize());
	pipeline->finish();
    }
    else if (this->stream_provider.getPointer())
    {
	Pl_Count count("stream provider count", pipeline);
	this->stream_provider->provideStreamData(
	    this->objid, this->generation, &count);
	qpdf_offset_t actual_length = count.getCount();
	qpdf_offset_t desired_length = 0;
        if (this->stream_dict.hasKey("/Length"))
        {
	    desired_length = this->stream_dict.getKey("/Length").getIntValue();
            if (actual_length == desired_length)
            {
                QTC::TC("qpdf", "QPDF_Stream pipe use stream provider");
            }
            else
            {
                QTC::TC("qpdf", "QPDF_Stream provider length mismatch");
                throw std::logic_error(
                    "stream data provider for " +
                    QUtil::int_to_string(this->objid) + " " +
                    QUtil::int_to_string(this->generation) +
                    " provided " +
                    QUtil::int_to_string(actual_length) +
                    " bytes instead of expected " +
                    QUtil::int_to_string(desired_length) + " bytes");
            }
        }
        else
        {
            QTC::TC("qpdf", "QPDF_Stream provider length not provided");
            this->stream_dict.replaceKey(
                "/Length", QPDFObjectHandle::newInteger(actual_length));
        }
    }
    else if (this->offset == 0)
    {
	QTC::TC("qpdf", "QPDF_Stream pipe no stream data");
	throw std::logic_error(
	    "pipeStreamData called for stream with no data");
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
			       QPDFObjectHandle const& filter,
			       QPDFObjectHandle const& decode_parms)
{
    this->stream_data = data;
    this->stream_provider = 0;
    replaceFilterData(filter, decode_parms, data->getSize());
}

void
QPDF_Stream::replaceStreamData(
    PointerHolder<QPDFObjectHandle::StreamDataProvider> provider,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    this->stream_provider = provider;
    this->stream_data = 0;
    replaceFilterData(filter, decode_parms, 0);
}

void
QPDF_Stream::replaceFilterData(QPDFObjectHandle const& filter,
			       QPDFObjectHandle const& decode_parms,
			       size_t length)
{
    this->stream_dict.replaceOrRemoveKey("/Filter", filter);
    this->stream_dict.replaceOrRemoveKey("/DecodeParms", decode_parms);
    if (length == 0)
    {
        QTC::TC("qpdf", "QPDF_Stream unknown stream length");
        this->stream_dict.removeKey("/Length");
    }
    else
    {
        this->stream_dict.replaceKey(
            "/Length", QPDFObjectHandle::newInteger(length));
    }
}

void
QPDF_Stream::replaceDict(QPDFObjectHandle new_dict)
{
    this->stream_dict = new_dict;
    QPDFObjectHandle length_obj = new_dict.getKey("/Length");
    if (length_obj.isInteger())
    {
        this->length = length_obj.getIntValue();
    }
    else
    {
        this->length = 0;
    }
}
