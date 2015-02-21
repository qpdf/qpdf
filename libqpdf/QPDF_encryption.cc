// This file implements methods from the QPDF class that involve
// encryption.

#include <qpdf/QPDF.hh>

#include <qpdf/QPDFExc.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Pl_RC4.hh>
#include <qpdf/Pl_AES_PDF.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_SHA2.hh>
#include <qpdf/RC4.hh>
#include <qpdf/MD5.hh>

#include <algorithm>
#include <assert.h>
#include <string.h>

static unsigned char const padding_string[] = {
    0x28, 0xbf, 0x4e, 0x5e, 0x4e, 0x75, 0x8a, 0x41,
    0x64, 0x00, 0x4e, 0x56, 0xff, 0xfa, 0x01, 0x08,
    0x2e, 0x2e, 0x00, 0xb6, 0xd0, 0x68, 0x3e, 0x80,
    0x2f, 0x0c, 0xa9, 0xfe, 0x64, 0x53, 0x69, 0x7a
};

static unsigned int const key_bytes = 32;

// V4 key lengths apply to V <= 4
static unsigned int const OU_key_bytes_V4 = sizeof(MD5::Digest);

static unsigned int const OU_key_bytes_V5 = 48;
static unsigned int const OUE_key_bytes_V5 = 32;
static unsigned int const Perms_key_bytes_V5 = 16;

int
QPDF::EncryptionData::getV() const
{
    return this->V;
}

int
QPDF::EncryptionData::getR() const
{
    return this->R;
}

int
QPDF::EncryptionData::getLengthBytes() const
{
    return this->Length_bytes;
}

int
QPDF::EncryptionData::getP() const
{
    return this->P;
}

std::string const&
QPDF::EncryptionData::getO() const
{
    return this->O;
}

std::string const&
QPDF::EncryptionData::getU() const
{
    return this->U;
}

std::string const&
QPDF::EncryptionData::getOE() const
{
    return this->OE;
}

std::string const&
QPDF::EncryptionData::getUE() const
{
    return this->UE;
}

std::string const&
QPDF::EncryptionData::getPerms() const
{
    return this->Perms;
}

std::string const&
QPDF::EncryptionData::getId1() const
{
    return this->id1;
}

bool
QPDF::EncryptionData::getEncryptMetadata() const
{
    return this->encrypt_metadata;
}

void
QPDF::EncryptionData::setO(std::string const& O)
{
    this->O = O;
}

void
QPDF::EncryptionData::setU(std::string const& U)
{
    this->U = U;
}

void
QPDF::EncryptionData::setV5EncryptionParameters(
    std::string const& O,
    std::string const& OE,
    std::string const& U,
    std::string const& UE,
    std::string const& Perms)
{
    this->O = O;
    this->OE = OE;
    this->U = U;
    this->UE = UE;
    this->Perms = Perms;
}

static void
pad_or_truncate_password_V4(std::string const& password, char k1[key_bytes])
{
    int password_bytes = std::min(static_cast<size_t>(key_bytes),
                                  password.length());
    int pad_bytes = key_bytes - password_bytes;
    memcpy(k1, password.c_str(), password_bytes);
    memcpy(k1 + password_bytes, padding_string, pad_bytes);
}

void
QPDF::trim_user_password(std::string& user_password)
{
    // Although unnecessary, this routine trims the padding string
    // from the end of a user password.  Its only purpose is for
    // recovery of user passwords which is done in the test suite.
    char const* cstr = user_password.c_str();
    size_t len = user_password.length();
    if (len < key_bytes)
    {
	return;
    }

    char const* p1 = cstr;
    char const* p2 = 0;
    while ((p2 = strchr(p1, '\x28')) != 0)
    {
	if (memcmp(p2, padding_string, len - (p2 - cstr)) == 0)
	{
	    user_password = user_password.substr(0, p2 - cstr);
	    return;
	}
        else
        {
            QTC::TC("qpdf", "QPDF_encryption skip 0x28");
            p1 = p2 + 1;
        }
    }
}

static std::string
pad_or_truncate_password_V4(std::string const& password)
{
    char k1[key_bytes];
    pad_or_truncate_password_V4(password, k1);
    return std::string(k1, key_bytes);
}

static std::string
truncate_password_V5(std::string const& password)
{
    return password.substr(
        0, std::min(static_cast<size_t>(127), password.length()));
}

static void
iterate_md5_digest(MD5& md5, MD5::Digest& digest, int iterations)
{
    md5.digest(digest);

    for (int i = 0; i < iterations; ++i)
    {
	MD5 m;
	m.encodeDataIncrementally(reinterpret_cast<char*>(digest),
                                  sizeof(digest));
	m.digest(digest);
    }
}


static void
iterate_rc4(unsigned char* data, int data_len,
	    unsigned char* okey, int key_len,
	    int iterations, bool reverse)
{
    unsigned char* key = new unsigned char[key_len];
    for (int i = 0; i < iterations; ++i)
    {
	int const xor_value = (reverse ? iterations - 1 - i : i);
	for (int j = 0; j < key_len; ++j)
	{
	    key[j] = okey[j] ^ xor_value;
	}
	RC4 rc4(key, key_len);
	rc4.process(data, data_len);
    }
    delete [] key;
}

static std::string
process_with_aes(std::string const& key,
                 bool encrypt,
                 std::string const& data,
                 size_t outlength = 0,
                 unsigned int repetitions = 1,
                 unsigned char const* iv = 0,
                 size_t iv_length = 0)
{
    Pl_Buffer buffer("buffer");
    Pl_AES_PDF aes("aes", &buffer, encrypt,
                   QUtil::unsigned_char_pointer(key),
                   key.length());
    if (iv)
    {
        aes.setIV(iv, iv_length);
    }
    else
    {
        aes.useZeroIV();
    }
    aes.disablePadding();
    for (unsigned int i = 0; i < repetitions; ++i)
    {
        aes.write(QUtil::unsigned_char_pointer(data), data.length());
    }
    aes.finish();
    PointerHolder<Buffer> bufp = buffer.getBuffer();
    if (outlength == 0)
    {
        outlength = bufp->getSize();
    }
    else
    {
        outlength = std::min(outlength, bufp->getSize());
    }
    return std::string(reinterpret_cast<char*>(bufp->getBuffer()), outlength);
}

static std::string
hash_V5(std::string const& password,
        std::string const& salt,
        std::string const& udata,
        QPDF::EncryptionData const& data)
{
    Pl_SHA2 hash(256);
    hash.write(QUtil::unsigned_char_pointer(password), password.length());
    hash.write(QUtil::unsigned_char_pointer(salt), salt.length());
    hash.write(QUtil::unsigned_char_pointer(udata), udata.length());
    hash.finish();
    std::string K = hash.getRawDigest();

    std::string result;
    if (data.getR() < 6)
    {
        result = K;
    }
    else
    {
        // Algorithm 2.B from ISO 32000-1 chapter 7: Computing a hash

        int round_number = 0;
        bool done = false;
        while (! done)
        {
            // The hash algorithm has us setting K initially to the R5
            // value and then repeating a series of steps 64 times
            // before starting with the termination case testing.  The
            // wording of the specification is very unclear as to the
            // exact number of times it should be run since the
            // wording about whether the initial setup counts as round
            // 0 or not is ambiguous.  This code counts the initial
            // setup (R5) value as round 0, which appears to be
            // correct.  This was determined to be correct by
            // increasing or decreasing the number of rounds by 1 or 2
            // from this value and generating 20 test files.  In this
            // interpretation, all the test files worked with Adobe
            // Reader X.  In the other configurations, many of the
            // files did not work, and we were accurately able to
            // predict which files didn't work by looking at the
            // conditions under which we terminated repetition.

            ++round_number;
            std::string K1 = password + K + udata;
            assert(K.length() >= 32);
            std::string E = process_with_aes(
                K.substr(0, 16), true, K1, 0, 64,
                QUtil::unsigned_char_pointer(K.substr(16, 16)), 16);

            // E_mod_3 is supposed to be mod 3 of the first 16 bytes
            // of E taken as as a (128-bit) big-endian number.  Since
            // (xy mod n) is equal to ((x mod n) + (y mod n)) mod n
            // and since 256 mod n is 1, we can just take the sums of
            // the the mod 3s of each byte to get the same result.
            int E_mod_3 = 0;
            for (unsigned int i = 0; i < 16; ++i)
            {
                E_mod_3 += static_cast<unsigned char>(E.at(i));
            }
            E_mod_3 %= 3;
            int next_hash = ((E_mod_3 == 0) ? 256 :
                             (E_mod_3 == 1) ? 384 :
                             512);
            Pl_SHA2 hash(next_hash);
            hash.write(QUtil::unsigned_char_pointer(E), E.length());
            hash.finish();
            K = hash.getRawDigest();

            if (round_number >= 64)
            {
                unsigned int ch = static_cast<unsigned char>(*(E.rbegin()));

                if (ch <= static_cast<unsigned int>(round_number - 32))
                {
                    done = true;
                }
            }
        }
        result = K.substr(0, 32);
    }

    return result;
}

std::string
QPDF::compute_data_key(std::string const& encryption_key,
		       int objid, int generation, bool use_aes,
                       int encryption_V, int encryption_R)
{
    // Algorithm 3.1 from the PDF 1.7 Reference Manual

    std::string result = encryption_key;

    if (encryption_V >= 5)
    {
        // Algorithm 3.1a (PDF 1.7 extension level 3): just use
        // encryption key straight.
        return result;
    }

    // Append low three bytes of object ID and low two bytes of generation
    result += static_cast<char>(objid & 0xff);
    result += static_cast<char>((objid >> 8) & 0xff);
    result += static_cast<char>((objid >> 16) & 0xff);
    result += static_cast<char>(generation & 0xff);
    result += static_cast<char>((generation >> 8) & 0xff);
    if (use_aes)
    {
	result += "sAlT";
    }

    MD5 md5;
    md5.encodeDataIncrementally(result.c_str(), result.length());
    MD5::Digest digest;
    md5.digest(digest);
    return std::string(reinterpret_cast<char*>(digest),
		       std::min(result.length(), static_cast<size_t>(16)));
}

std::string
QPDF::compute_encryption_key(
    std::string const& password, EncryptionData const& data)
{
    if (data.getV() >= 5)
    {
        // For V >= 5, the encryption key is generated and stored in
        // the file, encrypted separately with both user and owner
        // passwords.
        return recover_encryption_key_with_password(password, data);
    }
    else
    {
        // For V < 5, the encryption key is derived from the user
        // password.
        return compute_encryption_key_from_password(password, data);
    }
}

std::string
QPDF::compute_encryption_key_from_password(
    std::string const& password, EncryptionData const& data)
{
    // Algorithm 3.2 from the PDF 1.7 Reference Manual

    // This code does not properly handle Unicode passwords.
    // Passwords are supposed to be converted from OS codepage
    // characters to PDFDocEncoding.  Unicode passwords are supposed
    // to be converted to OS codepage before converting to
    // PDFDocEncoding.  We instead require the password to be
    // presented in its final form.

    MD5 md5;
    md5.encodeDataIncrementally(
	pad_or_truncate_password_V4(password).c_str(), key_bytes);
    md5.encodeDataIncrementally(data.getO().c_str(), key_bytes);
    char pbytes[4];
    int P = data.getP();
    pbytes[0] = static_cast<char>(P & 0xff);
    pbytes[1] = static_cast<char>((P >> 8) & 0xff);
    pbytes[2] = static_cast<char>((P >> 16) & 0xff);
    pbytes[3] = static_cast<char>((P >> 24) & 0xff);
    md5.encodeDataIncrementally(pbytes, 4);
    md5.encodeDataIncrementally(data.getId1().c_str(),
                                data.getId1().length());
    if ((data.getR() >= 4) && (! data.getEncryptMetadata()))
    {
	char bytes[4];
	memset(bytes, 0xff, 4);
	md5.encodeDataIncrementally(bytes, 4);
    }
    MD5::Digest digest;
    iterate_md5_digest(md5, digest, ((data.getR() >= 3) ? 50 : 0));
    return std::string(reinterpret_cast<char*>(digest),
                       std::min(static_cast<int>(sizeof(digest)),
                                data.getLengthBytes()));
}

static void
compute_O_rc4_key(std::string const& user_password,
		  std::string const& owner_password,
		  QPDF::EncryptionData const& data,
		  unsigned char key[OU_key_bytes_V4])
{
    if (data.getV() >= 5)
    {
	throw std::logic_error(
	    "compute_O_rc4_key called for file with V >= 5");
    }
    std::string password = owner_password;
    if (password.empty())
    {
	password = user_password;
    }
    MD5 md5;
    md5.encodeDataIncrementally(
	pad_or_truncate_password_V4(password).c_str(), key_bytes);
    MD5::Digest digest;
    iterate_md5_digest(md5, digest, ((data.getR() >= 3) ? 50 : 0));
    memcpy(key, digest, OU_key_bytes_V4);
}

static std::string
compute_O_value(std::string const& user_password,
		std::string const& owner_password,
		QPDF::EncryptionData const& data)
{
    // Algorithm 3.3 from the PDF 1.7 Reference Manual

    unsigned char O_key[OU_key_bytes_V4];
    compute_O_rc4_key(user_password, owner_password, data, O_key);

    char upass[key_bytes];
    pad_or_truncate_password_V4(user_password, upass);
    iterate_rc4(QUtil::unsigned_char_pointer(upass), key_bytes,
		O_key, data.getLengthBytes(),
                (data.getR() >= 3) ? 20 : 1, false);
    return std::string(upass, key_bytes);
}

static
std::string
compute_U_value_R2(std::string const& user_password,
		   QPDF::EncryptionData const& data)
{
    // Algorithm 3.4 from the PDF 1.7 Reference Manual

    std::string k1 = QPDF::compute_encryption_key(user_password, data);
    char udata[key_bytes];
    pad_or_truncate_password_V4("", udata);
    iterate_rc4(QUtil::unsigned_char_pointer(udata), key_bytes,
		QUtil::unsigned_char_pointer(k1),
                data.getLengthBytes(), 1, false);
    return std::string(udata, key_bytes);
}

static
std::string
compute_U_value_R3(std::string const& user_password,
		   QPDF::EncryptionData const& data)
{
    // Algorithm 3.5 from the PDF 1.7 Reference Manual

    std::string k1 = QPDF::compute_encryption_key(user_password, data);
    MD5 md5;
    md5.encodeDataIncrementally(
	pad_or_truncate_password_V4("").c_str(), key_bytes);
    md5.encodeDataIncrementally(data.getId1().c_str(),
                                data.getId1().length());
    MD5::Digest digest;
    md5.digest(digest);
    iterate_rc4(digest, sizeof(MD5::Digest),
		QUtil::unsigned_char_pointer(k1),
                data.getLengthBytes(), 20, false);
    char result[key_bytes];
    memcpy(result, digest, sizeof(MD5::Digest));
    // pad with arbitrary data -- make it consistent for the sake of
    // testing
    for (unsigned int i = sizeof(MD5::Digest); i < key_bytes; ++i)
    {
	result[i] = static_cast<char>((i * i) % 0xff);
    }
    return std::string(result, key_bytes);
}

static std::string
compute_U_value(std::string const& user_password,
		QPDF::EncryptionData const& data)
{
    if (data.getR() >= 3)
    {
	return compute_U_value_R3(user_password, data);
    }

    return compute_U_value_R2(user_password, data);
}

static bool
check_user_password_V4(std::string const& user_password,
                       QPDF::EncryptionData const& data)
{
    // Algorithm 3.6 from the PDF 1.7 Reference Manual

    std::string u_value = compute_U_value(user_password, data);
    int to_compare = ((data.getR() >= 3) ? sizeof(MD5::Digest)
                      : key_bytes);
    return (memcmp(data.getU().c_str(), u_value.c_str(), to_compare) == 0);
}

static bool
check_user_password_V5(std::string const& user_password,
                       QPDF::EncryptionData const& data)
{
    // Algorithm 3.11 from the PDF 1.7 extension level 3

    std::string user_data = data.getU().substr(0, 32);
    std::string validation_salt = data.getU().substr(32, 8);
    std::string password = truncate_password_V5(user_password);
    return (hash_V5(password, validation_salt, "", data) == user_data);
}

static bool
check_user_password(std::string const& user_password,
		    QPDF::EncryptionData const& data)
{
    if (data.getV() < 5)
    {
        return check_user_password_V4(user_password, data);
    }
    else
    {
        return check_user_password_V5(user_password, data);
    }
}

static bool
check_owner_password_V4(std::string& user_password,
                        std::string const& owner_password,
                        QPDF::EncryptionData const& data)
{
    // Algorithm 3.7 from the PDF 1.7 Reference Manual

    unsigned char key[OU_key_bytes_V4];
    compute_O_rc4_key(user_password, owner_password, data, key);
    unsigned char O_data[key_bytes];
    memcpy(O_data, QUtil::unsigned_char_pointer(data.getO()), key_bytes);
    iterate_rc4(O_data, key_bytes, key, data.getLengthBytes(),
                (data.getR() >= 3) ? 20 : 1, true);
    std::string new_user_password =
        std::string(reinterpret_cast<char*>(O_data), key_bytes);
    bool result = false;
    if (check_user_password(new_user_password, data))
    {
        result = true;
        user_password = new_user_password;
    }
    return result;
}

static bool
check_owner_password_V5(std::string const& owner_password,
                        QPDF::EncryptionData const& data)
{
    // Algorithm 3.12 from the PDF 1.7 extension level 3

    std::string user_data = data.getU().substr(0, 48);
    std::string owner_data = data.getO().substr(0, 32);
    std::string validation_salt = data.getO().substr(32, 8);
    std::string password = truncate_password_V5(owner_password);
    return (hash_V5(password, validation_salt, user_data,
                    data) == owner_data);
}

static bool
check_owner_password(std::string& user_password,
		     std::string const& owner_password,
		     QPDF::EncryptionData const& data)
{
    if (data.getV() < 5)
    {
        return check_owner_password_V4(user_password, owner_password, data);
    }
    else
    {
        return check_owner_password_V5(owner_password, data);
    }
}

std::string
QPDF::recover_encryption_key_with_password(
    std::string const& password, EncryptionData const& data)
{
    // Disregard whether Perms is valid.
    bool disregard;
    return recover_encryption_key_with_password(password, data, disregard);
}

static void
compute_U_UE_value_V5(std::string const& user_password,
                      std::string const& encryption_key,
                      QPDF::EncryptionData const& data,
                      std::string& U, std::string& UE)
{
    // Algorithm 3.8 from the PDF 1.7 extension level 3
    char k[16];
    QUtil::initializeWithRandomBytes(
        QUtil::unsigned_char_pointer(k), sizeof(k));
    std::string validation_salt(k, 8);
    std::string key_salt(k + 8, 8);
    U = hash_V5(user_password, validation_salt, "", data) +
        validation_salt + key_salt;
    std::string intermediate_key = hash_V5(user_password, key_salt, "", data);
    UE = process_with_aes(intermediate_key, true, encryption_key);
}

static void
compute_O_OE_value_V5(std::string const& owner_password,
                      std::string const& encryption_key,
                      QPDF::EncryptionData const& data,
                      std::string const& U,
                      std::string& O, std::string& OE)
{
    // Algorithm 3.9 from the PDF 1.7 extension level 3
    char k[16];
    QUtil::initializeWithRandomBytes(
        QUtil::unsigned_char_pointer(k), sizeof(k));
    std::string validation_salt(k, 8);
    std::string key_salt(k + 8, 8);
    O = hash_V5(owner_password, validation_salt, U, data) +
        validation_salt + key_salt;
    std::string intermediate_key = hash_V5(owner_password, key_salt, U, data);
    OE = process_with_aes(intermediate_key, true, encryption_key);
}

void
compute_Perms_value_V5_clear(std::string const& encryption_key,
                             QPDF::EncryptionData const& data,
                             unsigned char k[16])
{
    // From algorithm 3.10 from the PDF 1.7 extension level 3
    unsigned long long extended_perms = 0xffffffff00000000LL | data.getP();
    for (int i = 0; i < 8; ++i)
    {
        k[i] = static_cast<unsigned char>(extended_perms & 0xff);
        extended_perms >>= 8;
    }
    k[8] = data.getEncryptMetadata() ? 'T' : 'F';
    k[9] = 'a';
    k[10] = 'd';
    k[11] = 'b';
    QUtil::initializeWithRandomBytes(k + 12, 4);
}

static std::string
compute_Perms_value_V5(std::string const& encryption_key,
                       QPDF::EncryptionData const& data)
{
    // Algorithm 3.10 from the PDF 1.7 extension level 3
    unsigned char k[16];
    compute_Perms_value_V5_clear(encryption_key, data, k);
    return process_with_aes(
        encryption_key, true,
        std::string(reinterpret_cast<char*>(k), sizeof(k)));
}

std::string
QPDF::recover_encryption_key_with_password(
    std::string const& password, EncryptionData const& data,
    bool& perms_valid)
{
    // Algorithm 3.2a from the PDF 1.7 extension level 3

    // This code does not handle Unicode passwords correctly.
    // Empirical evidence suggests that most viewers don't.  We are
    // supposed to process the input string with the SASLprep (RFC
    // 4013) profile of stringprep (RFC 3454) and then convert the
    // result to UTF-8.

    perms_valid = false;
    std::string key_password = truncate_password_V5(password);
    std::string key_salt;
    std::string user_data;
    std::string encrypted_file_key;
    if (check_owner_password_V5(key_password, data))
    {
        key_salt = data.getO().substr(40, 8);
        user_data = data.getU().substr(0, 48);
        encrypted_file_key = data.getOE().substr(0, 32);
    }
    else if (check_user_password_V5(key_password, data))
    {
        key_salt = data.getU().substr(40, 8);
        encrypted_file_key = data.getUE().substr(0, 32);
    }
    std::string intermediate_key =
        hash_V5(key_password, key_salt, user_data, data);
    std::string file_key =
        process_with_aes(intermediate_key, false, encrypted_file_key);

    // Decrypt Perms and check against expected value
    std::string perms_check =
        process_with_aes(file_key, false, data.getPerms(), 12);
    unsigned char k[16];
    compute_Perms_value_V5_clear(file_key, data, k);
    perms_valid = (memcmp(perms_check.c_str(), k, 12) == 0);

    return file_key;
}

QPDF::encryption_method_e
QPDF::interpretCF(QPDFObjectHandle cf)
{
    if (cf.isName())
    {
	std::string filter = cf.getName();
	if (this->crypt_filters.count(filter) != 0)
	{
	    return this->crypt_filters[filter];
	}
	else if (filter == "/Identity")
	{
	    return e_none;
	}
	else
	{
	    return e_unknown;
	}
    }
    else
    {
	// Default: /Identity
	return e_none;
    }
}

void
QPDF::initializeEncryption()
{
    if (this->encryption_initialized)
    {
	return;
    }
    this->encryption_initialized = true;

    // After we initialize encryption parameters, we must used stored
    // key information and never look at /Encrypt again.  Otherwise,
    // things could go wrong if someone mutates the encryption
    // dictionary.

    if (! this->trailer.hasKey("/Encrypt"))
    {
	return;
    }

    // Go ahead and set this->encryption here.  That way, isEncrypted
    // will return true even if there were errors reading the
    // encryption dictionary.
    this->encrypted = true;

    std::string id1;
    QPDFObjectHandle id_obj = this->trailer.getKey("/ID");
    if ((id_obj.isArray() &&
         (id_obj.getArrayNItems() == 2) &&
         id_obj.getArrayItem(0).isString()))
    {
        id1 = id_obj.getArrayItem(0).getStringValue();
    }
    else
    {
        // Treating a missing ID as the empty string enables qpdf to
        // decrypt some invalid encrypted files with no /ID that
        // poppler can read but Adobe Reader can't.
	warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                     "trailer", this->file->getLastOffset(),
                     "invalid /ID in trailer dictionary"));
    }

    QPDFObjectHandle encryption_dict = this->trailer.getKey("/Encrypt");
    if (! encryption_dict.isDictionary())
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      this->last_object_description,
		      this->file->getLastOffset(),
		      "/Encrypt in trailer dictionary is not a dictionary");
    }

    if (! (encryption_dict.getKey("/Filter").isName() &&
	   (encryption_dict.getKey("/Filter").getName() == "/Standard")))
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "encryption dictionary", this->file->getLastOffset(),
		      "unsupported encryption filter");
    }
    if (! encryption_dict.getKey("/SubFilter").isNull())
    {
	warn(QPDFExc(qpdf_e_unsupported, this->file->getName(),
		     "encryption dictionary", this->file->getLastOffset(),
		     "file uses encryption SubFilters,"
		     " which qpdf does not support"));
    }

    if (! (encryption_dict.getKey("/V").isInteger() &&
	   encryption_dict.getKey("/R").isInteger() &&
	   encryption_dict.getKey("/O").isString() &&
	   encryption_dict.getKey("/U").isString() &&
	   encryption_dict.getKey("/P").isInteger()))
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "encryption dictionary", this->file->getLastOffset(),
		      "some encryption dictionary parameters are missing "
		      "or the wrong type");
    }

    int V = encryption_dict.getKey("/V").getIntValue();
    int R = encryption_dict.getKey("/R").getIntValue();
    std::string O = encryption_dict.getKey("/O").getStringValue();
    std::string U = encryption_dict.getKey("/U").getStringValue();
    unsigned int P = encryption_dict.getKey("/P").getIntValue();

    // If supporting new encryption R/V values, remember to update
    // error message inside this if statement.
    if (! (((R >= 2) && (R <= 6)) &&
	   ((V == 1) || (V == 2) || (V == 4) || (V == 5))))
    {
	throw QPDFExc(qpdf_e_unsupported, this->file->getName(),
		      "encryption dictionary", this->file->getLastOffset(),
		      "Unsupported /R or /V in encryption dictionary; R = " +
                      QUtil::int_to_string(R) + " (max 6), V = " +
                      QUtil::int_to_string(V) + " (max 5)");
    }

    this->encryption_V = V;
    this->encryption_R = R;

    // OE, UE, and Perms are only present if V >= 5.
    std::string OE;
    std::string UE;
    std::string Perms;

    if (V < 5)
    {
        if (! ((O.length() == key_bytes) && (U.length() == key_bytes)))
        {
            throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                          "encryption dictionary", this->file->getLastOffset(),
                          "incorrect length for /O and/or /U in "
                          "encryption dictionary");
        }
    }
    else
    {
        if (! (encryption_dict.getKey("/OE").isString() &&
               encryption_dict.getKey("/UE").isString() &&
               encryption_dict.getKey("/Perms").isString()))
        {
            throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                          "encryption dictionary", this->file->getLastOffset(),
                          "some V=5 encryption dictionary parameters are "
                          "missing or the wrong type");
        }
        OE = encryption_dict.getKey("/OE").getStringValue();
        UE = encryption_dict.getKey("/UE").getStringValue();
        Perms = encryption_dict.getKey("/Perms").getStringValue();

        if ((O.length() < OU_key_bytes_V5) ||
            (U.length() < OU_key_bytes_V5) ||
            (OE.length() < OUE_key_bytes_V5) ||
            (UE.length() < OUE_key_bytes_V5) ||
            (Perms.length() < Perms_key_bytes_V5))
        {
            throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                          "encryption dictionary", this->file->getLastOffset(),
                          "incorrect length for some of"
                          " /O, /U, /OE, /UE, or /Perms in"
                          " encryption dictionary");
        }
    }

    int Length = 40;
    if (encryption_dict.getKey("/Length").isInteger())
    {
	Length = encryption_dict.getKey("/Length").getIntValue();
	if ((Length % 8) || (Length < 40) || (Length > 256))
	{
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  "encryption dictionary", this->file->getLastOffset(),
			  "invalid /Length value in encryption dictionary");
	}
    }

    this->encrypt_metadata = true;
    if ((V >= 4) && (encryption_dict.getKey("/EncryptMetadata").isBool()))
    {
	this->encrypt_metadata =
	    encryption_dict.getKey("/EncryptMetadata").getBoolValue();
    }

    if ((V == 4) || (V == 5))
    {
	QPDFObjectHandle CF = encryption_dict.getKey("/CF");
	std::set<std::string> keys = CF.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    std::string const& filter = *iter;
	    QPDFObjectHandle cdict = CF.getKey(filter);
	    if (cdict.isDictionary())
	    {
		encryption_method_e method = e_none;
		if (cdict.getKey("/CFM").isName())
		{
		    std::string method_name = cdict.getKey("/CFM").getName();
		    if (method_name == "/V2")
		    {
			QTC::TC("qpdf", "QPDF_encryption CFM V2");
			method = e_rc4;
		    }
		    else if (method_name == "/AESV2")
		    {
			QTC::TC("qpdf", "QPDF_encryption CFM AESV2");
			method = e_aes;
		    }
		    else if (method_name == "/AESV3")
		    {
			QTC::TC("qpdf", "QPDF_encryption CFM AESV3");
			method = e_aesv3;
		    }
		    else
		    {
			// Don't complain now -- maybe we won't need
			// to reference this type.
			method = e_unknown;
		    }
		}
		this->crypt_filters[filter] = method;
	    }
	}

	QPDFObjectHandle StmF = encryption_dict.getKey("/StmF");
	QPDFObjectHandle StrF = encryption_dict.getKey("/StrF");
	QPDFObjectHandle EFF = encryption_dict.getKey("/EFF");
	this->cf_stream = interpretCF(StmF);
	this->cf_string = interpretCF(StrF);
	if (EFF.isName())
	{
	    this->cf_file = interpretCF(EFF);
	}
	else
	{
	    this->cf_file = this->cf_stream;
	}
    }

    EncryptionData data(V, R, Length / 8, P, O, U, OE, UE, Perms,
                        id1, this->encrypt_metadata);
    if (check_owner_password(
	    this->user_password, this->provided_password, data))
    {
	// password supplied was owner password; user_password has
	// been initialized for V < 5
    }
    else if (check_user_password(this->provided_password, data))
    {
	this->user_password = this->provided_password;
    }
    else
    {
	throw QPDFExc(qpdf_e_password, this->file->getName(),
		      "", 0, "invalid password");
    }

    if (V < 5)
    {
        // For V < 5, the user password is encrypted with the owner
        // password, and the user password is always used for
        // computing the encryption key.
        this->encryption_key = compute_encryption_key(
            this->user_password, data);
    }
    else
    {
        // For V >= 5, either password can be used independently to
        // compute the encryption key, and neither password can be
        // used to recover the other.
        bool perms_valid;
        this->encryption_key = recover_encryption_key_with_password(
            this->provided_password, data, perms_valid);
        if (! perms_valid)
        {
            warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                         "encryption dictionary", this->file->getLastOffset(),
                         "/Perms field in encryption dictionary"
                         " doesn't match expected value"));
        }
    }
}

std::string
QPDF::getKeyForObject(int objid, int generation, bool use_aes)
{
    if (! this->encrypted)
    {
	throw std::logic_error(
	    "request for encryption key in non-encrypted PDF");
    }

    if (! ((objid == this->cached_key_objid) &&
	   (generation == this->cached_key_generation)))
    {
	this->cached_object_encryption_key =
	    compute_data_key(this->encryption_key, objid, generation,
                             use_aes, this->encryption_V, this->encryption_R);
	this->cached_key_objid = objid;
	this->cached_key_generation = generation;
    }

    return this->cached_object_encryption_key;
}

void
QPDF::decryptString(std::string& str, int objid, int generation)
{
    if (objid == 0)
    {
	return;
    }
    bool use_aes = false;
    if (this->encryption_V >= 4)
    {
	switch (this->cf_string)
	{
	  case e_none:
	    return;

	  case e_aes:
	    use_aes = true;
	    break;

	  case e_aesv3:
	    use_aes = true;
	    break;

	  case e_rc4:
	    break;

	  default:
	    warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			 this->last_object_description,
			 this->file->getLastOffset(),
			 "unknown encryption filter for strings"
			 " (check /StrF in /Encrypt dictionary);"
			 " strings may be decrypted improperly"));
	    // To avoid repeated warnings, reset cf_string.  Assume
	    // we'd want to use AES if V == 4.
	    this->cf_string = e_aes;
	    break;
	}
    }

    std::string key = getKeyForObject(objid, generation, use_aes);
    try
    {
	if (use_aes)
	{
	    QTC::TC("qpdf", "QPDF_encryption aes decode string");
	    Pl_Buffer bufpl("decrypted string");
	    Pl_AES_PDF pl("aes decrypt string", &bufpl, false,
			  QUtil::unsigned_char_pointer(key),
                          key.length());
	    pl.write(QUtil::unsigned_char_pointer(str), str.length());
	    pl.finish();
	    PointerHolder<Buffer> buf = bufpl.getBuffer();
	    str = std::string(reinterpret_cast<char*>(buf->getBuffer()),
                              buf->getSize());
	}
	else
	{
	    QTC::TC("qpdf", "QPDF_encryption rc4 decode string");
	    unsigned int vlen = str.length();
	    // Using PointerHolder guarantees that tmp will
	    // be freed even if rc4.process throws an exception.
	    PointerHolder<char> tmp(true, QUtil::copy_string(str));
	    RC4 rc4(QUtil::unsigned_char_pointer(key), key.length());
	    rc4.process(QUtil::unsigned_char_pointer(tmp.getPointer()), vlen);
	    str = std::string(tmp.getPointer(), vlen);
	}
    }
    catch (QPDFExc&)
    {
	throw;
    }
    catch (std::runtime_error& e)
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      this->last_object_description,
		      this->file->getLastOffset(),
		      "error decrypting string for object " +
		      QUtil::int_to_string(objid) + " " +
		      QUtil::int_to_string(generation) + ": " + e.what());
    }
}

void
QPDF::decryptStream(Pipeline*& pipeline, int objid, int generation,
		    QPDFObjectHandle& stream_dict,
		    std::vector<PointerHolder<Pipeline> >& heap)
{
    std::string type;
    if (stream_dict.getKey("/Type").isName())
    {
	type = stream_dict.getKey("/Type").getName();
    }
    if (type == "/XRef")
    {
	QTC::TC("qpdf", "QPDF_encryption xref stream from encrypted file");
	return;
    }
    bool use_aes = false;
    if (this->encryption_V >= 4)
    {
	encryption_method_e method = e_unknown;
	std::string method_source = "/StmF from /Encrypt dictionary";

	if (stream_dict.getKey("/Filter").isOrHasName("/Crypt"))
        {
            if (stream_dict.getKey("/DecodeParms").isDictionary())
            {
                QPDFObjectHandle decode_parms =
                    stream_dict.getKey("/DecodeParms");
                if (decode_parms.getKey("/Type").isName() &&
                    (decode_parms.getKey("/Type").getName() ==
                     "/CryptFilterDecodeParms"))
                {
                    QTC::TC("qpdf", "QPDF_encryption stream crypt filter");
                    method = interpretCF(decode_parms.getKey("/Name"));
                    method_source = "stream's Crypt decode parameters";
                }
            }
            else if (stream_dict.getKey("/DecodeParms").isArray() &&
                     stream_dict.getKey("/Filter").isArray())
            {
                QPDFObjectHandle filter = stream_dict.getKey("/Filter");
                QPDFObjectHandle decode = stream_dict.getKey("/DecodeParms");
                if (filter.getArrayNItems() == decode.getArrayNItems())
                {
                    for (int i = 0; i < filter.getArrayNItems(); ++i)
                    {
                        if (filter.getArrayItem(i).isName() &&
                            (filter.getArrayItem(i).getName() == "/Crypt"))
                        {
                            QPDFObjectHandle crypt_params =
                                decode.getArrayItem(i);
                            if (crypt_params.isDictionary() &&
                                crypt_params.getKey("/Name").isName())
                            {
                                QTC::TC("qpdf", "QPDF_encrypt crypt array");
                                method = interpretCF(
                                    crypt_params.getKey("/Name"));
                                method_source = "stream's Crypt "
                                    "decode parameters (array)";
                            }
                        }
                    }
                }
            }
	}

	if (method == e_unknown)
	{
	    if ((! this->encrypt_metadata) && (type == "/Metadata"))
	    {
		QTC::TC("qpdf", "QPDF_encryption cleartext metadata");
		method = e_none;
	    }
	    else
	    {
                if (this->attachment_streams.count(
                        QPDFObjGen(objid, generation)) > 0)
                {
                    method = this->cf_file;
                }
                else
                {
                    method = this->cf_stream;
                }
	    }
	}
	use_aes = false;
	switch (method)
	{
	  case e_none:
	    return;
	    break;

	  case e_aes:
	    use_aes = true;
	    break;

	  case e_aesv3:
	    use_aes = true;
	    break;

	  case e_rc4:
	    break;

	  default:
	    // filter local to this stream.
	    warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			 this->last_object_description,
			 this->file->getLastOffset(),
			 "unknown encryption filter for streams"
			 " (check " + method_source + ");"
			 " streams may be decrypted improperly"));
	    // To avoid repeated warnings, reset cf_stream.  Assume
	    // we'd want to use AES if V == 4.
	    this->cf_stream = e_aes;
	    break;
	}
    }
    std::string key = getKeyForObject(objid, generation, use_aes);
    if (use_aes)
    {
	QTC::TC("qpdf", "QPDF_encryption aes decode stream");
	pipeline = new Pl_AES_PDF("AES stream decryption", pipeline,
				  false, QUtil::unsigned_char_pointer(key),
                                  key.length());
    }
    else
    {
	QTC::TC("qpdf", "QPDF_encryption rc4 decode stream");
	pipeline = new Pl_RC4("RC4 stream decryption", pipeline,
			      QUtil::unsigned_char_pointer(key),
                              key.length());
    }
    heap.push_back(pipeline);
}

void
QPDF::compute_encryption_O_U(
    char const* user_password, char const* owner_password,
    int V, int R, int key_len, int P, bool encrypt_metadata,
    std::string const& id1, std::string& O, std::string& U)
{
    if (V >= 5)
    {
	throw std::logic_error(
	    "compute_encryption_O_U called for file with V >= 5");
    }
    EncryptionData data(V, R, key_len, P, "", "", "", "", "",
                        id1, encrypt_metadata);
    data.setO(compute_O_value(user_password, owner_password, data));
    O = data.getO();
    data.setU(compute_U_value(user_password, data));
    U = data.getU();
}

void
QPDF::compute_encryption_parameters_V5(
    char const* user_password, char const* owner_password,
    int V, int R, int key_len, int P, bool encrypt_metadata,
    std::string const& id1,
    std::string& encryption_key,
    std::string& O, std::string& U,
    std::string& OE, std::string& UE, std::string& Perms)
{
    EncryptionData data(V, R, key_len, P, "", "", "", "", "",
                        id1, encrypt_metadata);
    unsigned char k[key_bytes];
    QUtil::initializeWithRandomBytes(k, key_bytes);
    encryption_key = std::string(reinterpret_cast<char*>(k), key_bytes);
    compute_U_UE_value_V5(user_password, encryption_key, data, U, UE);
    compute_O_OE_value_V5(owner_password, encryption_key, data, U, O, OE);
    Perms = compute_Perms_value_V5(encryption_key, data);
    data.setV5EncryptionParameters(O, OE, U, UE, Perms);
}

std::string const&
QPDF::getPaddedUserPassword() const
{
    return this->user_password;
}

std::string
QPDF::getTrimmedUserPassword() const
{
    std::string result = this->user_password;
    trim_user_password(result);
    return result;
}

std::string
QPDF::getEncryptionKey() const
{
    return this->encryption_key;
}

bool
QPDF::isEncrypted() const
{
    return this->encrypted;
}

bool
QPDF::isEncrypted(int& R, int& P)
{
    int V;
    encryption_method_e stream, string, file;
    return isEncrypted(R, P, V, stream, string, file);
}

bool
QPDF::isEncrypted(int& R, int& P, int& V,
                  encryption_method_e& stream_method,
                  encryption_method_e& string_method,
                  encryption_method_e& file_method)
{
    if (this->encrypted)
    {
	QPDFObjectHandle trailer = getTrailer();
	QPDFObjectHandle encrypt = trailer.getKey("/Encrypt");
	QPDFObjectHandle Pkey = encrypt.getKey("/P");
	QPDFObjectHandle Rkey = encrypt.getKey("/R");
        QPDFObjectHandle Vkey = encrypt.getKey("/V");
	P = Pkey.getIntValue();
	R = Rkey.getIntValue();
        V = Vkey.getIntValue();
        stream_method = this->cf_stream;
        string_method = this->cf_stream;
        file_method = this->cf_file;
	return true;
    }
    else
    {
	return false;
    }
}

static bool
is_bit_set(int P, int bit)
{
    // Bits in P are numbered from 1 in the spec
    return (P & (1 << (bit - 1)));
}

bool
QPDF::allowAccessibility()
{
    int R = 0;
    int P = 0;
    bool status = true;
    if (isEncrypted(R, P))
    {
	if (R < 3)
	{
	    status = is_bit_set(P, 5);
	}
	else
	{
	    status = is_bit_set(P, 10);
	}
    }
    return status;
}

bool
QPDF::allowExtractAll()
{
    int R = 0;
    int P = 0;
    bool status = true;
    if (isEncrypted(R, P))
    {
	status = is_bit_set(P, 5);
    }
    return status;
}

bool
QPDF::allowPrintLowRes()
{
    int R = 0;
    int P = 0;
    bool status = true;
    if (isEncrypted(R, P))
    {
	status = is_bit_set(P, 3);
    }
    return status;
}

bool
QPDF::allowPrintHighRes()
{
    int R = 0;
    int P = 0;
    bool status = true;
    if (isEncrypted(R, P))
    {
	status = is_bit_set(P, 3);
	if ((R >= 3) && (! is_bit_set(P, 12)))
	{
	    status = false;
	}
    }
    return status;
}

bool
QPDF::allowModifyAssembly()
{
    int R = 0;
    int P = 0;
    bool status = true;
    if (isEncrypted(R, P))
    {
	if (R < 3)
	{
	    status = is_bit_set(P, 4);
	}
	else
	{
	    status = is_bit_set(P, 11);
	}
    }
    return status;
}

bool
QPDF::allowModifyForm()
{
    int R = 0;
    int P = 0;
    bool status = true;
    if (isEncrypted(R, P))
    {
	if (R < 3)
	{
	    status = is_bit_set(P, 6);
	}
	else
	{
	    status = is_bit_set(P, 9);
	}
    }
    return status;
}

bool
QPDF::allowModifyAnnotation()
{
    int R = 0;
    int P = 0;
    bool status = true;
    if (isEncrypted(R, P))
    {
	status = is_bit_set(P, 6);
    }
    return status;
}

bool
QPDF::allowModifyOther()
{
    int R = 0;
    int P = 0;
    bool status = true;
    if (isEncrypted(R, P))
    {
	status = is_bit_set(P, 4);
    }
    return status;
}

bool
QPDF::allowModifyAll()
{
    int R = 0;
    int P = 0;
    bool status = true;
    if (isEncrypted(R, P))
    {
	status = (is_bit_set(P, 4) && is_bit_set(P, 6));
	if (R >= 3)
	{
	    status = status && (is_bit_set(P, 9) && is_bit_set(P, 11));
	}
    }
    return status;
}
