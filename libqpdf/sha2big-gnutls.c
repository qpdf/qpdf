#include <gnutls/crypto.h>
#include <stdio.h>
#include "sph/sph_sha2_gnutls.h"


/* see sph_sha3.h */
void
sph_sha384_init(void * cc)
{
	int ret;
	gnutls_hash_hd_t * ctx = (gnutls_hash_hd_t*) cc;

	ret = gnutls_hash_init(ctx, GNUTLS_DIG_SHA384);
	if (ret < 0)
	{
		fprintf(stderr, "GNU TLS: SHA384 error: %s\n", gnutls_strerror(ret));
		cc = NULL;
	}
}

/* see sph_sha3.h */
void
sph_sha384_close(void * cc, void *dst)
{
	gnutls_hash_hd_t * ctx = (gnutls_hash_hd_t*) cc;

	if (ctx != NULL && dst != NULL)
	{
		gnutls_hash_deinit(*(ctx), dst);
		cc = NULL;
	}
}

/* see sph_sha3.h */
void
sph_sha384(void * cc, const void * data, size_t len)
{
	gnutls_hash_hd_t * ctx = (gnutls_hash_hd_t*) cc;

	if (ctx != NULL && data != NULL)
		gnutls_hash(*(ctx), data, len);
}

/* see sph_sha3.h */
void
sph_sha512_init(void * cc)
{
	int ret;
	gnutls_hash_hd_t * ctx = (gnutls_hash_hd_t*) cc;

	ret = gnutls_hash_init(ctx, GNUTLS_DIG_SHA512);
	if (ret < 0)
	{
		fprintf(stderr, "GNU TLS: SHA512 error: %s\n", gnutls_strerror(ret));
		cc = NULL;
	}
}

/* see sph_sha3.h */
void
sph_sha512_close(void * cc, void * dst)
{
	gnutls_hash_hd_t * ctx = (gnutls_hash_hd_t*) cc;

	if (ctx != NULL && dst != NULL)
	{
		gnutls_hash_deinit(*(ctx), dst);
		cc = NULL;
	}
}

/* see sph_sha3.h */
void
sph_sha512(void * cc, const void * data, size_t len)
{
	gnutls_hash_hd_t * ctx = (gnutls_hash_hd_t*) cc;

	if (ctx != NULL && data != NULL)
		gnutls_hash(*(ctx), data, len);
}
