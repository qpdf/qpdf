#include <qpdf/QPDF_Stream.hh>

#include <qpdf/ContentNormalizer.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/SF_ASCII85Decode.hh>
#include <qpdf/SF_ASCIIHexDecode.hh>
#include <qpdf/SF_DCTDecode.hh>
#include <qpdf/SF_FlateLzwDecode.hh>
#include <qpdf/SF_RunLengthDecode.hh>

#include <stdexcept>

namespace
{
    class SF_Crypt: public QPDFStreamFilter
    {
      public:
        SF_Crypt() = default;
        virtual ~SF_Crypt() = default;

        virtual bool
        setDecodeParms(QPDFObjectHandle decode_parms)
        {
            if (decode_parms.isNull()) {
                return true;
            }
            bool filterable = true;
            for (auto const& key: decode_parms.getKeys()) {
                if (((key == "/Type") || (key == "/Name")) &&
                    ((!decode_parms.hasKey("/Type")) ||
                     decode_parms.isDictionaryOfType(
                         "/CryptFilterDecodeParms"))) {
                    // we handle this in decryptStream
                } else {
                    filterable = false;
                }
            }
            return filterable;
        }

        virtual Pipeline*
        getDecodePipeline(Pipeline*)
        {
            // Not used -- handled by pipeStreamData
            return nullptr;
        }
    };
} // namespace

std::map<std::string, std::string> QPDF_Stream::filter_abbreviations = {
    // The PDF specification provides these filter abbreviations for
    // use in inline images, but according to table H.1 in the pre-ISO
    // versions of the PDF specification, Adobe Reader also accepts
    // them for stream filters.
    {"/AHx", "/ASCIIHexDecode"},
    {"/A85", "/ASCII85Decode"},
    {"/LZW", "/LZWDecode"},
    {"/Fl", "/FlateDecode"},
    {"/RL", "/RunLengthDecode"},
    {"/CCF", "/CCITTFaxDecode"},
    {"/DCT", "/DCTDecode"},
};

std::map<std::string, std::function<std::shared_ptr<QPDFStreamFilter>()>>
    QPDF_Stream::filter_factories = {
        {"/Crypt", []() { return std::make_shared<SF_Crypt>(); }},
        {"/FlateDecode", SF_FlateLzwDecode::flate_factory},
        {"/LZWDecode", SF_FlateLzwDecode::lzw_factory},
        {"/RunLengthDecode", SF_RunLengthDecode::factory},
        {"/DCTDecode", SF_DCTDecode::factory},
        {"/ASCII85Decode", SF_ASCII85Decode::factory},
        {"/ASCIIHexDecode", SF_ASCIIHexDecode::factory},
};

QPDF_Stream::QPDF_Stream(
    QPDF* qpdf,
    int objid,
    int generation,
    QPDFObjectHandle stream_dict,
    qpdf_offset_t offset,
    size_t length) :
    qpdf(qpdf),
    objid(objid),
    generation(generation),
    filter_on_write(true),
    stream_dict(stream_dict),
    offset(offset),
    length(length)
{
    if (!stream_dict.isDictionary()) {
        throw std::logic_error("stream object instantiated with non-dictionary "
                               "object for dictionary");
    }
    setStreamDescription();
}

void
QPDF_Stream::registerStreamFilter(
    std::string const& filter_name,
    std::function<std::shared_ptr<QPDFStreamFilter>()> factory)
{
    filter_factories[filter_name] = factory;
}

void
QPDF_Stream::setFilterOnWrite(bool val)
{
    this->filter_on_write = val;
}

bool
QPDF_Stream::getFilterOnWrite() const
{
    return this->filter_on_write;
}

void
QPDF_Stream::releaseResolved()
{
    this->stream_provider = 0;
    QPDFObjectHandle::ReleaseResolver::releaseResolved(this->stream_dict);
}

void
QPDF_Stream::setObjGen(int objid, int generation)
{
    if (!((this->objid == 0) && (this->generation == 0))) {
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

JSON
QPDF_Stream::getJSON()
{
    return this->stream_dict.getJSON();
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

void
QPDF_Stream::setDescription(QPDF* qpdf, std::string const& description)
{
    this->QPDFObject::setDescription(qpdf, description);
    setDictDescription();
}

void
QPDF_Stream::setStreamDescription()
{
    setDescription(
        this->qpdf,
        this->qpdf->getFilename() + ", stream object " +
            QUtil::int_to_string(this->objid) + " " +
            QUtil::int_to_string(this->generation));
}

void
QPDF_Stream::setDictDescription()
{
    QPDF* qpdf = 0;
    std::string description;
    if ((!this->stream_dict.hasObjectDescription()) &&
        getDescription(qpdf, description)) {
        this->stream_dict.setObjectDescription(
            qpdf, description + " -> stream dictionary");
    }
}

QPDFObjectHandle
QPDF_Stream::getDict() const
{
    return this->stream_dict;
}

bool
QPDF_Stream::isDataModified() const
{
    return (!this->token_filters.empty());
}

qpdf_offset_t
QPDF_Stream::getOffset() const
{
    return this->offset;
}

size_t
QPDF_Stream::getLength() const
{
    return this->length;
}

std::shared_ptr<Buffer>
QPDF_Stream::getStreamDataBuffer() const
{
    return this->stream_data;
}

std::shared_ptr<QPDFObjectHandle::StreamDataProvider>
QPDF_Stream::getStreamDataProvider() const
{
    return this->stream_provider;
}

std::shared_ptr<Buffer>
QPDF_Stream::getStreamData(qpdf_stream_decode_level_e decode_level)
{
    Pl_Buffer buf("stream data buffer");
    bool filtered;
    pipeStreamData(&buf, &filtered, 0, decode_level, false, false);
    if (!filtered) {
        throw QPDFExc(
            qpdf_e_unsupported,
            qpdf->getFilename(),
            "",
            this->offset,
            "getStreamData called on unfilterable stream");
    }
    QTC::TC("qpdf", "QPDF_Stream getStreamData");
    return buf.getBufferSharedPointer();
}

std::shared_ptr<Buffer>
QPDF_Stream::getRawStreamData()
{
    Pl_Buffer buf("stream data buffer");
    if (!pipeStreamData(&buf, nullptr, 0, qpdf_dl_none, false, false)) {
        throw QPDFExc(
            qpdf_e_unsupported,
            qpdf->getFilename(),
            "",
            this->offset,
            "error getting raw stream data");
    }
    QTC::TC("qpdf", "QPDF_Stream getRawStreamData");
    return buf.getBufferSharedPointer();
}

bool
QPDF_Stream::filterable(
    std::vector<std::shared_ptr<QPDFStreamFilter>>& filters,
    bool& specialized_compression,
    bool& lossy_compression)
{
    // Check filters

    QPDFObjectHandle filter_obj = this->stream_dict.getKey("/Filter");
    bool filters_okay = true;

    std::vector<std::string> filter_names;

    if (filter_obj.isNull()) {
        // No filters
    } else if (filter_obj.isName()) {
        // One filter
        filter_names.push_back(filter_obj.getName());
    } else if (filter_obj.isArray()) {
        // Potentially multiple filters
        int n = filter_obj.getArrayNItems();
        for (int i = 0; i < n; ++i) {
            QPDFObjectHandle item = filter_obj.getArrayItem(i);
            if (item.isName()) {
                filter_names.push_back(item.getName());
            } else {
                filters_okay = false;
            }
        }
    } else {
        filters_okay = false;
    }

    if (!filters_okay) {
        QTC::TC("qpdf", "QPDF_Stream invalid filter");
        warn(
            qpdf_e_damaged_pdf,
            this->offset,
            "stream filter type is not name or array");
        return false;
    }

    bool filterable = true;

    for (auto& filter_name: filter_names) {
        if (filter_abbreviations.count(filter_name)) {
            QTC::TC("qpdf", "QPDF_Stream expand filter abbreviation");
            filter_name = filter_abbreviations[filter_name];
        }

        auto ff = filter_factories.find(filter_name);
        if (ff == filter_factories.end()) {
            filterable = false;
        } else {
            filters.push_back((ff->second)());
        }
    }

    if (!filterable) {
        return false;
    }

    // filters now contains a list of filters to be applied in order.
    // See which ones we can support.

    // See if we can support any decode parameters that are specified.

    QPDFObjectHandle decode_obj = this->stream_dict.getKey("/DecodeParms");
    std::vector<QPDFObjectHandle> decode_parms;
    if (decode_obj.isArray() && (decode_obj.getArrayNItems() == 0)) {
        decode_obj = QPDFObjectHandle::newNull();
    }
    if (decode_obj.isArray()) {
        for (int i = 0; i < decode_obj.getArrayNItems(); ++i) {
            decode_parms.push_back(decode_obj.getArrayItem(i));
        }
    } else {
        for (unsigned int i = 0; i < filter_names.size(); ++i) {
            decode_parms.push_back(decode_obj);
        }
    }

    // Ignore /DecodeParms entirely if /Filters is empty.  At least
    // one case of a file whose /DecodeParms was [ << >> ] when
    // /Filters was empty has been seen in the wild.
    if ((filters.size() != 0) && (decode_parms.size() != filters.size())) {
        warn(
            qpdf_e_damaged_pdf,
            this->offset,
            "stream /DecodeParms length is"
            " inconsistent with filters");
        filterable = false;
    }

    if (!filterable) {
        return false;
    }

    for (size_t i = 0; i < filters.size(); ++i) {
        auto filter = filters.at(i);
        auto decode_item = decode_parms.at(i);

        if (filter->setDecodeParms(decode_item)) {
            if (filter->isSpecializedCompression()) {
                specialized_compression = true;
            }
            if (filter->isLossyCompression()) {
                specialized_compression = true;
                lossy_compression = true;
            }
        } else {
            filterable = false;
        }
    }

    return filterable;
}

bool
QPDF_Stream::pipeStreamData(
    Pipeline* pipeline,
    bool* filterp,
    int encode_flags,
    qpdf_stream_decode_level_e decode_level,
    bool suppress_warnings,
    bool will_retry)
{
    std::vector<std::shared_ptr<QPDFStreamFilter>> filters;
    bool specialized_compression = false;
    bool lossy_compression = false;
    bool ignored;
    if (filterp == nullptr) {
        filterp = &ignored;
    }
    bool& filter = *filterp;
    filter = (!((encode_flags == 0) && (decode_level == qpdf_dl_none)));
    bool success = true;
    if (filter) {
        filter =
            filterable(filters, specialized_compression, lossy_compression);
        if ((decode_level < qpdf_dl_all) && lossy_compression) {
            filter = false;
        }
        if ((decode_level < qpdf_dl_specialized) && specialized_compression) {
            filter = false;
        }
        QTC::TC(
            "qpdf",
            "QPDF_Stream special filters",
            (!filter)                     ? 0
                : lossy_compression       ? 1
                : specialized_compression ? 2
                                          : 3);
    }

    if (pipeline == 0) {
        QTC::TC("qpdf", "QPDF_Stream pipeStreamData with null pipeline");
        // Return value is whether we can filter in this case.
        return filter;
    }

    // Construct the pipeline in reverse order. Force pipelines we
    // create to be deleted when this function finishes. Pipelines
    // created by QPDFStreamFilter objects will be deleted by those
    // objects.
    std::vector<std::shared_ptr<Pipeline>> to_delete;

    std::shared_ptr<ContentNormalizer> normalizer;
    std::shared_ptr<Pipeline> new_pipeline;
    if (filter) {
        if (encode_flags & qpdf_ef_compress) {
            new_pipeline = std::make_shared<Pl_Flate>(
                "compress stream", pipeline, Pl_Flate::a_deflate);
            to_delete.push_back(new_pipeline);
            pipeline = new_pipeline.get();
        }

        if (encode_flags & qpdf_ef_normalize) {
            normalizer = std::make_shared<ContentNormalizer>();
            new_pipeline = std::make_shared<Pl_QPDFTokenizer>(
                "normalizer", normalizer.get(), pipeline);
            to_delete.push_back(new_pipeline);
            pipeline = new_pipeline.get();
        }

        for (auto iter = this->token_filters.rbegin();
             iter != this->token_filters.rend();
             ++iter) {
            new_pipeline = std::make_shared<Pl_QPDFTokenizer>(
                "token filter", (*iter).get(), pipeline);
            to_delete.push_back(new_pipeline);
            pipeline = new_pipeline.get();
        }

        for (auto f_iter = filters.rbegin(); f_iter != filters.rend();
             ++f_iter) {
            auto decode_pipeline = (*f_iter)->getDecodePipeline(pipeline);
            if (decode_pipeline) {
                pipeline = decode_pipeline;
            }
            Pl_Flate* flate = dynamic_cast<Pl_Flate*>(pipeline);
            if (flate != nullptr) {
                flate->setWarnCallback([this](char const* msg, int code) {
                    warn(qpdf_e_damaged_pdf, this->offset, msg);
                });
            }
        }
    }

    if (this->stream_data.get()) {
        QTC::TC("qpdf", "QPDF_Stream pipe replaced stream data");
        pipeline->write(
            this->stream_data->getBuffer(), this->stream_data->getSize());
        pipeline->finish();
    } else if (this->stream_provider.get()) {
        Pl_Count count("stream provider count", pipeline);
        if (this->stream_provider->supportsRetry()) {
            if (!this->stream_provider->provideStreamData(
                    this->objid,
                    this->generation,
                    &count,
                    suppress_warnings,
                    will_retry)) {
                filter = false;
                success = false;
            }
        } else {
            this->stream_provider->provideStreamData(
                this->objid, this->generation, &count);
        }
        qpdf_offset_t actual_length = count.getCount();
        qpdf_offset_t desired_length = 0;
        if (success && this->stream_dict.hasKey("/Length")) {
            desired_length = this->stream_dict.getKey("/Length").getIntValue();
            if (actual_length == desired_length) {
                QTC::TC("qpdf", "QPDF_Stream pipe use stream provider");
            } else {
                QTC::TC("qpdf", "QPDF_Stream provider length mismatch");
                // This would be caused by programmer error on the
                // part of a library user, not by invalid input data.
                throw std::runtime_error(
                    "stream data provider for " +
                    QUtil::int_to_string(this->objid) + " " +
                    QUtil::int_to_string(this->generation) + " provided " +
                    QUtil::int_to_string(actual_length) +
                    " bytes instead of expected " +
                    QUtil::int_to_string(desired_length) + " bytes");
            }
        } else if (success) {
            QTC::TC("qpdf", "QPDF_Stream provider length not provided");
            this->stream_dict.replaceKey(
                "/Length", QPDFObjectHandle::newInteger(actual_length));
        }
    } else if (this->offset == 0) {
        QTC::TC("qpdf", "QPDF_Stream pipe no stream data");
        throw std::logic_error("pipeStreamData called for stream with no data");
    } else {
        QTC::TC("qpdf", "QPDF_Stream pipe original stream data");
        if (!QPDF::Pipe::pipeStreamData(
                this->qpdf,
                this->objid,
                this->generation,
                this->offset,
                this->length,
                this->stream_dict,
                pipeline,
                suppress_warnings,
                will_retry)) {
            filter = false;
            success = false;
        }
    }

    if (filter && (!suppress_warnings) && normalizer.get() &&
        normalizer->anyBadTokens()) {
        warn(
            qpdf_e_damaged_pdf,
            this->offset,
            "content normalization encountered bad tokens");
        if (normalizer->lastTokenWasBad()) {
            QTC::TC("qpdf", "QPDF_Stream bad token at end during normalize");
            warn(
                qpdf_e_damaged_pdf,
                this->offset,
                "normalized content ended with a bad token;"
                " you may be able to resolve this by"
                " coalescing content streams in combination"
                " with normalizing content. From the command"
                " line, specify --coalesce-contents");
        }
        warn(
            qpdf_e_damaged_pdf,
            this->offset,
            "Resulting stream data may be corrupted but is"
            " may still useful for manual inspection."
            " For more information on this warning, search"
            " for content normalization in the manual.");
    }

    return success;
}

void
QPDF_Stream::replaceStreamData(
    std::shared_ptr<Buffer> data,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    this->stream_data = data;
    this->stream_provider = 0;
    replaceFilterData(filter, decode_parms, data->getSize());
}

void
QPDF_Stream::replaceStreamData(
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> provider,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    this->stream_provider = provider;
    this->stream_data = 0;
    replaceFilterData(filter, decode_parms, 0);
}

void
QPDF_Stream::addTokenFilter(
    std::shared_ptr<QPDFObjectHandle::TokenFilter> token_filter)
{
    this->token_filters.push_back(token_filter);
}

void
QPDF_Stream::replaceFilterData(
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms,
    size_t length)
{
    this->stream_dict.replaceKey("/Filter", filter)
        .replaceKey("/DecodeParms", decode_parms);
    if (length == 0) {
        QTC::TC("qpdf", "QPDF_Stream unknown stream length");
        this->stream_dict.removeKey("/Length");
    } else {
        this->stream_dict.replaceKey(
            "/Length",
            QPDFObjectHandle::newInteger(QIntC::to_longlong(length)));
    }
}

void
QPDF_Stream::replaceDict(QPDFObjectHandle const& new_dict)
{
    this->stream_dict = new_dict;
    setDictDescription();
    QPDFObjectHandle length_obj = this->stream_dict.getKey("/Length");
    if (length_obj.isInteger()) {
        this->length = QIntC::to_size(length_obj.getUIntValue());
    } else {
        this->length = 0;
    }
}

void
QPDF_Stream::warn(
    qpdf_error_code_e error_code,
    qpdf_offset_t offset,
    std::string const& message)
{
    this->qpdf->warn(error_code, "", offset, message);
}
