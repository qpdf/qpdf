#include <qpdf/QPDFObjectHandle_private.hh>

#include <qpdf/ContentNormalizer.hh>
#include <qpdf/JSON_writer.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Base64.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDF_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/SF_ASCII85Decode.hh>
#include <qpdf/SF_ASCIIHexDecode.hh>
#include <qpdf/SF_DCTDecode.hh>
#include <qpdf/SF_FlateLzwDecode.hh>
#include <qpdf/SF_RunLengthDecode.hh>

#include <stdexcept>

using namespace std::literals;
using namespace qpdf;

namespace
{
    class SF_Crypt: public QPDFStreamFilter
    {
      public:
        SF_Crypt() = default;
        ~SF_Crypt() override = default;

        bool
        setDecodeParms(QPDFObjectHandle decode_parms) override
        {
            if (decode_parms.isNull()) {
                return true;
            }
            bool filterable = true;
            for (auto const& key: decode_parms.getKeys()) {
                if (((key == "/Type") || (key == "/Name")) &&
                    ((!decode_parms.hasKey("/Type")) ||
                     decode_parms.isDictionaryOfType("/CryptFilterDecodeParms"))) {
                    // we handle this in decryptStream
                } else {
                    filterable = false;
                }
            }
            return filterable;
        }

        Pipeline*
        getDecodePipeline(Pipeline*) override
        {
            // Not used -- handled by pipeStreamData
            return nullptr;
        }
    };

    class StreamBlobProvider
    {
      public:
        StreamBlobProvider(Stream stream, qpdf_stream_decode_level_e decode_level) :
            stream(stream),
            decode_level(decode_level)
        {
        }
        void
        operator()(Pipeline* p)
        {
            stream.pipeStreamData(p, nullptr, 0, decode_level, false, false);
        }

      private:
        Stream stream;
        qpdf_stream_decode_level_e decode_level;
    };
} // namespace

std::map<std::string, std::string> Stream::filter_abbreviations = {
    // The PDF specification provides these filter abbreviations for use in inline images, but
    // according to table H.1 in the pre-ISO versions of the PDF specification, Adobe Reader also
    // accepts them for stream filters.
    {"/AHx", "/ASCIIHexDecode"},
    {"/A85", "/ASCII85Decode"},
    {"/LZW", "/LZWDecode"},
    {"/Fl", "/FlateDecode"},
    {"/RL", "/RunLengthDecode"},
    {"/CCF", "/CCITTFaxDecode"},
    {"/DCT", "/DCTDecode"},
};

std::map<std::string, std::function<std::shared_ptr<QPDFStreamFilter>()>> Stream::filter_factories =
    {
        {"/Crypt", []() { return std::make_shared<SF_Crypt>(); }},
        {"/FlateDecode", SF_FlateLzwDecode::flate_factory},
        {"/LZWDecode", SF_FlateLzwDecode::lzw_factory},
        {"/RunLengthDecode", SF_RunLengthDecode::factory},
        {"/DCTDecode", SF_DCTDecode::factory},
        {"/ASCII85Decode", SF_ASCII85Decode::factory},
        {"/ASCIIHexDecode", SF_ASCIIHexDecode::factory},
};

Stream::Stream(
    QPDF& qpdf, QPDFObjGen og, QPDFObjectHandle stream_dict, qpdf_offset_t offset, size_t length) :
    BaseHandle(QPDFObject::create<QPDF_Stream>(&qpdf, og, std::move(stream_dict), length))
{
    auto descr = std::make_shared<QPDFObject::Description>(
        qpdf.getFilename() + ", stream object " + og.unparse(' '));
    obj->setDescription(&qpdf, descr, offset);
    setDictDescription();
}

void
Stream::registerStreamFilter(
    std::string const& filter_name, std::function<std::shared_ptr<QPDFStreamFilter>()> factory)
{
    filter_factories[filter_name] = factory;
}

JSON
Stream::getStreamJSON(
    int json_version,
    qpdf_json_stream_data_e json_data,
    qpdf_stream_decode_level_e decode_level,
    Pipeline* p,
    std::string const& data_filename)
{
    Pl_Buffer pb{"streamjson"};
    JSON::Writer jw{&pb, 0};
    decode_level =
        writeStreamJSON(json_version, jw, json_data, decode_level, p, data_filename, true);
    pb.finish();
    auto result = JSON::parse(pb.getString());
    if (json_data == qpdf_sj_inline) {
        result.addDictionaryMember("data", JSON::makeBlob(StreamBlobProvider(*this, decode_level)));
    }
    return result;
}

qpdf_stream_decode_level_e
Stream::writeStreamJSON(
    int json_version,
    JSON::Writer& jw,
    qpdf_json_stream_data_e json_data,
    qpdf_stream_decode_level_e decode_level,
    Pipeline* p,
    std::string const& data_filename,
    bool no_data_key)
{
    auto s = stream();
    switch (json_data) {
    case qpdf_sj_none:
    case qpdf_sj_inline:
        if (p != nullptr) {
            throw std::logic_error(
                "QPDF_Stream::writeStreamJSON: pipeline should only be supplied "
                "when json_data is file");
        }
        break;
    case qpdf_sj_file:
        if (p == nullptr) {
            throw std::logic_error(
                "QPDF_Stream::writeStreamJSON: pipeline must be supplied when json_data is file");
        }
        if (data_filename.empty()) {
            throw std::logic_error(
                "QPDF_Stream::writeStreamJSON: data_filename must be supplied "
                "when json_data is file");
        }
        break;
    }

    jw.writeStart('{');

    if (json_data == qpdf_sj_none) {
        jw.writeNext();
        jw << R"("dict": )";
        s->stream_dict.writeJSON(json_version, jw);
        jw.writeEnd('}');
        return decode_level;
    }

    Pl_Discard discard;
    Pl_Buffer buf_pl{"stream data"};
    Pipeline* data_pipeline = &buf_pl;
    if (no_data_key && json_data == qpdf_sj_inline) {
        data_pipeline = &discard;
    }
    // pipeStreamData produced valid data.
    bool buf_pl_ready = false;
    bool filtered = false;
    bool filter = (decode_level != qpdf_dl_none);
    for (int attempt = 1; attempt <= 2; ++attempt) {
        bool succeeded =
            pipeStreamData(data_pipeline, &filtered, 0, decode_level, false, (attempt == 1));
        if (!succeeded || (filter && !filtered)) {
            // Try again
            filter = false;
            decode_level = qpdf_dl_none;
            buf_pl.getString(); // reset buf_pl
        } else {
            buf_pl_ready = true;
            break;
        }
    }
    if (!buf_pl_ready) {
        throw std::logic_error("QPDF_Stream: failed to get stream data");
    }
    // We can use unsafeShallowCopy because we are only touching top-level keys.
    auto dict = s->stream_dict.unsafeShallowCopy();
    dict.removeKey("/Length");
    if (filter && filtered) {
        dict.removeKey("/Filter");
        dict.removeKey("/DecodeParms");
    }
    if (json_data == qpdf_sj_file) {
        jw.writeNext() << R"("datafile": ")" << JSON::Writer::encode_string(data_filename) << "\"";
        p->writeString(buf_pl.getString());
    } else if (json_data == qpdf_sj_inline) {
        if (!no_data_key) {
            jw.writeNext() << R"("data": ")";
            jw.writeBase64(buf_pl.getString()) << "\"";
        }
    } else {
        throw std::logic_error("QPDF_Stream::writeStreamJSON : unexpected value of json_data");
    }

    jw.writeNext() << R"("dict": )";
    dict.writeJSON(json_version, jw);
    jw.writeEnd('}');

    return decode_level;
}

void
qpdf::Stream::setDictDescription()
{
    auto s = stream();
    if (!s->stream_dict.hasObjectDescription()) {
        s->stream_dict.setObjectDescription(
            obj->getQPDF(), obj->getDescription() + " -> stream dictionary");
    }
}

std::shared_ptr<Buffer>
Stream::getStreamData(qpdf_stream_decode_level_e decode_level)
{
    Pl_Buffer buf("stream data buffer");
    bool filtered;
    pipeStreamData(&buf, &filtered, 0, decode_level, false, false);
    if (!filtered) {
        throw QPDFExc(
            qpdf_e_unsupported,
            obj->getQPDF()->getFilename(),
            "",
            obj->getParsedOffset(),
            "getStreamData called on unfilterable stream");
    }
    QTC::TC("qpdf", "QPDF_Stream getStreamData");
    return buf.getBufferSharedPointer();
}

std::shared_ptr<Buffer>
Stream::getRawStreamData()
{
    Pl_Buffer buf("stream data buffer");
    if (!pipeStreamData(&buf, nullptr, 0, qpdf_dl_none, false, false)) {
        throw QPDFExc(
            qpdf_e_unsupported,
            obj->getQPDF()->getFilename(),
            "",
            obj->getParsedOffset(),
            "error getting raw stream data");
    }
    QTC::TC("qpdf", "QPDF_Stream getRawStreamData");
    return buf.getBufferSharedPointer();
}

bool
Stream::isRootMetadata() const
{
    if (!getDict().isDictionaryOfType("/Metadata", "/XML")) {
        return false;
    }
    auto root_metadata = qpdf()->getRoot().getKey("/Metadata");
    return root_metadata.isStream() && root_metadata.getObjGen() == obj->getObjGen();
}

bool
Stream::filterable(
    std::vector<std::shared_ptr<QPDFStreamFilter>>& filters,
    bool& specialized_compression,
    bool& lossy_compression)
{
    auto s = stream();
    // Check filters

    QPDFObjectHandle filter_obj = s->stream_dict.getKey("/Filter");
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
        warn("stream filter type is not name or array");
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

    // filters now contains a list of filters to be applied in order. See which ones we can support.

    // See if we can support any decode parameters that are specified.

    QPDFObjectHandle decode_obj = s->stream_dict.getKey("/DecodeParms");
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

    // Ignore /DecodeParms entirely if /Filters is empty.  At least one case of a file whose
    // /DecodeParms was [ << >> ] when /Filters was empty has been seen in the wild.
    if ((filters.size() != 0) && (decode_parms.size() != filters.size())) {
        warn("stream /DecodeParms length is inconsistent with filters");
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
Stream::pipeStreamData(
    Pipeline* pipeline,
    bool* filterp,
    int encode_flags,
    qpdf_stream_decode_level_e decode_level,
    bool suppress_warnings,
    bool will_retry)
{
    auto s = stream();
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
        filter = filterable(filters, specialized_compression, lossy_compression);
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

    if (pipeline == nullptr) {
        QTC::TC("qpdf", "QPDF_Stream pipeStreamData with null pipeline");
        // Return value is whether we can filter in this case.
        return filter;
    }

    // Construct the pipeline in reverse order. Force pipelines we create to be deleted when this
    // function finishes. Pipelines created by QPDFStreamFilter objects will be deleted by those
    // objects.
    std::vector<std::shared_ptr<Pipeline>> to_delete;

    std::shared_ptr<ContentNormalizer> normalizer;
    std::shared_ptr<Pipeline> new_pipeline;
    if (filter) {
        if (encode_flags & qpdf_ef_compress) {
            new_pipeline =
                std::make_shared<Pl_Flate>("compress stream", pipeline, Pl_Flate::a_deflate);
            to_delete.push_back(new_pipeline);
            pipeline = new_pipeline.get();
        }

        if (encode_flags & qpdf_ef_normalize) {
            normalizer = std::make_shared<ContentNormalizer>();
            new_pipeline =
                std::make_shared<Pl_QPDFTokenizer>("normalizer", normalizer.get(), pipeline);
            to_delete.push_back(new_pipeline);
            pipeline = new_pipeline.get();
        }

        for (auto iter = s->token_filters.rbegin(); iter != s->token_filters.rend(); ++iter) {
            new_pipeline =
                std::make_shared<Pl_QPDFTokenizer>("token filter", (*iter).get(), pipeline);
            to_delete.push_back(new_pipeline);
            pipeline = new_pipeline.get();
        }

        for (auto f_iter = filters.rbegin(); f_iter != filters.rend(); ++f_iter) {
            auto decode_pipeline = (*f_iter)->getDecodePipeline(pipeline);
            if (decode_pipeline) {
                pipeline = decode_pipeline;
            }
            auto* flate = dynamic_cast<Pl_Flate*>(pipeline);
            if (flate != nullptr) {
                flate->setWarnCallback([this](char const* msg, int code) { warn(msg); });
            }
        }
    }

    if (s->stream_data.get()) {
        QTC::TC("qpdf", "QPDF_Stream pipe replaced stream data");
        pipeline->write(s->stream_data->getBuffer(), s->stream_data->getSize());
        pipeline->finish();
    } else if (s->stream_provider.get()) {
        Pl_Count count("stream provider count", pipeline);
        if (s->stream_provider->supportsRetry()) {
            if (!s->stream_provider->provideStreamData(
                    obj->getObjGen(), &count, suppress_warnings, will_retry)) {
                filter = false;
                success = false;
            }
        } else {
            s->stream_provider->provideStreamData(obj->getObjGen(), &count);
        }
        qpdf_offset_t actual_length = count.getCount();
        qpdf_offset_t desired_length = 0;
        if (success && s->stream_dict.hasKey("/Length")) {
            desired_length = s->stream_dict.getKey("/Length").getIntValue();
            if (actual_length == desired_length) {
                QTC::TC("qpdf", "QPDF_Stream pipe use stream provider");
            } else {
                QTC::TC("qpdf", "QPDF_Stream provider length mismatch");
                // This would be caused by programmer error on the part of a library user, not by
                // invalid input data.
                throw std::runtime_error(
                    "stream data provider for " + obj->getObjGen().unparse(' ') + " provided " +
                    std::to_string(actual_length) + " bytes instead of expected " +
                    std::to_string(desired_length) + " bytes");
            }
        } else if (success) {
            QTC::TC("qpdf", "QPDF_Stream provider length not provided");
            s->stream_dict.replaceKey("/Length", QPDFObjectHandle::newInteger(actual_length));
        }
    } else if (obj->getParsedOffset() == 0) {
        QTC::TC("qpdf", "QPDF_Stream pipe no stream data");
        throw std::logic_error("pipeStreamData called for stream with no data");
    } else {
        QTC::TC("qpdf", "QPDF_Stream pipe original stream data");
        if (!QPDF::Pipe::pipeStreamData(
                obj->getQPDF(),
                obj->getObjGen(),
                obj->getParsedOffset(),
                s->length,
                s->stream_dict,
                isRootMetadata(),
                pipeline,
                suppress_warnings,
                will_retry)) {
            filter = false;
            success = false;
        }
    }

    if (filter && (!suppress_warnings) && normalizer.get() && normalizer->anyBadTokens()) {
        warn("content normalization encountered bad tokens");
        if (normalizer->lastTokenWasBad()) {
            QTC::TC("qpdf", "QPDF_Stream bad token at end during normalize");
            warn(
                "normalized content ended with a bad token; you may be able to resolve this by "
                "coalescing content streams in combination with normalizing content. From the "
                "command line, specify --coalesce-contents");
        }
        warn(
            "Resulting stream data may be corrupted but is may still useful for manual "
            "inspection. For more information on this warning, search for content normalization "
            "in the manual.");
    }

    return success;
}

void
Stream::replaceStreamData(
    std::shared_ptr<Buffer> data,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    auto s = stream();
    s->stream_data = data;
    s->stream_provider = nullptr;
    replaceFilterData(filter, decode_parms, data->getSize());
}

void
Stream::replaceStreamData(
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> provider,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    auto s = stream();
    s->stream_provider = provider;
    s->stream_data = nullptr;
    replaceFilterData(filter, decode_parms, 0);
}

void
Stream::replaceFilterData(
    QPDFObjectHandle const& filter, QPDFObjectHandle const& decode_parms, size_t length)
{
    auto s = stream();
    if (filter) {
        s->stream_dict.replaceKey("/Filter", filter);
    }
    if (decode_parms) {
        s->stream_dict.replaceKey("/DecodeParms", decode_parms);
    }
    if (length == 0) {
        QTC::TC("qpdf", "QPDF_Stream unknown stream length");
        s->stream_dict.removeKey("/Length");
    } else {
        s->stream_dict.replaceKey(
            "/Length", QPDFObjectHandle::newInteger(QIntC::to_longlong(length)));
    }
}

void
Stream::warn(std::string const& message)
{
    obj->getQPDF()->warn(qpdf_e_damaged_pdf, "", obj->getParsedOffset(), message);
}

QPDFObjectHandle
QPDFObjectHandle::getDict() const
{
    return as_stream(error).getDict();
}

void
QPDFObjectHandle::setFilterOnWrite(bool val)
{
    as_stream(error).setFilterOnWrite(val);
}

bool
QPDFObjectHandle::getFilterOnWrite()
{
    return as_stream(error).getFilterOnWrite();
}

bool
QPDFObjectHandle::isDataModified()
{
    return as_stream(error).isDataModified();
}

void
QPDFObjectHandle::replaceDict(QPDFObjectHandle const& new_dict)
{
    as_stream(error).replaceDict(new_dict);
}

std::shared_ptr<Buffer>
QPDFObjectHandle::getStreamData(qpdf_stream_decode_level_e level)
{
    return as_stream(error).getStreamData(level);
}

std::shared_ptr<Buffer>
QPDFObjectHandle::getRawStreamData()
{
    return as_stream(error).getRawStreamData();
}

bool
QPDFObjectHandle::pipeStreamData(
    Pipeline* p,
    bool* filtering_attempted,
    int encode_flags,
    qpdf_stream_decode_level_e decode_level,
    bool suppress_warnings,
    bool will_retry)
{
    return as_stream(error).pipeStreamData(
        p, filtering_attempted, encode_flags, decode_level, suppress_warnings, will_retry);
}

bool
QPDFObjectHandle::pipeStreamData(
    Pipeline* p,
    int encode_flags,
    qpdf_stream_decode_level_e decode_level,
    bool suppress_warnings,
    bool will_retry)
{
    bool filtering_attempted;
    as_stream(error).pipeStreamData(
        p, &filtering_attempted, encode_flags, decode_level, suppress_warnings, will_retry);
    return filtering_attempted;
}

bool
QPDFObjectHandle::pipeStreamData(Pipeline* p, bool filter, bool normalize, bool compress)
{
    int encode_flags = 0;
    qpdf_stream_decode_level_e decode_level = qpdf_dl_none;
    if (filter) {
        decode_level = qpdf_dl_generalized;
        if (normalize) {
            encode_flags |= qpdf_ef_normalize;
        }
        if (compress) {
            encode_flags |= qpdf_ef_compress;
        }
    }
    return pipeStreamData(p, encode_flags, decode_level, false);
}

void
QPDFObjectHandle::replaceStreamData(
    std::shared_ptr<Buffer> data,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    as_stream(error).replaceStreamData(data, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(
    std::string const& data, QPDFObjectHandle const& filter, QPDFObjectHandle const& decode_parms)
{
    auto b = std::make_shared<Buffer>(data.length());
    unsigned char* bp = b->getBuffer();
    if (bp) {
        memcpy(bp, data.c_str(), data.length());
    }
    as_stream(error).replaceStreamData(b, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(
    std::shared_ptr<StreamDataProvider> provider,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    as_stream(error).replaceStreamData(provider, filter, decode_parms);
}

namespace
{
    class FunctionProvider: public QPDFObjectHandle::StreamDataProvider
    {
      public:
        FunctionProvider(std::function<void(Pipeline*)> provider) :
            StreamDataProvider(false),
            p1(provider),
            p2(nullptr)
        {
        }
        FunctionProvider(std::function<bool(Pipeline*, bool, bool)> provider) :
            StreamDataProvider(true),
            p1(nullptr),
            p2(provider)
        {
        }

        void
        provideStreamData(QPDFObjGen const&, Pipeline* pipeline) override
        {
            p1(pipeline);
        }

        bool
        provideStreamData(
            QPDFObjGen const&, Pipeline* pipeline, bool suppress_warnings, bool will_retry) override
        {
            return p2(pipeline, suppress_warnings, will_retry);
        }

      private:
        std::function<void(Pipeline*)> p1;
        std::function<bool(Pipeline*, bool, bool)> p2;
    };
} // namespace

void
QPDFObjectHandle::replaceStreamData(
    std::function<void(Pipeline*)> provider,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    auto sdp = std::shared_ptr<StreamDataProvider>(new FunctionProvider(provider));
    as_stream(error).replaceStreamData(sdp, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(
    std::function<bool(Pipeline*, bool, bool)> provider,
    QPDFObjectHandle const& filter,
    QPDFObjectHandle const& decode_parms)
{
    auto sdp = std::shared_ptr<StreamDataProvider>(new FunctionProvider(provider));
    as_stream(error).replaceStreamData(sdp, filter, decode_parms);
}

JSON
QPDFObjectHandle::getStreamJSON(
    int json_version,
    qpdf_json_stream_data_e json_data,
    qpdf_stream_decode_level_e decode_level,
    Pipeline* p,
    std::string const& data_filename)
{
    return as_stream(error).getStreamJSON(json_version, json_data, decode_level, p, data_filename);
}
