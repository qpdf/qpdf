// This file implements methods from the QPDF class that involve
// encryption.

#include <qpdf/QPDF.hh>

#include <qpdf/QPDFExc.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/Pl_RC4.hh>
#include <qpdf/RC4.hh>
#include <qpdf/MD5.hh>

#include <string.h>

static char const padding_string[] = {
    0x28, 0xbf, 0x4e, 0x5e, 0x4e, 0x75, 0x8a, 0x41,
    0x64, 0x00, 0x4e, 0x56, 0xff, 0xfa, 0x01, 0x08,
    0x2e, 0x2e, 0x00, 0xb6, 0xd0, 0x68, 0x3e, 0x80,
    0x2f, 0x0c, 0xa9, 0xfe, 0x64, 0x53, 0x69, 0x7a
};

static unsigned int const O_key_bytes = sizeof(MD5::Digest);
static unsigned int const id_bytes = 16;
static unsigned int const key_bytes = 32;

void
pad_or_truncate_password(std::string const& password, char k1[key_bytes])
{
    int password_bytes = std::min((size_t) key_bytes, password.length());
    int pad_bytes = key_bytes - password_bytes;
    memcpy(k1, password.c_str(), password_bytes);
    memcpy(k1 + password_bytes, padding_string, pad_bytes);
}

DLL_EXPORT
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

    char const* p = 0;
    while ((p = strchr(cstr, '\x28')) != 0)
    {
	if (memcmp(p, padding_string, len - (p - cstr)) == 0)
	{
	    user_password = user_password.substr(0, p - cstr);
	    return;
	}
    }
}

static std::string
pad_or_truncate_password(std::string const& password)
{
    char k1[key_bytes];
    pad_or_truncate_password(password, k1);
    return std::string(k1, key_bytes);
}

static void
iterate_md5_digest(MD5& md5, MD5::Digest& digest, int iterations)
{
    md5.digest(digest);

    for (int i = 0; i < iterations; ++i)
    {
	MD5 m;
	m.encodeDataIncrementally((char*)digest, sizeof(digest));
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

DLL_EXPORT
std::string
QPDF::compute_data_key(std::string const& encryption_key,
		       int objid, int generation)
{
    // Algorithm 3.1 from the PDF 1.4 Reference Manual

    std::string result = encryption_key;

    // Append low three bytes of object ID and low two bytes of generation
    result += (char) (objid & 0xff);
    result += (char) ((objid >> 8) & 0xff);
    result += (char) ((objid >> 16) & 0xff);
    result += (char) (generation & 0xff);
    result += (char) ((generation >> 8) & 0xff);

    MD5 md5;
    md5.encodeDataIncrementally(result.c_str(), result.length());
    MD5::Digest digest;
    md5.digest(digest);
    return std::string((char*) digest,
		       std::min(result.length(), (size_t) 16));
}

DLL_EXPORT
std::string
QPDF::compute_encryption_key(
    std::string const& password, EncryptionData const& data)
{
    // Algorithm 3.2 from the PDF 1.4 Reference Manual

    MD5 md5;
    md5.encodeDataIncrementally(
	pad_or_truncate_password(password).c_str(), key_bytes);
    md5.encodeDataIncrementally(data.O.c_str(), key_bytes);
    char pbytes[4];
    pbytes[0] = (char) (data.P & 0xff);
    pbytes[1] = (char) ((data.P >> 8) & 0xff);
    pbytes[2] = (char) ((data.P >> 16) & 0xff);
    pbytes[3] = (char) ((data.P >> 24) & 0xff);
    md5.encodeDataIncrementally(pbytes, 4);
    md5.encodeDataIncrementally(data.id1.c_str(), id_bytes);
    MD5::Digest digest;
    iterate_md5_digest(md5, digest, ((data.R == 3) ? 50 : 0));
    return std::string((char*)digest, data.Length_bytes);
}

static void
compute_O_rc4_key(std::string const& user_password,
		  std::string const& owner_password,
		  QPDF::EncryptionData const& data,
		  unsigned char key[O_key_bytes])
{
    std::string password = owner_password;
    if (password.empty())
    {
	password = user_password;
    }
    MD5 md5;
    md5.encodeDataIncrementally(
	pad_or_truncate_password(password).c_str(), key_bytes);
    MD5::Digest digest;
    iterate_md5_digest(md5, digest, ((data.R == 3) ? 50 : 0));
    memcpy(key, digest, O_key_bytes);
}

static std::string
compute_O_value(std::string const& user_password,
		std::string const& owner_password,
		QPDF::EncryptionData const& data)
{
    // Algorithm 3.3 from the PDF 1.4 Reference Manual

    unsigned char O_key[O_key_bytes];
    compute_O_rc4_key(user_password, owner_password, data, O_key);

    char upass[key_bytes];
    pad_or_truncate_password(user_password, upass);
    iterate_rc4((unsigned char*) upass, key_bytes,
		O_key, data.Length_bytes, (data.R == 3) ? 20 : 1, false);
    return std::string(upass, key_bytes);
}

static
std::string
compute_U_value_R2(std::string const& user_password,
		   QPDF::EncryptionData const& data)
{
    // Algorithm 3.4 from the PDF 1.4 Reference Manual

    std::string k1 = QPDF::compute_encryption_key(user_password, data);
    char udata[key_bytes];
    pad_or_truncate_password("", udata);
    iterate_rc4((unsigned char*) udata, key_bytes,
		(unsigned char*)k1.c_str(), data.Length_bytes, 1, false);
    return std::string(udata, key_bytes);
}

static
std::string
compute_U_value_R3(std::string const& user_password,
		   QPDF::EncryptionData const& data)
{
    // Algorithm 3.5 from the PDF 1.4 Reference Manual

    std::string k1 = QPDF::compute_encryption_key(user_password, data);
    MD5 md5;
    md5.encodeDataIncrementally(
	pad_or_truncate_password("").c_str(), key_bytes);
    md5.encodeDataIncrementally(data.id1.c_str(), data.id1.length());
    MD5::Digest digest;
    md5.digest(digest);
    iterate_rc4(digest, sizeof(MD5::Digest),
		(unsigned char*) k1.c_str(), data.Length_bytes, 20, false);
    char result[key_bytes];
    memcpy(result, digest, sizeof(MD5::Digest));
    // pad with arbitrary data -- make it consistent for the sake of
    // testing
    for (unsigned int i = sizeof(MD5::Digest); i < key_bytes; ++i)
    {
	result[i] = (char)((i * i) % 0xff);
    }
    return std::string(result, key_bytes);
}

static std::string
compute_U_value(std::string const& user_password,
		QPDF::EncryptionData const& data)
{
    if (data.R == 3)
    {
	return compute_U_value_R3(user_password, data);
    }

    return compute_U_value_R2(user_password, data);
}

static bool
check_user_password(std::string const& user_password,
		    QPDF::EncryptionData const& data)
{
    // Algorithm 3.6 from the PDF 1.4 Reference Manual

    std::string u_value = compute_U_value(user_password, data);
    int to_compare = ((data.R == 3) ? sizeof(MD5::Digest) : key_bytes);
    return (memcmp(data.U.c_str(), u_value.c_str(), to_compare) == 0);
}

static bool
check_owner_password(std::string& user_password,
		     std::string const& owner_password,
		     QPDF::EncryptionData const& data)
{
    // Algorithm 3.7 from the PDF 1.4 Reference Manual

    unsigned char key[O_key_bytes];
    compute_O_rc4_key(user_password, owner_password, data, key);
    unsigned char O_data[key_bytes];
    memcpy(O_data, (unsigned char*) data.O.c_str(), key_bytes);
    iterate_rc4(O_data, key_bytes, key, data.Length_bytes,
		(data.R == 3) ? 20 : 1, true);
    std::string new_user_password =
	std::string((char*)O_data, key_bytes);
    bool result = false;
    if (check_user_password(new_user_password, data))
    {
	result = true;
	user_password = new_user_password;
    }
    return result;
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

    QPDFObjectHandle id_obj = this->trailer.getKey("/ID");
    if (! (id_obj.isArray() &&
	   (id_obj.getArrayNItems() == 2) &&
	   id_obj.getArrayItem(0).isString()))
    {
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "invalid /ID in trailer dictionary");
    }

    std::string id1 = id_obj.getArrayItem(0).getStringValue();
    if (id1.length() != id_bytes)
    {
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "first /ID string in trailer dictionary has "
		      "incorrect length");
    }

    QPDFObjectHandle encryption_dict = this->trailer.getKey("/Encrypt");
    if (! encryption_dict.isDictionary())
    {
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "/Encrypt in trailer dictionary is not a dictionary");
    }

    if (! (encryption_dict.getKey("/Filter").isName() &&
	   (encryption_dict.getKey("/Filter").getName() == "/Standard")))
    {
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "unsupported encryption filter");
    }

    if (! (encryption_dict.getKey("/V").isInteger() &&
	   encryption_dict.getKey("/R").isInteger() &&
	   encryption_dict.getKey("/O").isString() &&
	   encryption_dict.getKey("/U").isString() &&
	   encryption_dict.getKey("/P").isInteger()))
    {
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "some encryption dictionary parameters are missing "
		      "or the wrong type");
    }

    int V = encryption_dict.getKey("/V").getIntValue();
    int R = encryption_dict.getKey("/R").getIntValue();
    std::string O = encryption_dict.getKey("/O").getStringValue();
    std::string U = encryption_dict.getKey("/U").getStringValue();
    unsigned int P = (unsigned int) encryption_dict.getKey("/P").getIntValue();

    if (! (((R == 2) || (R == 3)) &&
	   ((V == 1) || (V == 2))))
    {
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "Unsupported /R or /V in encryption dictionary");
    }

    if (! ((O.length() == key_bytes) && (U.length() == key_bytes)))
    {
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "incorrect length for /O and/or /P in "
		      "encryption dictionary");
    }

    int Length = 40;
    if (encryption_dict.getKey("/Length").isInteger())
    {
	Length = encryption_dict.getKey("/Length").getIntValue();
	if ((Length % 8) || (Length < 40) || (Length > 128))
	{
	    throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
			  "invalid /Length value in encryption dictionary");
	}
    }

    EncryptionData data(V, R, Length / 8, P, O, U, id1);
    if (check_owner_password(this->user_password, this->provided_password, data))
    {
	// password supplied was owner password; user_password has
	// been initialized
    }
    else if (check_user_password(this->provided_password, data))
    {
	this->user_password = this->provided_password;
    }
    else
    {
	throw QPDFExc(this->file.getName() + ": invalid password");
    }

    this->encryption_key = compute_encryption_key(this->user_password, data);
}

std::string
QPDF::getKeyForObject(int objid, int generation)
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
	    compute_data_key(this->encryption_key, objid, generation);
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
    std::string key = getKeyForObject(objid, generation);
    char* tmp = QUtil::copy_string(str);
    unsigned int vlen = str.length();
    RC4 rc4((unsigned char const*)key.c_str(), key.length());
    rc4.process((unsigned char*)tmp, vlen);
    str = std::string(tmp, vlen);
    delete [] tmp;
}

void
QPDF::decryptStream(Pipeline*& pipeline, int objid, int generation,
		    std::vector<PointerHolder<Pipeline> >& heap)
{
    std::string key = getKeyForObject(objid, generation);
    pipeline = new Pl_RC4("stream decryption", pipeline,
			  (unsigned char*) key.c_str(), key.length());
    heap.push_back(pipeline);
}

DLL_EXPORT
void
QPDF::compute_encryption_O_U(
    char const* user_password, char const* owner_password,
    int V, int R, int key_len, int P,
    std::string const& id1, std::string& O, std::string& U)
{
    EncryptionData data(V, R, key_len, P, "", "", id1);
    data.O = compute_O_value(user_password, owner_password, data);
    O = data.O;
    U = compute_U_value(user_password, data);
}

DLL_EXPORT
std::string const&
QPDF::getPaddedUserPassword() const
{
    return this->user_password;
}

DLL_EXPORT
std::string
QPDF::getTrimmedUserPassword() const
{
    std::string result = this->user_password;
    trim_user_password(result);
    return result;
}

DLL_EXPORT
bool
QPDF::isEncrypted() const
{
    return this->encrypted;
}

DLL_EXPORT
bool
QPDF::isEncrypted(int& R, int& P)
{
    if (this->encrypted)
    {
	QPDFObjectHandle trailer = getTrailer();
	QPDFObjectHandle encrypt = trailer.getKey("/Encrypt");
	QPDFObjectHandle Pkey = encrypt.getKey("/P");
	QPDFObjectHandle Rkey = encrypt.getKey("/R");
	P = Pkey.getIntValue();
	R = Rkey.getIntValue();
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

DLL_EXPORT
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

DLL_EXPORT
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

DLL_EXPORT
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

DLL_EXPORT
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

DLL_EXPORT
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

DLL_EXPORT
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

DLL_EXPORT
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

DLL_EXPORT
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

DLL_EXPORT
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
