
#ifndef __PL_PNGFILTER_HH__
#define __PL_PNGFILTER_HH__

// This pipeline applies or reverses the application of a PNG filter
// as described in the PNG specification.

// NOTE: In its initial implementation, it only encodes and decodes
// filters "none" and "up".  The primary motivation of this code is to
// encode and decode PDF 1.5+ XRef streams which are often encoded
// with Flate predictor 12, which corresponds to the PNG up filter.
// At present, the bytes_per_pixel parameter is ignored, and an
// exception is thrown if any row of the file has a filter of other
// than 0 or 2.  Finishing the implementation would not be difficult.
// See chapter 6 of the PNG specification for a description of the
// filter algorithms.

#include <qpdf/Pipeline.hh>

class Pl_PNGFilter: public Pipeline
{
  public:
    class Exception: public Pipeline::Exception
    {
      public:
	Exception(std::string const& message) :
	    Pipeline::Exception(message)
	{
	}

	virtual ~Exception() throw ()
	{
	}
    };

    // Encoding is not presently supported
    enum action_e { a_encode, a_decode };

    Pl_PNGFilter(char const* identifier, Pipeline* next,
		 action_e action, unsigned int columns,
		 unsigned int bytes_per_pixel);
    virtual ~Pl_PNGFilter();

    virtual void write(unsigned char* data, int len);
    virtual void finish();

  private:
    void processRow();
    void encodeRow();
    void decodeRow();

    action_e action;
    unsigned int columns;
    unsigned char* cur_row;
    unsigned char* prev_row;
    unsigned char* buf1;
    unsigned char* buf2;
    int pos;
    int incoming;
};

#endif // __PL_PNGFILTER_HH__
