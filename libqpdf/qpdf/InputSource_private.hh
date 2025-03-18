#ifndef QPDF_INPUTSOURCE_PRIVATE_HH
#define QPDF_INPUTSOURCE_PRIVATE_HH

#include <qpdf/BufferInputSource.hh>
#include <qpdf/InputSource.hh>

#include <limits>
#include <sstream>
#include <stdexcept>

namespace qpdf::is
{
    class OffsetBuffer final: public InputSource
    {
      public:
        OffsetBuffer(std::string const& description, Buffer* buf, qpdf_offset_t global_offset) :
            proxied(description, buf),
            global_offset(global_offset)
        {
            if (global_offset < 0) {
                throw std::logic_error("is::OffsetBuffer constructed with negative offset");
            }
            last_offset = global_offset;
        }

        ~OffsetBuffer() final = default;

        qpdf_offset_t
        findAndSkipNextEOL() final
        {
            return proxied.findAndSkipNextEOL() + global_offset;
        }

        std::string const&
        getName() const final
        {
            return proxied.getName();
        }

        qpdf_offset_t
        tell() final
        {
            return proxied.tell() + global_offset;
        }

        void
        seek(qpdf_offset_t offset, int whence) final
        {
            if (whence == SEEK_SET) {
                proxied.seek(offset - global_offset, whence);
            } else {
                proxied.seek(offset, whence);
            }
        }

        void
        rewind() final
        {
            seek(0, SEEK_SET);
        }

        size_t
        read(char* buffer, size_t length) final
        {
            size_t result = proxied.read(buffer, length);
            setLastOffset(proxied.getLastOffset() + global_offset);
            return result;
        }

        void
        unreadCh(char ch) final
        {
            proxied.unreadCh(ch);
        }

      private:
        BufferInputSource proxied;
        qpdf_offset_t global_offset;
    };

} // namespace qpdf::is

inline size_t
InputSource::read(std::string& str, size_t count, qpdf_offset_t at)
{
    if (at >= 0) {
        seek(at, SEEK_SET);
    }
    str.resize(count);
    str.resize(read(str.data(), count));
    return str.size();
}

inline std::string
InputSource::read(size_t count, qpdf_offset_t at)
{
    std::string result(count, '\0');
    (void)read(result, count, at);
    return result;
}

inline void
InputSource::loadBuffer()
{
    buf_idx = 0;
    buf_len = qpdf_offset_t(read(buffer, buf_size));
    // NB read sets last_offset
    buf_start = last_offset;
}

inline qpdf_offset_t
InputSource::fastTell()
{
    if (buf_len == 0) {
        loadBuffer();
    } else {
        auto curr = tell();
        if (curr < buf_start || curr >= (buf_start + buf_len)) {
            loadBuffer();
        } else {
            last_offset = curr;
            buf_idx = curr - buf_start;
        }
    }
    return last_offset;
}

inline bool
InputSource::fastRead(char& ch)
{
    // Before calling fastRead, fastTell must be called to prepare the buffer. Once reading is
    // complete, fastUnread must be called to set the correct file position.
    if (buf_idx < buf_len) {
        ch = buffer[buf_idx];
        ++(buf_idx);
        ++(last_offset);
        return true;

    } else if (buf_len == 0) {
        return false;
    } else {
        seek(buf_start + buf_len, SEEK_SET);
        fastTell();
        return fastRead(ch);
    }
}

inline void
InputSource::fastUnread(bool back)
{
    last_offset -= back ? 1 : 0;
    seek(last_offset, SEEK_SET);
}

#endif // QPDF_INPUTSOURCE_PRIVATE_HH
