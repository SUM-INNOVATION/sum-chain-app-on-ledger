/*
 * SUM Chain Ledger App - Address Derivation
 * Address = Base58( BLAKE3(pubkey32)[12:32] )
 */

#ifndef ADDRESS_H
#define ADDRESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Derive a 20-byte address from a 32-byte Ed25519 public key.
 * Address = BLAKE3(pubkey32)[12:32] (bytes 12-31 inclusive, 20 bytes)
 *
 * @param pubkey32   32-byte Ed25519 public key.
 * @param out_addr20 Output buffer for 20-byte address.
 */
void sumchain_address_bytes_from_pubkey(const uint8_t pubkey32[32], uint8_t out_addr20[20]);

/*
 * Encode a 20-byte address as Base58 string.
 *
 * @param addr20  20-byte raw address.
 * @param out     Output buffer for null-terminated Base58 string.
 * @param out_len Size of output buffer (must be >= ADDRESS_BASE58_MAX_LEN).
 * @return Number of characters written (excluding null), or 0 on error.
 */
size_t sumchain_address_to_base58(const uint8_t addr20[20], char *out, size_t out_len);

/*
 * Derive and format the address for a given BIP32 path.
 *
 * @param path        BIP32 derivation path.
 * @param display     If true, show the address on device display for confirmation.
 * @param out_str     Output buffer for Base58 address string.
 * @param out_str_len Size of output buffer.
 * @return true on success, false on failure.
 */
bool sumchain_get_address_for_path(const bip32_path_t *path,
                                   bool display,
                                   char *out_str,
                                   size_t out_str_len);

/*
 * Base58 encoding utility.
 *
 * @param in      Input bytes.
 * @param in_len  Length of input.
 * @param out     Output buffer for null-terminated string.
 * @param out_len Size of output buffer.
 * @return Number of characters written (excluding null), or 0 on error.
 */
size_t base58_encode(const uint8_t *in, size_t in_len, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* ADDRESS_H */
