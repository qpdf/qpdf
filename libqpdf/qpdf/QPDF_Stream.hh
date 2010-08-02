#ifndef __QPDF_STREAM_HH__
#define __QPDF_STREAM_HH__

#include <qpdf/QPDFObject.hh>

#include <qpdf/QPDFObjectHandle.hh>

class Pipeline;
class QPDF;

class QPDF_Stream: public QPDFObject
{
  public:
    QPDF_Stream(QPDF*, int objid, int generation,
		QPDFObjectHandle stream_dict,
		off_t offset, int length);
    virtual ~QPDF_Stream();
    virtual std::string unparse();
    QPDFObjectHandle getDict() const;

    // See comments in QPDFObjectHandle.hh for these methods.
    bool pipeStreamData(Pipeline*, bool filter,
			bool normalize, bool compress);
    PointerHolder<Buffer> getStreamData();
    void replaceStreamData(PointerHolder<Buffer> data,
			   QPDFObjectHandle filter,
			   QPDFObjectHandle decode_parms);

  private:
    bool filterable(std::vector<std::string>& filters,
		    int& predictor, int& columns, bool& early_code_change);

    QPDF* qpdf;
    int objid;
    int generation;
    QPDFObjectHandle stream_dict;
    off_t offset;
    int length;
    PointerHolder<Buffer> stream_data;
};

#endif // __QPDF_STREAM_HH__
