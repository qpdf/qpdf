#ifndef QPDF_OFFSETINPUTSOURCE_HH
#define QPDF_OFFSETINPUTSOURCE_HH

// This class implements an InputSource that proxies for an underlying
// input source but offset a specific number of bytes.

#include <qpdf/InputSource.hh>
#include <qpdf/PointerHolder.hh>

class OffsetInputSource: public InputSource
{
  public:
    OffsetInputSource(
        std::shared_ptr<InputSource>, qpdf_offset_t global_offset);
    virtual ~OffsetInputSource();

    virtual qpdf_offset_t findAndSkipNextEOL();
    virtual std::string const& getName() const;
    virtual qpdf_offset_t tell();
    virtual void seek(qpdf_offset_t offset, int whence);
    virtual void rewind();
    virtual size_t read(char* buffer, size_t length);
    virtual void unreadCh(char ch);

  private:
    std::shared_ptr<InputSource> proxied;
    qpdf_offset_t global_offset;
    qpdf_offset_t max_safe_offset;
};

#endif // QPDF_OFFSETINPUTSOURCE_HH
