#ifndef __QPDF_INPUTSOURCE_HH__
#define __QPDF_INPUTSOURCE_HH__

#include <qpdf/Types.h>
#include <stdio.h>
#include <string>

class InputSource
{
  public:
    InputSource() :
        last_offset(0)
    {
    }
    virtual ~InputSource()
    {
    }

    void setLastOffset(qpdf_offset_t);
    qpdf_offset_t getLastOffset() const;
    std::string readLine(size_t max_line_length);

    virtual qpdf_offset_t findAndSkipNextEOL() = 0;
    virtual std::string const& getName() const = 0;
    virtual qpdf_offset_t tell() = 0;
    virtual void seek(qpdf_offset_t offset, int whence) = 0;
    virtual void rewind() = 0;
    virtual size_t read(char* buffer, size_t length) = 0;
    virtual void unreadCh(char ch) = 0;

  protected:
    qpdf_offset_t last_offset;
};

#endif // __QPDF_INPUTSOURCE_HH__
