/*
 * SUM Chain BLAKE3 Wrapper Implementation
 */

#include "sum_blake3.h"
#include <string.h>

/* Ledger SDK provides explicit_bzero; fallback for host testing */
#ifdef HAVE_BOLOS_SDK
#include "os.h"
#define secure_memzero(ptr, len) explicit_bzero((ptr), (len))
#else
/* Volatile pointer trick to prevent optimization */
static void secure_memzero(void *ptr, size_t len) {
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (len--) {
        *p++ = 0;
    }
}
#endif

void sum_blake3_init(sum_blake3_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    blake3_hasher_init(&ctx->hasher);
    ctx->initialized = 1;
}

void sum_blake3_update(sum_blake3_ctx_t *ctx, const uint8_t *in, size_t in_len) {
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    if (in == NULL && in_len > 0) {
        return;
    }
    blake3_hasher_update(&ctx->hasher, in, in_len);
}

void sum_blake3_finalize32(sum_blake3_ctx_t *ctx, uint8_t out32[32]) {
    if (ctx == NULL || !ctx->initialized || out32 == NULL) {
        return;
    }
    blake3_hasher_finalize(&ctx->hasher, out32, 32);
    /* Mark as finalized to prevent reuse */
    ctx->initialized = 0;
}

void sum_blake3_hash(const uint8_t *in, size_t in_len, uint8_t out32[32]) {
    sum_blake3_ctx_t ctx;
    sum_blake3_init(&ctx);
    sum_blake3_update(&ctx, in, in_len);
    sum_blake3_finalize32(&ctx, out32);
    sum_blake3_zeroize(&ctx);
}

void sum_blake3_reset(sum_blake3_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    blake3_hasher_reset(&ctx->hasher);
    ctx->initialized = 1;
}

void sum_blake3_zeroize(sum_blake3_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    secure_memzero(ctx, sizeof(sum_blake3_ctx_t));
}
