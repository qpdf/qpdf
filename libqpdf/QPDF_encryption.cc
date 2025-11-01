// This file implements methods from the QPDF class that involve encryption.

#include <qpdf/QPDF_private.hh>

#include <qpdf/QPDFExc.hh>

#include <qpdf/MD5.hh>
#include <qpdf/Pl_AES_PDF.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_RC4.hh>
#include <qpdf/Pl_SHA2.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/RC4.hh>
#include <qpdf/Util.hh>

#include <algorithm>
#include <cstring>

using namespace qpdf;
using namespace std::literals;

using Encryption = QPDF::Doc::Encryption;

static std::string padding_string =
    "\x28\xbf\x4e\x5e\x4e\x75\x8a\x41\x64\x00\x4e\x56\xff\xfa\x01\x08"
    "\x2e\x2e\x00\xb6\xd0\x68\x3e\x80\x2f\x0c\xa9\xfe\x64\x53\x69\x7a"s;

static unsigned int const key_bytes = 32;

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

int
Encryption::getV() const
{
    return this->V;
}

int
Encryption::getR() const
{
    return this->R;
}

int
Encryption::getLengthBytes() const
{
    return this->Length_bytes;
}

int
Encryption::getP() const
{
    return static_cast<int>(P.to_ulong());
}

bool
Encryption::getP(size_t bit) const
{
    qpdf_assert_debug(bit);
    return P.test(bit - 1);
}

bool
QPDF::EncryptionParameters::P(size_t bit) const
{
    qpdf_assert_debug(bit);
    return P_.test(bit - 1);
}

std::string const&
Encryption::getO() const
{
    return this->O;
}

std::string const&
Encryption::getU() const
{
    return this->U;
}

std::string const&
Encryption::getOE() const
{
    return this->OE;
}

std::string const&
Encryption::getUE() const
{
    return this->UE;
}

std::string const&
Encryption::getPerms() const
{
    return this->Perms;
}

std::string const&
Encryption::getId1() const
{
    return this->id1;
}

bool
Encryption::getEncryptMetadata() const
{
    return this->encrypt_metadata;
}

void
Encryption::setO(std::string const& O)
{
    this->O = O;
}

void
Encryption::setU(std::string const& U)
{
    this->U = U;
}

void
Encryption::setP(size_t bit, bool val)
{
    qpdf_assert_debug(bit);
    P.set(bit - 1, val);
}

void
Encryption::setP(unsigned long val)
{
    P = std::bitset<32>(val);
}

void
Encryption::setId1(std::string const& val)
{
    id1 = val;
}

void
Encryption::setV5EncryptionParameters(
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

void
QPDF::trim_user_password(std::string& user_password)
{
    // Although unnecessary, this routine trims the padding string from the end of a user password.
    // Its only purpose is for recovery of user passwords which is done in the test suite.
    if (user_password.size() < key_bytes) {
        return;
    }

    auto idx = user_password.find('\x28');

    while (idx != user_password.npos) {
        if (padding_string.starts_with(user_password.substr(idx))) {
            user_password.resize(idx);
            return;
        }
        QTC::TC("qpdf", "QPDF_encryption skip 0x28");
        idx = user_password.find('\x28', ++idx);
    }
}

static std::string
pad_or_truncate_password_V4(std::string password)
{
    if (password.size() < key_bytes) {
        password.append(padding_string);
    }
    password.resize(key_bytes);
    return password;
}

static std::string
iterate_md5_digest(MD5& md5, int iterations, int key_len)
{
    MD5::Digest digest;
    md5.digest(digest);
    auto len = std::min(QIntC::to_size(key_len), sizeof(digest));
    for (int i = 0; i < iterations; ++i) {
        MD5 m;
        m.encodeDataIncrementally(reinterpret_cast<char*>(digest), len);
        m.digest(digest);
    }
    return {reinterpret_cast<char*>(digest), len};
}

static void
iterate_rc4(std::string& data, std::string_view okey, int iterations, bool reverse)
{
    auto len = okey.size();
    std::string key(len, '\0');
    for (int i = 0; i < iterations; ++i) {
        int const xor_value = (reverse ? iterations - 1 - i : i);
        for (size_t j = 0; j < len; ++j) {
            key[j] = static_cast<char>(okey[j] ^ xor_value);
        }
        RC4::process(key, data);
    }
}

static std::string
process_with_aes(
    std::string const& key,
    bool encrypt,
    std::string const& data,
    size_t outlength = 0,
    unsigned int repetitions = 1,
    unsigned char const* iv = nullptr,
    size_t iv_length = 0)
{
    Pl_Buffer buffer("buffer");
    Pl_AES_PDF aes("aes", &buffer, encrypt, key);
    if (iv) {
        aes.setIV(iv, iv_length);
    } else {
        aes.useZeroIV();
    }
    aes.disablePadding();
    for (unsigned int i = 0; i < repetitions; ++i) {
        aes.writeString(data);
    }
    aes.finish();
    if (outlength == 0) {
        return buffer.getString();
    } else {
        return buffer.getString().substr(0, outlength);
    }
}

std::string
Encryption::hash_V5(
    std::string const& password, std::string const& salt, std::string const& udata) const
{
    Pl_SHA2 hash(256);
    hash.writeString(password);
    hash.writeString(salt);
    hash.writeString(udata);
    hash.finish();
    std::string K = hash.getRawDigest();

    std::string result;
    if (getR() < 6) {
        result = K;
    } else {
        // Algorithm 2.B from ISO 32000-1 chapter 7: Computing a hash

        int round_number = 0;
        bool done = false;
        while (!done) {
            // The hash algorithm has us setting K initially to the R5 value and then repeating a
            // series of steps 64 times before starting with the termination case testing.  The
            // wording of the specification is very unclear as to the exact number of times it
            // should be run since the wording about whether the initial setup counts as round 0 or
            // not is ambiguous.  This code counts the initial setup (R5) value as round 0, which
            // appears to be correct.  This was determined to be correct by increasing or decreasing
            // the number of rounds by 1 or 2 from this value and generating 20 test files.  In this
            // interpretation, all the test files worked with Adobe Reader X.  In the other
            // configurations, many of the files did not work, and we were accurately able to
            // predict which files didn't work by looking at the conditions under which we
            // terminated repetition.

            ++round_number;
            std::string K1 = password + K + udata;
            qpdf_assert_debug(K.length() >= 32);
            std::string E = process_with_aes(
                K.substr(0, 16),
                true,
                K1,
                0,
                64,
                QUtil::unsigned_char_pointer(K.substr(16, 16)),
                16);

            // E_mod_3 is supposed to be mod 3 of the first 16 bytes of E taken as as a (128-bit)
            // big-endian number.  Since (xy mod n) is equal to ((x mod n) + (y mod n)) mod n and
            // since 256 mod n is 1, we can just take the sums of the the mod 3s of each byte to get
            // the same result.
            int E_mod_3 = 0;
            for (unsigned int i = 0; i < 16; ++i) {
                E_mod_3 += static_cast<unsigned char>(E.at(i));
            }
            E_mod_3 %= 3;
            int next_hash = ((E_mod_3 == 0) ? 256 : (E_mod_3 == 1) ? 384 : 512);
            Pl_SHA2 sha2(next_hash);
            sha2.writeString(E);
            sha2.finish();
            K = sha2.getRawDigest();

            if (round_number >= 64) {
                unsigned int ch = static_cast<unsigned char>(*(E.rbegin()));

                if (ch <= QIntC::to_uint(round_number - 32)) {
                    done = true;
                }
            }
        }
        result = K.substr(0, 32);
    }

    return result;
}

static void
pad_short_parameter(std::string& param, size_t max_len)
{
    if (param.length() < max_len) {
        QTC::TC("qpdf", "QPDF_encryption pad short parameter");
        param.append(max_len - param.length(), '\0');
    }
}

std::string
QPDF::compute_data_key(
    std::string const& encryption_key,
    int objid,
    int generation,
    bool use_aes,
    int encryption_V,
    int encryption_R)
{
    // Algorithm 3.1 from the PDF 1.7 Reference Manual

    std::string result = encryption_key;

    if (encryption_V >= 5) {
        // Algorithm 3.1a (PDF 1.7 extension level 3): just use encryption key straight.
        return result;
    }

    // Append low three bytes of object ID and low two bytes of generation
    result.append(1, static_cast<char>(objid & 0xff));
    result.append(1, static_cast<char>((objid >> 8) & 0xff));
    result.append(1, static_cast<char>((objid >> 16) & 0xff));
    result.append(1, static_cast<char>(generation & 0xff));
    result.append(1, static_cast<char>((generation >> 8) & 0xff));
    if (use_aes) {
        result += "sAlT";
    }
    return MD5::digest(result).substr(0, result.size());
}

std::string
QPDF::compute_encryption_key(std::string const& password, EncryptionData const& ed)
{
    return Encryption(
               ed.getV(),
               ed.getR(),
               ed.getLengthBytes(),
               ed.getP(),
               ed.getO(),
               ed.getU(),
               ed.getOE(),
               ed.getUE(),
               ed.getPerms(),
               ed.getId1(),
               ed.getEncryptMetadata())
        .compute_encryption_key(password);
}

std::string
Encryption::compute_encryption_key(std::string const& password) const
{
    if (getV() >= 5) {
        // For V >= 5, the encryption key is generated and stored in the file, encrypted separately
        // with both user and owner passwords.
        return recover_encryption_key_with_password(password);
    } else {
        // For V < 5, the encryption key is derived from the user
        // password.
        return compute_encryption_key_from_password(password);
    }
}

std::string
Encryption::compute_encryption_key_from_password(std::string const& password) const
{
    // Algorithm 3.2 from the PDF 1.7 Reference Manual

    // This code does not properly handle Unicode passwords. Passwords are supposed to be converted
    // from OS codepage characters to PDFDocEncoding.  Unicode passwords are supposed to be
    // converted to OS codepage before converting to PDFDocEncoding.  We instead require the
    // password to be presented in its final form.

    MD5 md5;
    md5.encodeDataIncrementally(pad_or_truncate_password_V4(password));
    md5.encodeDataIncrementally(getO());
    char pbytes[4];
    int p = getP();
    pbytes[0] = static_cast<char>(p & 0xff);
    pbytes[1] = static_cast<char>((p >> 8) & 0xff);
    pbytes[2] = static_cast<char>((p >> 16) & 0xff);
    pbytes[3] = static_cast<char>((p >> 24) & 0xff);
    md5.encodeDataIncrementally(pbytes, 4);
    md5.encodeDataIncrementally(getId1());
    if (getR() >= 4 && !getEncryptMetadata()) {
        md5.encodeDataIncrementally("\xff\xff\xff\xff");
    }
    return iterate_md5_digest(md5, (getR() >= 3 ? 50 : 0), getLengthBytes());
}

std::string
Encryption::compute_O_rc4_key(
    std::string const& user_password, std::string const& owner_password) const
{
    if (getV() >= 5) {
        throw std::logic_error("compute_O_rc4_key called for file with V >= 5");
    }
    std::string password = owner_password.empty() ? user_password : owner_password;
    MD5 md5;
    md5.encodeDataIncrementally(pad_or_truncate_password_V4(password));
    return iterate_md5_digest(md5, (getR() >= 3 ? 50 : 0), getLengthBytes());
}

std::string
Encryption::compute_O_value(
    std::string const& user_password, std::string const& owner_password) const
{
    // Algorithm 3.3 from the PDF 1.7 Reference Manual

    auto upass = pad_or_truncate_password_V4(user_password);
    std::string O_key = compute_O_rc4_key(user_password, owner_password);
    pad_short_parameter(O_key, QIntC::to_size(getLengthBytes()));
    iterate_rc4(upass, O_key, getR() >= 3 ? 20 : 1, false);
    return upass;
}

std::string
Encryption::compute_U_value_R2(std::string const& user_password) const
{
    // Algorithm 3.4 from the PDF 1.7 Reference Manual

    std::string k1 = compute_encryption_key(user_password);
    auto udata = padding_string;
    pad_short_parameter(k1, QIntC::to_size(getLengthBytes()));
    iterate_rc4(udata, k1, 1, false);
    return udata;
}

std::string
Encryption::compute_U_value_R3(std::string const& user_password) const
{
    // Algorithm 3.5 from the PDF 1.7 Reference Manual

    std::string k1 = compute_encryption_key(user_password);
    MD5 md5;
    md5.encodeDataIncrementally(padding_string);
    md5.encodeDataIncrementally(getId1());
    auto result = md5.digest();
    pad_short_parameter(k1, QIntC::to_size(getLengthBytes()));
    iterate_rc4(result, k1, 20, false);
    // pad with arbitrary data -- make it consistent for the sake of testing
    result += "\x0\x21\x44\x69\x90\xb9\xe4\x11\x40\x71\xa4\xd9\x10\x49\x84\xc1"s;
    return result;
}

std::string
Encryption::compute_U_value(std::string const& user_password) const
{
    if (getR() >= 3) {
        return compute_U_value_R3(user_password);
    }

    return compute_U_value_R2(user_password);
}

bool
Encryption::check_user_password_V4(std::string const& user_password) const
{
    // Algorithm 3.6 from the PDF 1.7 Reference Manual

    std::string u_value = compute_U_value(user_password);
    size_t to_compare = (getR() >= 3 ? sizeof(MD5::Digest) : key_bytes);
    return memcmp(getU().c_str(), u_value.c_str(), to_compare) == 0;
}

bool
Encryption::check_user_password_V5(std::string const& user_password) const
{
    // Algorithm 3.11 from the PDF 1.7 extension level 3

    std::string user_data = getU().substr(0, 32);
    std::string validation_salt = getU().substr(32, 8);
    std::string password = user_password.substr(0, 127);
    return hash_V5(user_password.substr(0, 127), validation_salt, "") == user_data;
}

bool
Encryption::check_user_password(std::string const& user_password) const
{
    if (getV() < 5) {
        return check_user_password_V4(user_password);
    } else {
        return check_user_password_V5(user_password);
    }
}

bool
Encryption::check_owner_password_V4(
    std::string& user_password, std::string const& owner_password) const
{
    // Algorithm 3.7 from the PDF 1.7 Reference Manual

    auto key = compute_O_rc4_key(user_password, owner_password);
    pad_short_parameter(key, QIntC::to_size(getLengthBytes()));
    auto new_user_password = O.substr(0, key_bytes);
    iterate_rc4(new_user_password, key, (getR() >= 3) ? 20 : 1, true);
    if (check_user_password(new_user_password)) {
        user_password = new_user_password;
        return true;
    }
    return false;
}

bool
Encryption::check_owner_password_V5(std::string const& owner_password) const
{
    // Algorithm 3.12 from the PDF 1.7 extension level 3

    std::string user_data = getU().substr(0, 48);
    std::string owner_data = getO().substr(0, 32);
    std::string validation_salt = getO().substr(32, 8);
    return hash_V5(owner_password.substr(0, 127), validation_salt, user_data) == owner_data;
}

bool
Encryption::check_owner_password(
    std::string& user_password, std::string const& owner_password) const
{
    if (getV() < 5) {
        return check_owner_password_V4(user_password, owner_password);
    } else {
        return check_owner_password_V5(owner_password);
    }
}

std::string
Encryption::recover_encryption_key_with_password(std::string const& password) const
{
    // Disregard whether Perms is valid.
    bool disregard;
    return recover_encryption_key_with_password(password, disregard);
}

std::string
Encryption::compute_Perms_value_V5_clear() const
{
    // From algorithm 3.10 from the PDF 1.7 extension level 3
    std::string k = "    \xff\xff\xff\xffTadb    ";
    int perms = getP();
    for (size_t i = 0; i < 4; ++i) {
        k[i] = static_cast<char>(perms & 0xff);
        perms >>= 8;
    }
    if (!getEncryptMetadata()) {
        k[8] = 'F';
    }
    QUtil::initializeWithRandomBytes(reinterpret_cast<unsigned char*>(&k[12]), 4);
    return k;
}

std::string
Encryption::recover_encryption_key_with_password(
    std::string const& password, bool& perms_valid) const
{
    // Algorithm 3.2a from the PDF 1.7 extension level 3

    // This code does not handle Unicode passwords correctly. Empirical evidence suggests that most
    // viewers don't.  We are supposed to process the input string with the SASLprep (RFC 4013)
    // profile of stringprep (RFC 3454) and then convert the result to UTF-8.

    perms_valid = false;
    std::string key_password = password.substr(0, 127);
    std::string key_salt;
    std::string user_data;
    std::string encrypted_file_key;
    if (check_owner_password_V5(key_password)) {
        key_salt = getO().substr(40, 8);
        user_data = getU().substr(0, 48);
        encrypted_file_key = getOE().substr(0, 32);
    } else if (check_user_password_V5(key_password)) {
        key_salt = getU().substr(40, 8);
        encrypted_file_key = getUE().substr(0, 32);
    }
    std::string intermediate_key = hash_V5(key_password, key_salt, user_data);
    std::string file_key = process_with_aes(intermediate_key, false, encrypted_file_key);

    // Decrypt Perms and check against expected value
    auto perms_check = process_with_aes(file_key, false, getPerms()).substr(0, 12);
    perms_valid = compute_Perms_value_V5_clear().substr(0, 12) == perms_check;
    return file_key;
}

QPDF::encryption_method_e
QPDF::EncryptionParameters::interpretCF(Name const& cf) const
{
    if (!cf) {
        // Default: /Identity
        return e_none;
    }
    auto it = crypt_filters.find(cf);
    if (it != crypt_filters.end()) {
        return it->second;
    }
    if (cf == "/Identity") {
        return e_none;
    }
    return e_unknown;
}

void
QPDF::initializeEncryption()
{
    m->encp->initialize(*this);
}

void
QPDF::EncryptionParameters::initialize(QPDF& qpdf)
{
    if (encryption_initialized) {
        return;
    }
    encryption_initialized = true;

    auto& c = qpdf.m->c;
    auto& qm = *qpdf.m;
    auto& trailer = qm.trailer;
    auto& file = qm.file;

    auto warn_damaged_pdf = [&qpdf, c](std::string const& msg) {
        qpdf.warn(c.damagedPDF("encryption dictionary", msg));
    };
    auto throw_damaged_pdf = [&qpdf](std::string const& msg) {
        throw qpdf.m->c.damagedPDF("encryption dictionary", msg);
    };
    auto unsupported = [&file](std::string const& msg) -> QPDFExc {
        return {
            qpdf_e_unsupported,
            file->getName(),
            "encryption dictionary",
            file->getLastOffset(),
            msg};
    };

    // After we initialize encryption parameters, we must use stored key information and never look
    // at /Encrypt again.  Otherwise, things could go wrong if someone mutates the encryption
    // dictionary.

    if (!trailer.contains("/Encrypt")) {
        return;
    }

    // Go ahead and set m->encrypted here.  That way, isEncrypted will return true even if there
    // were errors reading the encryption dictionary.
    encrypted = true;

    std::string id1;
    auto id_obj = trailer.getKey("/ID");
    if (id_obj.size() != 2 || !id_obj.getArrayItem(0).isString()) {
        // Treating a missing ID as the empty string enables qpdf to decrypt some invalid encrypted
        // files with no /ID that poppler can read but Adobe Reader can't.
        qpdf.warn(qpdf.m->c.damagedPDF("trailer", "invalid /ID in trailer dictionary"));
    } else {
        id1 = id_obj.getArrayItem(0).getStringValue();
    }

    auto encryption_dict = trailer.getKey("/Encrypt");
    if (!encryption_dict.isDictionary()) {
        throw qpdf.m->c.damagedPDF("/Encrypt in trailer dictionary is not a dictionary");
    }

    if (Name(encryption_dict["/Filter"]) != "/Standard") {
        throw unsupported("unsupported encryption filter");
    }
    if (!encryption_dict.getKey("/SubFilter").null()) {
        qpdf.warn(unsupported("file uses encryption SubFilters, which qpdf does not support"));
    }

    if (!(encryption_dict.getKey("/V").isInteger() && encryption_dict.getKey("/R").isInteger() &&
          encryption_dict.getKey("/O").isString() && encryption_dict.getKey("/U").isString() &&
          encryption_dict.getKey("/P").isInteger())) {
        throw_damaged_pdf("some encryption dictionary parameters are missing or the wrong type");
    }

    int V = encryption_dict.getKey("/V").getIntValueAsInt();
    int R = encryption_dict.getKey("/R").getIntValueAsInt();
    std::string O = encryption_dict.getKey("/O").getStringValue();
    std::string U = encryption_dict.getKey("/U").getStringValue();
    int p = static_cast<int>(encryption_dict.getKey("/P").getIntValue());

    // If supporting new encryption R/V values, remember to update error message inside this if
    // statement.
    if (!(2 <= R && R <= 6 && (V == 1 || V == 2 || V == 4 || V == 5))) {
        throw unsupported(
            "Unsupported /R or /V in encryption dictionary; R = " + std::to_string(R) +
            " (max 6), V = " + std::to_string(V) + " (max 5)");
    }

    P_ = std::bitset<32>(static_cast<unsigned long long>(p));
    encryption_V = V;
    R_ = R;

    // OE, UE, and Perms are only present if V >= 5.
    std::string OE;
    std::string UE;
    std::string Perms;

    if (V < 5) {
        // These must be exactly the right number of bytes.
        pad_short_parameter(O, key_bytes);
        pad_short_parameter(U, key_bytes);
        if (!(O.length() == key_bytes && U.length() == key_bytes)) {
            throw_damaged_pdf("incorrect length for /O and/or /U in encryption dictionary");
        }
    } else {
        if (!(encryption_dict.getKey("/OE").isString() &&
              encryption_dict.getKey("/UE").isString() &&
              encryption_dict.getKey("/Perms").isString())) {
            throw_damaged_pdf(
                "some V=5 encryption dictionary parameters are missing or the wrong type");
        }
        OE = encryption_dict.getKey("/OE").getStringValue();
        UE = encryption_dict.getKey("/UE").getStringValue();
        Perms = encryption_dict.getKey("/Perms").getStringValue();

        // These may be longer than the minimum number of bytes.
        pad_short_parameter(O, OU_key_bytes_V5);
        pad_short_parameter(U, OU_key_bytes_V5);
        pad_short_parameter(OE, OUE_key_bytes_V5);
        pad_short_parameter(UE, OUE_key_bytes_V5);
        pad_short_parameter(Perms, Perms_key_bytes_V5);
    }

    int Length = 128; // Just take a guess.
    if (V <= 1) {
        Length = 40;
    } else if (V == 4) {
        Length = 128;
    } else if (V == 5) {
        Length = 256;
    } else {
        if (encryption_dict.getKey("/Length").isInteger()) {
            Length = encryption_dict.getKey("/Length").getIntValueAsInt();
            if (Length % 8 || Length < 40 || Length > 128) {
                Length = 128; // Just take a guess.
            }
        }
    }

    encrypt_metadata = true;
    if (V >= 4 && encryption_dict.getKey("/EncryptMetadata").isBool()) {
        encrypt_metadata = encryption_dict.getKey("/EncryptMetadata").getBoolValue();
    }

    if (V == 4 || V == 5) {
        auto CF = encryption_dict.getKey("/CF");
        for (auto const& [filter, cdict]: CF.as_dictionary()) {
            if (cdict.isDictionary()) {
                encryption_method_e method = e_none;
                if (Name const& CFM = cdict["/CFM"]) {
                    if (CFM == "/V2") {
                        method = e_rc4;
                    } else if (CFM == "/AESV2") {
                        method = e_aes;
                    } else if (CFM == "/AESV3") {
                        method = e_aesv3;
                    } else {
                        // Don't complain now -- maybe we won't need to reference this type.
                        method = e_unknown;
                    }
                }
                crypt_filters[filter] = method;
            }
        }

        cf_stream = interpretCF(encryption_dict["/StmF"]);
        cf_string = interpretCF(encryption_dict["/StrF"]);
        if (Name const& EFF = encryption_dict["/EFF"]) {
            // qpdf does not use this for anything other than informational purposes. This is
            // intended to instruct conforming writers on which crypt filter should be used when new
            // file attachments are added to a PDF file, but qpdf never generates encrypted files
            // with non-default crypt filters. Prior to 10.2, I was under the mistaken impression
            // that this was supposed to be used for decrypting attachments, but the code was wrong
            // in a way that turns out not to have mattered because no writers were generating files
            // the way I was imagining. Still, providing this information could be useful when
            // looking at a file generated by something else, such as Acrobat when specifying that
            // only attachments should be encrypted.
            cf_file = interpretCF(EFF);
        } else {
            cf_file = cf_stream;
        }
    }

    Encryption data(V, R, Length / 8, p, O, U, OE, UE, Perms, id1, encrypt_metadata);
    if (qm.cf.password_is_hex_key()) {
        // ignore passwords in file
        encryption_key = QUtil::hex_decode(provided_password);
        return;
    }

    owner_password_matched = data.check_owner_password(user_password, provided_password);
    if (owner_password_matched && V < 5) {
        // password supplied was owner password; user_password has been initialized for V < 5
        if (qpdf.getTrimmedUserPassword() == provided_password) {
            user_password_matched = true;
            QTC::TC("qpdf", "QPDF_encryption user matches owner V < 5");
        }
    } else {
        user_password_matched = data.check_user_password(provided_password);
        if (user_password_matched) {
            user_password = provided_password;
        }
    }
    if (user_password_matched && owner_password_matched) {
        QTC::TC("qpdf", "QPDF_encryption same password", (V < 5) ? 0 : 1);
    }
    if (!(owner_password_matched || user_password_matched)) {
        throw QPDFExc(qpdf_e_password, file->getName(), "", 0, "invalid password");
    }

    if (V < 5) {
        // For V < 5, the user password is encrypted with the owner password, and the user password
        // is always used for computing the encryption key.
        encryption_key = data.compute_encryption_key(user_password);
    } else {
        // For V >= 5, either password can be used independently to compute the encryption key, and
        // neither password can be used to recover the other.
        bool perms_valid;
        encryption_key = data.recover_encryption_key_with_password(provided_password, perms_valid);
        if (!perms_valid) {
            warn_damaged_pdf("/Perms field in encryption dictionary doesn't match expected value");
        }
    }
}

std::string
QPDF::getKeyForObject(std::shared_ptr<EncryptionParameters> encp, QPDFObjGen og, bool use_aes)
{
    if (!encp->encrypted) {
        throw std::logic_error("request for encryption key in non-encrypted PDF");
    }

    if (og != encp->cached_key_og) {
        encp->cached_object_encryption_key = compute_data_key(
            encp->encryption_key, og.getObj(), og.getGen(), use_aes, encp->encryption_V, encp->R());
        encp->cached_key_og = og;
    }

    return encp->cached_object_encryption_key;
}

void
QPDF::decryptString(std::string& str, QPDFObjGen og)
{
    if (!og.isIndirect()) {
        return;
    }
    bool use_aes = false;
    if (m->encp->encryption_V >= 4) {
        switch (m->encp->cf_string) {
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
            warn(m->c.damagedPDF(
                "unknown encryption filter for strings (check /StrF in "
                "/Encrypt dictionary); strings may be decrypted improperly"));
            // To avoid repeated warnings, reset cf_string.  Assume we'd want to use AES if V == 4.
            m->encp->cf_string = e_aes;
            use_aes = true;
            break;
        }
    }

    std::string key = getKeyForObject(m->encp, og, use_aes);
    try {
        if (use_aes) {
            QTC::TC("qpdf", "QPDF_encryption aes decode string");
            Pl_Buffer bufpl("decrypted string");
            Pl_AES_PDF pl("aes decrypt string", &bufpl, false, key);
            pl.writeString(str);
            pl.finish();
            str = bufpl.getString();
        } else {
            QTC::TC("qpdf", "QPDF_encryption rc4 decode string");
            size_t vlen = str.length();
            // Using std::shared_ptr guarantees that tmp will be freed even if rc4.process throws an
            // exception.
            auto tmp = QUtil::make_unique_cstr(str);
            RC4 rc4(QUtil::unsigned_char_pointer(key), QIntC::to_int(key.length()));
            auto data = QUtil::unsigned_char_pointer(tmp.get());
            rc4.process(data, vlen, data);
            str = std::string(tmp.get(), vlen);
        }
    } catch (QPDFExc&) {
        throw;
    } catch (std::runtime_error& e) {
        throw m->c.damagedPDF(
            "error decrypting string for object " + og.unparse() + ": " + e.what());
    }
}

// Prepend a decryption pipeline to 'pipeline'. The decryption pipeline (returned as
// 'decrypt_pipeline' must be owned by the caller to ensure that it stays alive while the pipeline
// is in use.
void
QPDF::decryptStream(
    std::shared_ptr<EncryptionParameters> encp,
    std::shared_ptr<InputSource> file,
    QPDF& qpdf_for_warning,
    Pipeline*& pipeline,
    QPDFObjGen og,
    QPDFObjectHandle& stream_dict,
    bool is_root_metadata,
    std::unique_ptr<Pipeline>& decrypt_pipeline)
{
    if (Name(stream_dict["/Type"]) == "/XRef") {
        return;
    }
    bool use_aes = false;
    if (encp->encryption_V >= 4) {
        encryption_method_e method = e_unknown;
        std::string method_source = "/StmF from /Encrypt dictionary";

        if (stream_dict.getKey("/Filter").isOrHasName("/Crypt")) {
            if (Dictionary decode_parms = stream_dict["/DecodeParms"]) {
                if (Name(decode_parms["/Type"]) == "/CryptFilterDecodeParms") {
                    method = encp->interpretCF(decode_parms["/Name"]);
                    method_source = "stream's Crypt decode parameters";
                }
            } else {
                Array filter = stream_dict["/Filter"];
                Array decode = stream_dict.getKey("/DecodeParms");
                if (filter.size() == decode.size()) {
                    size_t i = 0;
                    for (Name item: filter) {
                        if (item == "/Crypt") {
                            if (Name name = decode[i]["/Name"]) {
                                method = encp->interpretCF(name);
                                method_source = "stream's Crypt decode parameters (array)";
                            }
                            break;
                        }
                        ++i;
                    }
                }
            }
        }

        if (method == e_unknown) {
            if (!encp->encrypt_metadata && is_root_metadata) {
                method = e_none;
            } else {
                method = encp->cf_stream;
            }
        }
        use_aes = false;
        switch (method) {
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
            qpdf_for_warning.warn(
                {qpdf_e_damaged_pdf,
                 file->getName(),
                 "",
                 file->getLastOffset(),
                 "unknown encryption filter for streams (check " + method_source +
                     "); streams may be decrypted improperly"});
            // To avoid repeated warnings, reset cf_stream.  Assume we'd want to use AES if V == 4.
            encp->cf_stream = e_aes;
            use_aes = true;
            break;
        }
    }
    std::string key = getKeyForObject(encp, og, use_aes);
    if (use_aes) {
        decrypt_pipeline =
            std::make_unique<Pl_AES_PDF>("AES stream decryption", pipeline, false, key);
    } else {
        decrypt_pipeline = std::make_unique<Pl_RC4>("RC4 stream decryption", pipeline, key);
    }
    pipeline = decrypt_pipeline.get();
}

void
QPDF::compute_encryption_O_U(
    char const* user_password,
    char const* owner_password,
    int V,
    int R,
    int key_len,
    int P,
    bool encrypt_metadata,
    std::string const& id1,
    std::string& out_O,
    std::string& out_U)
{
    Encryption data(V, R, key_len, P, "", "", "", "", "", id1, encrypt_metadata);
    data.compute_encryption_O_U(user_password, owner_password);
    out_O = data.getO();
    out_U = data.getU();
}

void
Encryption::compute_encryption_O_U(char const* user_password, char const* owner_password)
{
    if (V >= 5) {
        throw std::logic_error("compute_encryption_O_U called for file with V >= 5");
    }
    O = compute_O_value(user_password, owner_password);
    U = compute_U_value(user_password);
}

void
QPDF::compute_encryption_parameters_V5(
    char const* user_password,
    char const* owner_password,
    int V,
    int R,
    int key_len,
    int P,
    bool encrypt_metadata,
    std::string const& id1,
    std::string& encryption_key,
    std::string& out_O,
    std::string& out_U,
    std::string& out_OE,
    std::string& out_UE,
    std::string& out_Perms)
{
    Encryption data(V, R, key_len, P, "", "", "", "", "", id1, encrypt_metadata);
    encryption_key = data.compute_encryption_parameters_V5(user_password, owner_password);

    out_O = data.getO();
    out_U = data.getU();
    out_OE = data.getOE();
    out_UE = data.getUE();
    out_Perms = data.getPerms();
}

std::string
Encryption::compute_encryption_parameters_V5(char const* user_password, char const* owner_password)
{
    auto out_encryption_key = util::random_string(key_bytes);
    // Algorithm 8 from the PDF 2.0
    auto validation_salt = util::random_string(8);
    auto key_salt = util::random_string(8);
    U = hash_V5(user_password, validation_salt, "").append(validation_salt).append(key_salt);
    auto intermediate_key = hash_V5(user_password, key_salt, "");
    UE = process_with_aes(intermediate_key, true, out_encryption_key);
    // Algorithm 9 from the PDF 2.0
    validation_salt = util::random_string(8);
    key_salt = util::random_string(8);
    O = hash_V5(owner_password, validation_salt, U) + validation_salt + key_salt;
    intermediate_key = hash_V5(owner_password, key_salt, U);
    OE = process_with_aes(intermediate_key, true, out_encryption_key);
    // Algorithm 10 from the PDF 2.0
    Perms = process_with_aes(out_encryption_key, true, compute_Perms_value_V5_clear());
    return out_encryption_key;
}

std::string
Encryption::compute_parameters(char const* user_password, char const* owner_password)
{
    if (V < 5) {
        compute_encryption_O_U(user_password, owner_password);
        return compute_encryption_key(user_password);
    } else {
        return compute_encryption_parameters_V5(user_password, owner_password);
    }
}

std::string const&
QPDF::getPaddedUserPassword() const
{
    return m->encp->user_password;
}

std::string
QPDF::getTrimmedUserPassword() const
{
    std::string result = m->encp->user_password;
    trim_user_password(result);
    return result;
}

std::string
QPDF::getEncryptionKey() const
{
    return m->encp->encryption_key;
}

bool
QPDF::isEncrypted() const
{
    return m->encp->encrypted;
}

bool
QPDF::isEncrypted(int& R, int& P)
{
    if (!m->encp->encrypted) {
        return false;
    }
    P = m->encp->P();
    R = m->encp->R();
    return true;
}

bool
QPDF::isEncrypted(
    int& R,
    int& P,
    int& V,
    encryption_method_e& stream_method,
    encryption_method_e& string_method,
    encryption_method_e& file_method)
{
    if (!m->encp->encrypted) {
        return false;
    }
    P = m->encp->P();
    R = m->encp->R();
    V = m->encp->encryption_V;
    stream_method = m->encp->cf_stream;
    string_method = m->encp->cf_string;
    file_method = m->encp->cf_file;
    return true;
}

bool
QPDF::ownerPasswordMatched() const
{
    return m->encp->owner_password_matched;
}

bool
QPDF::userPasswordMatched() const
{
    return m->encp->user_password_matched;
}

bool
QPDF::allowAccessibility()
{
    return m->encp->R() < 3 ? m->encp->P(5) : m->encp->P(10);
}

bool
QPDF::allowExtractAll()
{
    return m->encp->P(5);
}

bool
QPDF::allowPrintLowRes()
{
    return m->encp->P(3);
}

bool
QPDF::allowPrintHighRes()
{
    return allowPrintLowRes() && (m->encp->R() < 3 ? true : m->encp->P(12));
}

bool
QPDF::allowModifyAssembly()
{
    return m->encp->R() < 3 ? m->encp->P(4) : m->encp->P(11);
}

bool
QPDF::allowModifyForm()
{
    return m->encp->R() < 3 ? m->encp->P(6) : m->encp->P(9);
}

bool
QPDF::allowModifyAnnotation()
{
    return m->encp->P(6);
}

bool
QPDF::allowModifyOther()
{
    return m->encp->P(4);
}

bool
QPDF::allowModifyAll()
{
    return allowModifyAnnotation() && allowModifyOther() &&
        (m->encp->R() < 3 ? true : allowModifyForm() && allowModifyAssembly());
}
