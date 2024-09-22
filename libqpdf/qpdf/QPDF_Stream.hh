#ifndef QPDF_STREAM_HH
#define QPDF_STREAM_HH

#include <qpdf/Types.h>

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFStreamFilter.hh>
#include <qpdf/QPDFValue.hh>

#include <functional>
#include <memory>

class Pipeline;
class QPDF;

class QPDF_Stream final: public QPDFValue
{
  public:
    ~QPDF_Stream() final = default;
    static std::shared_ptr<QPDFObject>
    create(QPDF*, QPDFObjGen og, QPDFObjectHandle stream_dict, qpdf_offset_t offset, size_t length);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) final;
    std::string unparse() final;
    void writeJSON(int json_version, JSON::Writer& p) final;
    void setDescription(
        QPDF*, std::shared_ptr<QPDFValue::Description>& description, qpdf_offset_t offset) final;
    void disconnect() final;

  private:
    friend class qpdf::Stream;

    QPDF_Stream(
        QPDF*, QPDFObjGen og, QPDFObjectHandle stream_dict, qpdf_offset_t offset, size_t length);

    void replaceFilterData(
        QPDFObjectHandle const& filter, QPDFObjectHandle const& decode_parms, size_t length);
    void setDictDescription();

    bool filter_on_write;
    QPDFObjectHandle stream_dict;
    size_t length;
    std::shared_ptr<Buffer> stream_data;
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> stream_provider;
    std::vector<std::shared_ptr<QPDFObjectHandle::TokenFilter>> token_filters;
};

#endif // QPDF_STREAM_HH
