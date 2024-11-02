#ifndef QPDF_INPUTSOURCE_PRIVATE_HH
#define QPDF_INPUTSOURCE_PRIVATE_HH

#include <qpdf/InputSource.hh>

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
