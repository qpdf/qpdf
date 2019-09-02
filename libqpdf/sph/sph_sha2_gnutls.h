#include <qpdf/qpdf-config.h>
#include <gnutls/crypto.h>

typedef gnutls_hash_hd_t sph_sha256_context;
typedef gnutls_hash_hd_t sph_sha384_context;
typedef gnutls_hash_hd_t sph_sha512_context;

/**
 * Initialize a SHA-256 context.
 *
 * @param cc   the SHA-256 context (pointer to
 *             a <code>sph_sha256_context</code>)
 */
void sph_sha256_init(void *cc);

/**
 * Process some data bytes, for SHA-256.
 *
 * @param cc     the SHA-224 context
 * @param data   the input data
 * @param len    the input data length (in bytes)
 */
void sph_sha256(void *cc, const void *data, size_t len);

/**
 * Terminate the current SHA-256 computation and output the result into the
 * provided buffer. The destination buffer must be wide enough to
 * accomodate the result (32 bytes).
 *
 * @param cc    the SHA-256 context
 * @param dst   the destination buffer
 */
void sph_sha256_close(void *cc, void *dst);

/**
 * Initialize a SHA-384 context.
 *
 * @param cc   the SHA-384 context (pointer to
 *             a <code>sph_sha384_context</code>)
 */
void sph_sha384_init(void *cc);

/**
 * Process some data bytes. It is acceptable that <code>len</code> is zero
 * (in which case this function does nothing).
 *
 * @param cc     the SHA-384 context
 * @param data   the input data
 * @param len    the input data length (in bytes)
 */
void sph_sha384(void *cc, const void *data, size_t len);

/**
 * Terminate the current SHA-384 computation and output the result into the
 * provided buffer. The destination buffer must be wide enough to
 * accomodate the result (48 bytes).
 *
 * @param cc    the SHA-384 context
 * @param dst   the destination buffer
 */
void sph_sha384_close(void *cc, void *dst);

/**
 * Initialize a SHA-512 context.
 *
 * @param cc   the SHA-512 context (pointer to
 *             a <code>sph_sha512_context</code>)
 */
void sph_sha512_init(void *cc);

/**
 * Process some data bytes, for SHA-512.
 *
 * @param cc     the SHA-384 context
 * @param data   the input data
 * @param len    the input data length (in bytes)
 */
void sph_sha512(void *cc, const void *data, size_t len);

/**
 * Terminate the current SHA-512 computation and output the result into the
 * provided buffer. The destination buffer must be wide enough to
 * accomodate the result (64 bytes).
 *
 * @param cc    the SHA-512 context
 * @param dst   the destination buffer
 */
void sph_sha512_close(void *cc, void *dst);
