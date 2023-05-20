#include <qpdf/MD5.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QUtil.hh>

#include <cstring>

MD5::MD5()
{
    init();
}

void
MD5::init()
{
    this->crypto = QPDFCryptoProvider::getImpl();
    this->crypto->MD5_init();
}

void
MD5::finalize()
{
    this->crypto->MD5_finalize();
}

void
MD5::reset()
{
    init();
}

void
MD5::encodeString(char const* str)
{
    size_t len = strlen(str);
    crypto->MD5_init();
    encodeDataIncrementally(str, len);
    crypto->MD5_finalize();
}

void
MD5::appendString(char const* input_string)
{
    encodeDataIncrementally(input_string, strlen(input_string));
}

void
MD5::encodeDataIncrementally(char const* data, size_t len)
{
    this->crypto->MD5_update(QUtil::unsigned_char_pointer(data), len);
}

void
MD5::encodeFile(char const* filename, qpdf_offset_t up_to_offset)
{
    char buffer[1024];

    FILE* file = QUtil::safe_fopen(filename, "rb");
    size_t len;
    size_t so_far = 0;
    size_t to_try = 1024;
    size_t up_to_size = 0;
    if (up_to_offset >= 0) {
        up_to_size = QIntC::to_size(up_to_offset);
    }
    do {
        if ((up_to_offset >= 0) && ((so_far + to_try) > up_to_size)) {
            to_try = up_to_size - so_far;
        }
        len = fread(buffer, 1, to_try, file);
        if (len > 0) {
            encodeDataIncrementally(buffer, len);
            so_far += len;
            if ((up_to_offset >= 0) && (so_far >= up_to_size)) {
                break;
            }
        }
    } while (len > 0);
    if (ferror(file)) {
        // Assume, perhaps incorrectly, that errno was set by the
        // underlying call to read....
        (void)fclose(file);
        QUtil::throw_system_error(
            std::string("MD5: read error on ") + filename);
    }
    (void)fclose(file);

    this->crypto->MD5_finalize();
}

void
MD5::digest(Digest result)
{
    this->crypto->MD5_finalize();
    this->crypto->MD5_digest(result);
}

void
MD5::print()
{
    Digest digest_val;
    digest(digest_val);

    unsigned int i;
    for (i = 0; i < 16; ++i) {
        printf("%02x", digest_val[i]);
    }
    printf("\n");
}

std::string
MD5::unparse()
{
    this->crypto->MD5_finalize();
    Digest digest_val;
    digest(digest_val);
    return QUtil::hex_encode(
        std::string(reinterpret_cast<char*>(digest_val), 16));
}

std::string
MD5::getDataChecksum(char const* buf, size_t len)
{
    MD5 m;
    m.encodeDataIncrementally(buf, len);
    return m.unparse();
}

std::string
MD5::getFileChecksum(char const* filename, qpdf_offset_t up_to_offset)
{
    MD5 m;
    m.encodeFile(filename, up_to_offset);
    return m.unparse();
}

bool
MD5::checkDataChecksum(char const* const checksum, char const* buf, size_t len)
{
    std::string actual_checksum = getDataChecksum(buf, len);
    return (checksum == actual_checksum);
}

bool
MD5::checkFileChecksum(
    char const* const checksum,
    char const* filename,
    qpdf_offset_t up_to_offset)
{
    bool result = false;
    try {
        std::string actual_checksum = getFileChecksum(filename, up_to_offset);
        result = (checksum == actual_checksum);
    } catch (std::runtime_error const&) {
        // Ignore -- return false
    }
    return result;
}
