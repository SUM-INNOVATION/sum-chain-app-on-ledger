/*
 * SUM Chain Ledger App - Cryptographic Operations
 * Ed25519 key derivation and signing using BOLOS SDK.
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Validate a BIP32 derivation path.
 * Requirements for Ed25519:
 * - Path length must be 1-MAX_BIP32_PATH_LEN
 * - All components should be hardened (0x80000000 bit set)
 *
 * @param path Pointer to path structure.
 * @return true if valid, false otherwise.
 */
bool crypto_validate_path(const bip32_path_t *path);

/*
 * Parse a BIP32 path from raw APDU data.
 * Format: [length:1 byte] [path[0]:4 bytes BE] [path[1]:4 bytes BE] ...
 *
 * @param data     Raw data buffer.
 * @param data_len Length of data buffer.
 * @param path     Output path structure.
 * @return Number of bytes consumed, or 0 on error.
 */
size_t crypto_parse_path(const uint8_t *data, size_t data_len, bip32_path_t *path);

/*
 * Derive Ed25519 public key from BIP32 path.
 *
 * @param path      Validated derivation path.
 * @param pubkey32  Output buffer for 32-byte public key.
 * @return true on success, false on failure.
 */
bool crypto_derive_pubkey(const bip32_path_t *path, uint8_t pubkey32[32]);

/*
 * Sign a 32-byte hash with Ed25519 using the private key at the given path.
 * The private key is derived, used, and immediately zeroized.
 *
 * @param path      Validated derivation path.
 * @param hash32    32-byte hash to sign.
 * @param sig64     Output buffer for 64-byte signature.
 * @return true on success, false on failure.
 */
bool crypto_sign_hash(const bip32_path_t *path, const uint8_t hash32[32], uint8_t sig64[64]);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_H */
