/*
 * SUM Chain BLAKE3 Wrapper
 * Thin wrapper around BLAKE3 for Ledger embedded use.
 * No dynamic allocation.
 */

#ifndef SUM_BLAKE3_H
#define SUM_BLAKE3_H

#include <stddef.h>
#include <stdint.h>
#include "blake3/blake3.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Wrapped hasher context type.
 * Contains the underlying blake3_hasher plus any app-specific state.
 */
typedef struct {
    blake3_hasher hasher;
    uint8_t initialized;   /* Guard against use before init */
} sum_blake3_ctx_t;

/*
 * Initialize a BLAKE3 hasher context for standard hashing.
 * Must be called before update/finalize.
 *
 * @param ctx Pointer to context (caller-provided, no allocation).
 */
void sum_blake3_init(sum_blake3_ctx_t *ctx);

/*
 * Update the hasher with input data (incremental/streaming).
 * Can be called multiple times to feed data in chunks.
 *
 * @param ctx    Initialized context.
 * @param in     Input data buffer.
 * @param in_len Length of input data.
 */
void sum_blake3_update(sum_blake3_ctx_t *ctx, const uint8_t *in, size_t in_len);

/*
 * Finalize the hash and produce 32-byte output.
 * After calling this, the context should not be used again unless re-initialized.
 *
 * @param ctx   Initialized context with all data fed via update.
 * @param out32 Output buffer for 32-byte hash result.
 */
void sum_blake3_finalize32(sum_blake3_ctx_t *ctx, uint8_t out32[32]);

/*
 * Convenience: Hash a single buffer in one shot and produce 32 bytes.
 *
 * @param in     Input data buffer.
 * @param in_len Length of input data.
 * @param out32  Output buffer for 32-byte hash result.
 */
void sum_blake3_hash(const uint8_t *in, size_t in_len, uint8_t out32[32]);

/*
 * Reset the context to re-use it for a new hash (avoids re-init overhead).
 * Internally calls blake3_hasher_reset.
 *
 * @param ctx Previously initialized context.
 */
void sum_blake3_reset(sum_blake3_ctx_t *ctx);

/*
 * Securely zeroize the context to clear any internal state.
 * Should be called when done with sensitive data.
 *
 * @param ctx Context to zeroize.
 */
void sum_blake3_zeroize(sum_blake3_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SUM_BLAKE3_H */
