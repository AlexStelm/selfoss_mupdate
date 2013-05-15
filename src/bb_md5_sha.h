/*
 * hash_md5_sha.c minimal header
 * copied from Busybox libbb.h
 */

#ifndef BB_MD5_SHA_H
#define BB_MD5_SHA_H

#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <endian.h>

#define FAST_FUNC
#define ALWAYS_INLINE inline
#define bb_bswap_64(x) bswap_64(x)

/* --- platform.h --- */

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#elif defined(_BYTE_ORDER) && _BYTE_ORDER == _BIG_ENDIAN
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#elif defined(_BYTE_ORDER) && _BYTE_ORDER == _LITTLE_ENDIAN
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#elif defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#elif defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#elif defined(__386__)
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#else
# error "Can't determine endianness"
#endif

#if BB_BIG_ENDIAN
# define SWAP_BE16(x) (x)
# define SWAP_BE32(x) (x)
# define SWAP_BE64(x) (x)
# define SWAP_LE16(x) bswap_16(x)
# define SWAP_LE32(x) bswap_32(x)
# define SWAP_LE64(x) bb_bswap_64(x)
# define IF_BIG_ENDIAN(...) __VA_ARGS__
# define IF_LITTLE_ENDIAN(...)
#else
# define SWAP_BE16(x) bswap_16(x)
# define SWAP_BE32(x) bswap_32(x)
# define SWAP_BE64(x) bb_bswap_64(x)
# define SWAP_LE16(x) (x)
# define SWAP_LE32(x) (x)
# define SWAP_LE64(x) (x)
# define IF_BIG_ENDIAN(...)
# define IF_LITTLE_ENDIAN(...) __VA_ARGS__
#endif

/* --- libbb.h --- */


typedef struct md5_ctx_t {
	uint8_t wbuffer[64]; /* always correctly aligned for uint64_t */
	void (*process_block)(struct md5_ctx_t*) FAST_FUNC;
	uint64_t total64;    /* must be directly before hash[] */
	uint32_t hash[8];    /* 4 elements for md5, 5 for sha1, 8 for sha256 */
} md5_ctx_t;

#if 0
typedef struct md5_ctx_t sha1_ctx_t;
typedef struct md5_ctx_t sha256_ctx_t;
typedef struct sha512_ctx_t {
	uint64_t total64[2];  /* must be directly before hash[] */
	uint64_t hash[8];
	uint8_t wbuffer[128]; /* always correctly aligned for uint64_t */
} sha512_ctx_t;
#endif /* 0 */

void md5_begin(md5_ctx_t *ctx) FAST_FUNC;
void md5_hash(md5_ctx_t *ctx, const void *data, size_t length) FAST_FUNC;
void md5_end(md5_ctx_t *ctx, void *resbuf) FAST_FUNC;

#if 0
void sha1_begin(sha1_ctx_t *ctx) FAST_FUNC;
#define sha1_hash md5_hash
void sha1_end(sha1_ctx_t *ctx, void *resbuf) FAST_FUNC;
void sha256_begin(sha256_ctx_t *ctx) FAST_FUNC;
#define sha256_hash md5_hash
#define sha256_end  sha1_end
void sha512_begin(sha512_ctx_t *ctx) FAST_FUNC;
void sha512_hash(sha512_ctx_t *ctx, const void *buffer, size_t len) FAST_FUNC;
void sha512_end(sha512_ctx_t *ctx, void *resbuf) FAST_FUNC;
#endif /* 0 */

#endif /* BB_MD5_SHA_H */

