/*
 * SUM Chain Ledger App - Cryptographic Operations Implementation
 */

#include "crypto.h"
#include <string.h>

#ifdef HAVE_BOLOS_SDK
#include "os.h"
#include "cx.h"
#endif

bool crypto_validate_path(const bip32_path_t *path) {
    if (path == NULL) {
        return false;
    }

    /* Check path length */
    if (path->length == 0 || path->length > MAX_BIP32_PATH_LEN) {
        return false;
    }

    /* For Ed25519, all components should be hardened */
    for (uint8_t i = 0; i < path->length; i++) {
        if ((path->path[i] & 0x80000000) == 0) {
            return false;  /* Not hardened */
        }
    }

    return true;
}

size_t crypto_parse_path(const uint8_t *data, size_t data_len, bip32_path_t *path) {
    if (data == NULL || path == NULL || data_len < 1) {
        return 0;
    }

    /* First byte is path length */
    uint8_t len = data[0];
    if (len == 0 || len > MAX_BIP32_PATH_LEN) {
        return 0;
    }

    /* Check we have enough data for all path components (4 bytes each, big-endian) */
    size_t required = 1 + (size_t)len * 4;
    if (data_len < required) {
        return 0;
    }

    path->length = len;
    for (uint8_t i = 0; i < len; i++) {
        const uint8_t *p = &data[1 + i * 4];
        path->path[i] = ((uint32_t)p[0] << 24) |
                        ((uint32_t)p[1] << 16) |
                        ((uint32_t)p[2] << 8)  |
                        ((uint32_t)p[3]);
    }

    return required;
}

#ifdef HAVE_BOLOS_SDK

bool crypto_derive_pubkey(const bip32_path_t *path, uint8_t pubkey32[32]) {
    cx_ecfp_private_key_t private_key;
    cx_ecfp_public_key_t  public_key;
    uint8_t raw_privkey[PRIVKEY_LEN];
    bool success = false;

    if (path == NULL || pubkey32 == NULL) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            /* Derive raw private key from seed */
            os_perso_derive_node_bip32_seed_key(
                HDW_ED25519_SLIP10,
                CX_CURVE_Ed25519,
                path->path,
                path->length,
                raw_privkey,
                NULL,
                NULL,
                0
            );

            /* Initialize private key structure */
            cx_ecfp_init_private_key_no_throw(
                CX_CURVE_Ed25519,
                raw_privkey,
                PRIVKEY_LEN,
                &private_key
            );

            /* Generate public key */
            cx_ecfp_generate_pair_no_throw(
                CX_CURVE_Ed25519,
                &public_key,
                &private_key,
                1  /* Keep private key */
            );

            /*
             * Ed25519 public key from BOLOS is 65 bytes: 0x04 || X (32) || Y (32)
             * We need the compressed form which is just the Y coordinate with
             * parity in the high bit. For Ed25519, the convention is:
             * compressed = Y with bit 255 = X[0] & 1
             */
            cx_edwards_compress_point_no_throw(CX_CURVE_Ed25519, public_key.W, public_key.W_len);
            /* After compression, W contains 33 bytes: 0x02/0x03 || 32-byte compressed point */
            /* Copy the 32-byte compressed key (skip the prefix byte) */
            memcpy(pubkey32, public_key.W + 1, PUBKEY_LEN);

            success = true;
        }
        CATCH_OTHER(e) {
            success = false;
        }
        FINALLY {
            /* Zeroize sensitive data */
            explicit_bzero(&private_key, sizeof(private_key));
            explicit_bzero(raw_privkey, sizeof(raw_privkey));
        }
    }
    END_TRY;

    return success;
}

bool crypto_sign_hash(const bip32_path_t *path, const uint8_t hash32[32], uint8_t sig64[64]) {
    cx_ecfp_private_key_t private_key;
    uint8_t raw_privkey[PRIVKEY_LEN];
    bool success = false;
    size_t sig_len = SIGNATURE_LEN;

    if (path == NULL || hash32 == NULL || sig64 == NULL) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            /* Derive raw private key from seed */
            os_perso_derive_node_bip32_seed_key(
                HDW_ED25519_SLIP10,
                CX_CURVE_Ed25519,
                path->path,
                path->length,
                raw_privkey,
                NULL,
                NULL,
                0
            );

            /* Initialize private key structure */
            cx_ecfp_init_private_key_no_throw(
                CX_CURVE_Ed25519,
                raw_privkey,
                PRIVKEY_LEN,
                &private_key
            );

            /* Sign the hash with Ed25519 */
            cx_eddsa_sign_no_throw(
                &private_key,
                CX_SHA512,
                hash32,
                HASH_LEN,
                sig64,
                sig_len
            );

            success = true;
        }
        CATCH_OTHER(e) {
            success = false;
        }
        FINALLY {
            /* Zeroize sensitive data */
            explicit_bzero(&private_key, sizeof(private_key));
            explicit_bzero(raw_privkey, sizeof(raw_privkey));
        }
    }
    END_TRY;

    return success;
}

#else
/* Stub implementations for host-side testing */

bool crypto_derive_pubkey(const bip32_path_t *path, uint8_t pubkey32[32]) {
    (void)path;
    /* Return a dummy pubkey for testing */
    memset(pubkey32, 0x42, PUBKEY_LEN);
    return true;
}

bool crypto_sign_hash(const bip32_path_t *path, const uint8_t hash32[32], uint8_t sig64[64]) {
    (void)path;
    (void)hash32;
    /* Return a dummy signature for testing */
    memset(sig64, 0xAA, SIGNATURE_LEN);
    return true;
}

#endif /* HAVE_BOLOS_SDK */
