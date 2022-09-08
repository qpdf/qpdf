#ifndef QPDF_STREAM_HH
#define QPDF_STREAM_HH

#include <qpdf/Types.h>

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFStreamFilter.hh>
#include <qpdf/QPDFValue.hh>

#include <functional>
#include <memory>

class Pipeline;
class QPDF;

class QPDF_Stream: public QPDFValue
{
  public:
    virtual ~QPDF_Stream() = default;
    static std::shared_ptr<QPDFValueProxy> create(
        QPDF*,
        QPDFObjGen const& og,
        QPDFObjectHandle stream_dict,
        qpdf_offset_t offset,
        size_t length);
    virtual std::shared_ptr<QPDFValueProxy> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual void setDescription(QPDF*, std::string const&);
    virtual void disconnect();
    QPDFObjectHandle getDict() const;
    bool isDataModified() const;
    void setFilterOnWrite(bool);
    bool getFilterOnWrite() const;

    // Methods to help QPDF copy foreign streams
    qpdf_offset_t getOffset() const;
    size_t getLength() const;
    std::shared_ptr<Buffer> getStreamDataBuffer() const;
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider>
    getStreamDataProvider() const;

    // See comments in QPDFObjectHandle.hh for these methods.
    bool pipeStreamData(
        Pipeline*,
        bool* tried_filtering,
        int encode_flags,
        qpdf_stream_decode_level_e decode_level,
        bool suppress_warnings,
        bool will_retry);
    std::shared_ptr<Buffer> getStreamData(qpdf_stream_decode_level_e);
    std::shared_ptr<Buffer> getRawStreamData();
    void replaceStreamData(
        std::shared_ptr<Buffer> data,
        QPDFObjectHandle const& filter,
        QPDFObjectHandle const& decode_parms);
    void replaceStreamData(
        std::shared_ptr<QPDFObjectHandle::StreamDataProvider> provider,
        QPDFObjectHandle const& filter,
        QPDFObjectHandle const& decode_parms);
    void
    addTokenFilter(std::shared_ptr<QPDFObjectHandle::TokenFilter> token_filter);
    JSON getStreamJSON(
        int json_version,
        qpdf_json_stream_data_e json_data,
        qpdf_stream_decode_level_e decode_level,
        Pipeline* p,
        std::string const& data_filename);

    void replaceDict(QPDFObjectHandle const& new_dict);

    static void registerStreamFilter(
        std::string const& filter_name,
        std::function<std::shared_ptr<QPDFStreamFilter>()> factory);

    // Replace object ID and generation.  This may only be called if
    // object ID and generation are 0.  It is used by QPDFObjectHandle
    // when adding streams to files.
    void setObjGen(QPDFObjGen const& og);

  private:
    QPDF_Stream(
        QPDF*,
        QPDFObjGen const& og,
        QPDFObjectHandle stream_dict,
        qpdf_offset_t offset,
        size_t length);
    static std::map<std::string, std::string> filter_abbreviations;
    static std::
        map<std::string, std::function<std::shared_ptr<QPDFStreamFilter>()>>
            filter_factories;

    void replaceFilterData(
        QPDFObjectHandle const& filter,
        QPDFObjectHandle const& decode_parms,
        size_t length);
    bool filterable(
        std::vector<std::shared_ptr<QPDFStreamFilter>>& filters,
        bool& specialized_compression,
        bool& lossy_compression);
    void warn(
        qpdf_error_code_e error_code,
        qpdf_offset_t offset,
        std::string const& message);
    void setDictDescription();

    QPDF* qpdf;
    QPDFObjGen og;
    bool filter_on_write;
    QPDFObjectHandle stream_dict;
    qpdf_offset_t offset;
    size_t length;
    std::shared_ptr<Buffer> stream_data;
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> stream_provider;
    std::vector<std::shared_ptr<QPDFObjectHandle::TokenFilter>> token_filters;
};

#endif // QPDF_STREAM_HH
