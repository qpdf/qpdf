// This file implements methods from the QPDF class that involve
// encryption.

#include <qpdf/QPDF.hh>

#include <qpdf/QPDFExc.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Pl_RC4.hh>
#include <qpdf/Pl_AES_PDF.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/RC4.hh>
#include <qpdf/MD5.hh>

#include <assert.h>
#include <string.h>

static unsigned char const padding_string[] = {
    0x28, 0xbf, 0x4e, 0x5e, 0x4e, 0x75, 0x8a, 0x41,
    0x64, 0x00, 0x4e, 0x56, 0xff, 0xfa, 0x01, 0x08,
    0x2e, 0x2e, 0x00, 0xb6, 0xd0, 0x68, 0x3e, 0x80,
    0x2f, 0x0c, 0xa9, 0xfe, 0x64, 0x53, 0x69, 0x7a
};

static unsigned int const O_key_bytes = sizeof(MD5::Digest);
static unsigned int const key_bytes = 32;

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

static void
pad_or_truncate_password(std::string const& password, char k1[key_bytes])
{
    int password_bytes = std::min(key_bytes, (unsigned int)password.length());
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

std::string
QPDF::compute_data_key(std::string const& encryption_key,
		       int objid, int generation,
		       bool use_aes)
{
    // Algorithm 3.1 from the PDF 1.7 Reference Manual

    std::string result = encryption_key;

    // Append low three bytes of object ID and low two bytes of generation
    result += (char) (objid & 0xff);
    result += (char) ((objid >> 8) & 0xff);
    result += (char) ((objid >> 16) & 0xff);
    result += (char) (generation & 0xff);
    result += (char) ((generation >> 8) & 0xff);
    if (use_aes)
    {
	result += "sAlT";
    }

    MD5 md5;
    md5.encodeDataIncrementally(result.c_str(), (int)result.length());
    MD5::Digest digest;
    md5.digest(digest);
    return std::string((char*) digest,
		       std::min(result.length(), (size_t) 16));
}

std::string
QPDF::compute_encryption_key(
    std::string const& password, EncryptionData const& data)
{
    // Algorithm 3.2 from the PDF 1.7 Reference Manual

    MD5 md5;
    md5.encodeDataIncrementally(
	pad_or_truncate_password(password).c_str(), key_bytes);
    md5.encodeDataIncrementally(data.getO().c_str(), key_bytes);
    char pbytes[4];
    int P = data.getP();
    pbytes[0] = (char) (P & 0xff);
    pbytes[1] = (char) ((P >> 8) & 0xff);
    pbytes[2] = (char) ((P >> 16) & 0xff);
    pbytes[3] = (char) ((P >> 24) & 0xff);
    md5.encodeDataIncrementally(pbytes, 4);
    md5.encodeDataIncrementally(data.getId1().c_str(),
                                (int)data.getId1().length());
    if ((data.getR() >= 4) && (! data.getEncryptMetadata()))
    {
	char bytes[4];
	memset(bytes, 0xff, 4);
	md5.encodeDataIncrementally(bytes, 4);
    }
    MD5::Digest digest;
    iterate_md5_digest(md5, digest, ((data.getR() >= 3) ? 50 : 0));
    return std::string((char*)digest, data.getLengthBytes());
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
    iterate_md5_digest(md5, digest, ((data.getR() >= 3) ? 50 : 0));
    memcpy(key, digest, O_key_bytes);
}

static std::string
compute_O_value(std::string const& user_password,
		std::string const& owner_password,
		QPDF::EncryptionData const& data)
{
    // Algorithm 3.3 from the PDF 1.7 Reference Manual

    unsigned char O_key[O_key_bytes];
    compute_O_rc4_key(user_password, owner_password, data, O_key);

    char upass[key_bytes];
    pad_or_truncate_password(user_password, upass);
    iterate_rc4((unsigned char*) upass, key_bytes,
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
    pad_or_truncate_password("", udata);
    iterate_rc4((unsigned char*) udata, key_bytes,
		(unsigned char*)k1.c_str(), data.getLengthBytes(), 1, false);
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
	pad_or_truncate_password("").c_str(), key_bytes);
    md5.encodeDataIncrementally(data.getId1().c_str(),
                                (int)data.getId1().length());
    MD5::Digest digest;
    md5.digest(digest);
    iterate_rc4(digest, sizeof(MD5::Digest),
		(unsigned char*) k1.c_str(), data.getLengthBytes(), 20, false);
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
    if (data.getR() >= 3)
    {
	return compute_U_value_R3(user_password, data);
    }

    return compute_U_value_R2(user_password, data);
}

static bool
check_user_password(std::string const& user_password,
		    QPDF::EncryptionData const& data)
{
    // Algorithm 3.6 from the PDF 1.7 Reference Manual

    std::string u_value = compute_U_value(user_password, data);
    int to_compare = ((data.getR() >= 3) ? sizeof(MD5::Digest) : key_bytes);
    return (memcmp(data.getU().c_str(), u_value.c_str(), to_compare) == 0);
}

static bool
check_owner_password(std::string& user_password,
		     std::string const& owner_password,
		     QPDF::EncryptionData const& data)
{
    // Algorithm 3.7 from the PDF 1.7 Reference Manual

    unsigned char key[O_key_bytes];
    compute_O_rc4_key(user_password, owner_password, data, key);
    unsigned char O_data[key_bytes];
    memcpy(O_data, (unsigned char*) data.getO().c_str(), key_bytes);
    iterate_rc4(O_data, key_bytes, key, data.getLengthBytes(),
		(data.getR() >= 3) ? 20 : 1, true);
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

    QPDFObjectHandle id_obj = this->trailer.getKey("/ID");
    if (! (id_obj.isArray() &&
	   (id_obj.getArrayNItems() == 2) &&
	   id_obj.getArrayItem(0).isString()))
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "trailer", this->file->getLastOffset(),
		      "invalid /ID in trailer dictionary");
    }

    std::string id1 = id_obj.getArrayItem(0).getStringValue();
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
    unsigned int P = (unsigned int) encryption_dict.getKey("/P").getIntValue();

    if (! (((R == 2) || (R == 3) || (R == 4)) &&
	   ((V == 1) || (V == 2) || (V == 4))))
    {
	throw QPDFExc(qpdf_e_unsupported, this->file->getName(),
		      "encryption dictionary", this->file->getLastOffset(),
		      "Unsupported /R or /V in encryption dictionary");
    }

    this->encryption_V = V;

    if (! ((O.length() == key_bytes) && (U.length() == key_bytes)))
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "encryption dictionary", this->file->getLastOffset(),
		      "incorrect length for /O and/or /P in "
		      "encryption dictionary");
    }

    int Length = 40;
    if (encryption_dict.getKey("/Length").isInteger())
    {
	Length = encryption_dict.getKey("/Length").getIntValue();
	if ((Length % 8) || (Length < 40) || (Length > 128))
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

    if (V == 4)
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
	if (this->cf_file != this->cf_stream)
	{
	    // The issue for qpdf is that it can't tell the difference
	    // between an embedded file stream and a regular stream.
	    // Search for a comment containing cf_file.  To fix this,
	    // we need files with encrypted embedded files and
	    // non-encrypted native streams and vice versa.  Also if
	    // it is possible for them to be encrypted in different
	    // ways, we should have some of those too.  In cases where
	    // we can detect whether a stream is encrypted or not, we
	    // might want to try to detecet that automatically in
	    // defense of possible logic errors surrounding detection
	    // of embedded file streams, unless that's really clear
	    // from the specification.
	    throw QPDFExc(qpdf_e_unsupported, this->file->getName(),
			  "encryption dictionary", this->file->getLastOffset(),
			  "This document has embedded files that are"
			  " encrypted differently from the rest of the file."
			  "  qpdf does not presently support this due to"
			  " lack of test data; if possible, please submit"
			  " a bug report that includes this file.");
	}
    }
    EncryptionData data(V, R, Length / 8, P, O, U, id1, this->encrypt_metadata);
    if (check_owner_password(
	    this->user_password, this->provided_password, data))
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
	throw QPDFExc(qpdf_e_password, this->file->getName(),
		      "", 0, "invalid password");
    }

    this->encryption_key = compute_encryption_key(this->user_password, data);
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
	    compute_data_key(this->encryption_key, objid, generation, use_aes);
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
    if (this->encryption_V == 4)
    {
	switch (this->cf_string)
	{
	  case e_none:
	    return;

	  case e_aes:
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
	    assert(key.length() == Pl_AES_PDF::key_size);
	    Pl_Buffer bufpl("decrypted string");
	    Pl_AES_PDF pl("aes decrypt string", &bufpl, false,
			  (unsigned char const*)key.c_str());
	    pl.write((unsigned char*)str.c_str(), str.length());
	    pl.finish();
	    PointerHolder<Buffer> buf = bufpl.getBuffer();
	    str = std::string((char*)buf->getBuffer(), buf->getSize());
	}
	else
	{
	    QTC::TC("qpdf", "QPDF_encryption rc4 decode string");
	    unsigned int vlen = (int)str.length();
	    // Using PointerHolder guarantees that tmp will
	    // be freed even if rc4.process throws an exception.
	    PointerHolder<char> tmp(true, QUtil::copy_string(str));
	    RC4 rc4((unsigned char const*)key.c_str(), (int)key.length());
	    rc4.process((unsigned char*)tmp.getPointer(), vlen);
	    str = std::string(tmp.getPointer(), vlen);
	}
    }
    catch (QPDFExc& e)
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
    if (this->encryption_V == 4)
    {
	encryption_method_e method = e_unknown;
	std::string method_source = "/StmF from /Encrypt dictionary";

	if (stream_dict.getKey("/Filter").isOrHasName("/Crypt") &&
	    stream_dict.getKey("/DecodeParms").isDictionary())
	{
	    QPDFObjectHandle decode_parms = stream_dict.getKey("/DecodeParms");
	    if (decode_parms.getKey("/Type").isName() &&
		(decode_parms.getKey("/Type").getName() ==
		 "/CryptFilterDecodeParms"))
	    {
		QTC::TC("qpdf", "QPDF_encryption stream crypt filter");
		method = interpretCF(decode_parms.getKey("/Name"));
		method_source = "stream's Crypt decode parameters";
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
		// NOTE: We should should use cf_file if this is an
		// embedded file, but we can't yet detect embedded
		// file streams as such.  When fixing, search for all
		// occurrences of cf_file to find a reference to this
		// comment.
		method = this->cf_stream;
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
	assert(key.length() == Pl_AES_PDF::key_size);
	pipeline = new Pl_AES_PDF("AES stream decryption", pipeline,
				  false, (unsigned char*) key.c_str());
    }
    else
    {
	QTC::TC("qpdf", "QPDF_encryption rc4 decode stream");
	pipeline = new Pl_RC4("RC4 stream decryption", pipeline,
			      (unsigned char*) key.c_str(), (int)key.length());
    }
    heap.push_back(pipeline);
}

void
QPDF::compute_encryption_O_U(
    char const* user_password, char const* owner_password,
    int V, int R, int key_len, int P, bool encrypt_metadata,
    std::string const& id1, std::string& O, std::string& U)
{
    EncryptionData data(V, R, key_len, P, "", "", id1, encrypt_metadata);
    data.setO(compute_O_value(user_password, owner_password, data));
    O = data.getO();
    data.setU(compute_U_value(user_password, data));
    U = data.getU();
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
