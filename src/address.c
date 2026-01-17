/*
 * SUM Chain Ledger App - Address Derivation Implementation
 */

#include "address.h"
#include "crypto.h"
#include "crypto/sum_blake3.h"
#include <string.h>

/* Base58 alphabet (Bitcoin-style, no 0OIl) */
static const char BASE58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

/*
 * Base58 encode implementation.
 * No checksum - pure Base58.
 * Works in-place with a scratch buffer on stack.
 */
size_t base58_encode(const uint8_t *in, size_t in_len, char *out, size_t out_len) {
    if (in == NULL || out == NULL || out_len == 0) {
        return 0;
    }

    if (in_len == 0) {
        out[0] = '\0';
        return 0;
    }

    /*
     * Maximum Base58 length for N bytes is ceil(N * log(256) / log(58)) ≈ N * 1.37
     * For 20 bytes: max ~28 chars. We use a fixed scratch buffer.
     */
    #define MAX_INPUT_LEN 32
    #define MAX_B58_LEN   45

    if (in_len > MAX_INPUT_LEN) {
        out[0] = '\0';
        return 0;
    }

    /* Work buffer: we'll do division in place */
    uint8_t buf[MAX_INPUT_LEN];
    memcpy(buf, in, in_len);

    /* Count leading zeros */
    size_t leading_zeros = 0;
    while (leading_zeros < in_len && buf[leading_zeros] == 0) {
        leading_zeros++;
    }

    /* Temporary output buffer (reversed) */
    char b58_rev[MAX_B58_LEN];
    size_t b58_len = 0;

    /* Convert to Base58 via repeated division */
    size_t start = leading_zeros;
    while (start < in_len) {
        uint32_t carry = 0;
        size_t i;

        /* Divide buf by 58, collecting the remainder */
        for (i = start; i < in_len; i++) {
            carry = carry * 256 + buf[i];
            buf[i] = (uint8_t)(carry / 58);
            carry = carry % 58;
        }

        /* The remainder is the next Base58 digit */
        if (b58_len >= MAX_B58_LEN) {
            out[0] = '\0';
            return 0;  /* Overflow */
        }
        b58_rev[b58_len++] = BASE58_ALPHABET[carry];

        /* Skip leading zeros in the quotient */
        while (start < in_len && buf[start] == 0) {
            start++;
        }
    }

    /* Output leading '1's for each leading zero byte */
    size_t total_len = leading_zeros + b58_len;
    if (total_len + 1 > out_len) {
        out[0] = '\0';
        return 0;  /* Output buffer too small */
    }

    size_t idx = 0;
    for (size_t i = 0; i < leading_zeros; i++) {
        out[idx++] = '1';
    }

    /* Reverse the Base58 digits into output */
    for (size_t i = b58_len; i > 0; i--) {
        out[idx++] = b58_rev[i - 1];
    }
    out[idx] = '\0';

    return idx;

    #undef MAX_INPUT_LEN
    #undef MAX_B58_LEN
}

void sumchain_address_bytes_from_pubkey(const uint8_t pubkey32[32], uint8_t out_addr20[20]) {
    uint8_t hash[32];

    if (pubkey32 == NULL || out_addr20 == NULL) {
        return;
    }

    /* Compute BLAKE3 hash of the public key */
    sum_blake3_hash(pubkey32, 32, hash);

    /* Take bytes [12..31] (20 bytes) as the address */
    memcpy(out_addr20, &hash[12], 20);

    /* Zeroize intermediate hash */
    SECURE_ZEROIZE(hash, sizeof(hash));
}

size_t sumchain_address_to_base58(const uint8_t addr20[20], char *out, size_t out_len) {
    if (addr20 == NULL || out == NULL) {
        return 0;
    }

    return base58_encode(addr20, ADDRESS_LEN, out, out_len);
}

bool sumchain_get_address_for_path(const bip32_path_t *path,
                                   bool display,
                                   char *out_str,
                                   size_t out_str_len) {
    uint8_t pubkey[PUBKEY_LEN];
    uint8_t addr_bytes[ADDRESS_LEN];

    if (path == NULL || out_str == NULL || out_str_len < ADDRESS_BASE58_MAX_LEN) {
        return false;
    }

    /* Validate and derive public key */
    if (!crypto_validate_path(path)) {
        return false;
    }

    if (!crypto_derive_pubkey(path, pubkey)) {
        return false;
    }

    /* Derive address from pubkey */
    sumchain_address_bytes_from_pubkey(pubkey, addr_bytes);

    /* Encode as Base58 */
    size_t len = sumchain_address_to_base58(addr_bytes, out_str, out_str_len);
    if (len == 0) {
        return false;
    }

    /* Zeroize intermediate buffers */
    SECURE_ZEROIZE(pubkey, sizeof(pubkey));
    SECURE_ZEROIZE(addr_bytes, sizeof(addr_bytes));

    /*
     * If display is requested, show on device.
     * This would trigger the UI flow for address confirmation.
     * For now, we assume the APDU handler manages display separately.
     */
    (void)display;  /* TODO: Hook into UX flow if needed */

    return true;
}
