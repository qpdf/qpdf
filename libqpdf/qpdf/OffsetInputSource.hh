#ifndef QPDF_OFFSETINPUTSOURCE_HH
#define QPDF_OFFSETINPUTSOURCE_HH

// This class implements an InputSource that proxies for an underlying
// input source but offset a specific number of bytes.

#include <qpdf/InputSource.hh>

class OffsetInputSource: public InputSource
{
  public:
    OffsetInputSource(std::shared_ptr<InputSource>, qpdf_offset_t global_offset);
    ~OffsetInputSource() override = default;

    qpdf_offset_t findAndSkipNextEOL() override;
    std::string const& getName() const override;
    qpdf_offset_t tell() override;
    void seek(qpdf_offset_t offset, int whence) override;
    void rewind() override;
    size_t read(char* buffer, size_t length) override;
    void unreadCh(char ch) override;

  private:
    std::shared_ptr<InputSource> proxied;
    qpdf_offset_t global_offset;
    qpdf_offset_t max_safe_offset;
};

#endif // QPDF_OFFSETINPUTSOURCE_HH
