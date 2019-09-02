#include <gnutls/crypto.h>
#include <qpdf/MD5.hh>
#include <qpdf/QUtil.hh>


// MD5 initialization. Begins an MD5 operation, writing a new context.
void MD5::init()
{
    int ret;

    ret = gnutls_hash_init(&ctx, GNUTLS_DIG_MD5);
    if (ret < 0) {
	QUtil::throw_system_error(
	    std::string("GNU TLS: MD5 error:") + std::string(gnutls_strerror(ret)));
	ctx = nullptr;
    }

    finalized = false;
    memset(digest_val, 0, sizeof(digest_val));
}

// MD5 block update operation. Continues an MD5 message-digest
// operation, processing another message block, and updating the
// context.

void MD5::update(unsigned char *input,
		 size_t inputLen)
{
    if (ctx != nullptr && input != nullptr)
	gnutls_hash(ctx, input, inputLen);
}

// MD5 finalization. Ends an MD5 message-digest operation, writing the
// the message digest and zeroizing the context.
void MD5::final()
{
    if (finalized)
	return;

    if (ctx != nullptr)
	gnutls_hash_deinit(ctx, digest_val);

    finalized = true;
}

// MD5 basic transformation. Transforms state based on block.
void MD5::transform(UINT4 state[4], unsigned char block[64])
{}

// Encodes input (UINT4) into output (unsigned char). Assumes len is a
// multiple of 4.
void MD5::encode(unsigned char *output, UINT4 *input, size_t len)
{}

// Decodes input (unsigned char) into output (UINT4). Assumes len is a
// multiple of 4.
void MD5::decode(UINT4 *output, unsigned char *input, size_t len)
{}
