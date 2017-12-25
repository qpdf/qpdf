#ifndef __QPDF_STREAM_HH__
#define __QPDF_STREAM_HH__

#include <qpdf/Types.h>

#include <qpdf/QPDFObject.hh>
#include <qpdf/QPDFObjectHandle.hh>

class Pipeline;
class QPDF;

class QPDF_Stream: public QPDFObject
{
  public:
    QPDF_Stream(QPDF*, int objid, int generation,
		QPDFObjectHandle stream_dict,
		qpdf_offset_t offset, size_t length);
    virtual ~QPDF_Stream();
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    QPDFObjectHandle getDict() const;

    // See comments in QPDFObjectHandle.hh for these methods.
    bool pipeStreamData(Pipeline*,
                        unsigned long encode_flags,
                        qpdf_stream_decode_level_e decode_level,
                        bool suppress_warnings, bool will_retry);
    PointerHolder<Buffer> getStreamData(qpdf_stream_decode_level_e);
    PointerHolder<Buffer> getRawStreamData();
    void replaceStreamData(PointerHolder<Buffer> data,
			   QPDFObjectHandle const& filter,
			   QPDFObjectHandle const& decode_parms);
    void replaceStreamData(
	PointerHolder<QPDFObjectHandle::StreamDataProvider> provider,
	QPDFObjectHandle const& filter,
	QPDFObjectHandle const& decode_parms);

    void replaceDict(QPDFObjectHandle new_dict);

    // Replace object ID and generation.  This may only be called if
    // object ID and generation are 0.  It is used by QPDFObjectHandle
    // when adding streams to files.
    void setObjGen(int objid, int generation);

  protected:
    virtual void releaseResolved();

  private:
    static std::map<std::string, std::string> filter_abbreviations;

    void replaceFilterData(QPDFObjectHandle const& filter,
			   QPDFObjectHandle const& decode_parms,
			   size_t length);
    bool understandDecodeParams(
        std::string const& filter, QPDFObjectHandle decode_params,
        int& predictor, int& columns,
        int& colors, int& bits_per_component,
        bool& early_code_change);
    bool filterable(std::vector<std::string>& filters,
                    bool& specialized_compression, bool& lossy_compression,
		    int& predictor, int& columns,
                    int& colors, int& bits_per_component,
                    bool& early_code_change);
    void warn(QPDFExc const& e);

    QPDF* qpdf;
    int objid;
    int generation;
    QPDFObjectHandle stream_dict;
    qpdf_offset_t offset;
    size_t length;
    PointerHolder<Buffer> stream_data;
    PointerHolder<QPDFObjectHandle::StreamDataProvider> stream_provider;
};

#endif // __QPDF_STREAM_HH__
