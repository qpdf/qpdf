
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

    // See comments in QPDFObjectHandle.hh
    bool pipeStreamData(Pipeline*, bool filter,
			bool normalize, bool compress);

    // See comments in QPDFObjectHandle.hh
    PointerHolder<Buffer> getStreamData();

  private:
    bool filterable(std::vector<std::string>& filters,
		    int& predictor, int& columns, bool& early_code_change);


    QPDF* qpdf;
    int objid;
    int generation;
    QPDFObjectHandle stream_dict;
    off_t offset;
    int length;
};

#endif // __QPDF_STREAM_HH__
