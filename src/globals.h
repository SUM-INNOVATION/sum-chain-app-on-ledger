/*
 * SUM Chain Ledger App - Global State and Definitions
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "crypto/sum_blake3.h"

#ifdef HAVE_BOLOS_SDK
#include "os.h"
#include "cx.h"
#include "ux.h"
#endif

/*
 * Application version
 */
#define APPVERSION_MAJOR 1
#define APPVERSION_MINOR 0
#define APPVERSION_PATCH 0

/*
 * APDU instruction codes
 */
#define CLA_SUMCHAIN          0xE0

#define INS_GET_VERSION       0x00
#define INS_GET_APP_NAME      0x01
#define INS_GET_PUBLIC_KEY    0x02
#define INS_GET_ADDRESS       0x03
#define INS_SIGN_TX           0x04

/*
 * APDU P1/P2 constants for INS_SIGN_TX
 */
#define P1_FIRST_CHUNK        0x00
#define P1_MORE_CHUNK         0x80

#define P2_LAST_CHUNK         0x00
#define P2_MORE_CHUNKS        0x80

/*
 * Status words
 */
#define SW_OK                         0x9000
#define SW_USER_REJECTED              0x6985
#define SW_INVALID_PARAM              0x6B00
#define SW_INVALID_DATA               0x6A80
#define SW_INVALID_PATH               0x6A81
#define SW_INVALID_P1P2               0x6B00
#define SW_INS_NOT_SUPPORTED          0x6D00
#define SW_CLA_NOT_SUPPORTED          0x6E00
#define SW_WRONG_LENGTH               0x6700
#define SW_SECURITY_STATUS            0x6982
#define SW_CONDITIONS_NOT_SATISFIED   0x6985
#define SW_INTERNAL_ERROR             0x6F00
#define SW_TX_PARSE_ERROR             0x6F01
#define SW_TX_OVERFLOW                0x6F02
#define SW_SESSION_ERROR              0x6F03
#define SW_TX_TOO_LARGE               0x6F04

/*
 * Limits and sizes
 */
#define MAX_BIP32_PATH_LEN        10     /* Maximum derivation path depth */
#define PUBKEY_LEN                32     /* Ed25519 public key */
#define PRIVKEY_LEN               32     /* Ed25519 private key */
#define SIGNATURE_LEN             64     /* Ed25519 signature */
#define ADDRESS_LEN               20     /* SUM Chain address (bytes) */
#define ADDRESS_BASE58_MAX_LEN    35     /* Base58 encoded address + null */
#define HASH_LEN                  32     /* BLAKE3 hash output */
#define MAX_TX_SIZE               8192   /* Maximum transaction size (streaming, not buffered) */

/*
 * Transaction types
 */
#define TX_TYPE_TRANSFER          0x00

/*
 * BIP32 derivation path structure
 */
typedef struct {
    uint8_t  length;                       /* Number of path components (1-10) */
    uint32_t path[MAX_BIP32_PATH_LEN];     /* Path components */
} bip32_path_t;

/*
 * Transaction parser state enum
 */
typedef enum {
    TX_PARSE_STATE_INIT = 0,
    TX_PARSE_STATE_VERSION,
    TX_PARSE_STATE_CHAIN_ID,
    TX_PARSE_STATE_SENDER,
    TX_PARSE_STATE_NONCE,
    TX_PARSE_STATE_GAS_PRICE,
    TX_PARSE_STATE_GAS_LIMIT,
    TX_PARSE_STATE_TX_TYPE,
    /* Transfer-specific fields */
    TX_PARSE_STATE_RECIPIENT,
    TX_PARSE_STATE_AMOUNT,
    /* Terminal states */
    TX_PARSE_STATE_DONE,
    TX_PARSE_STATE_ERROR
} tx_parse_state_t;

/*
 * Parsed transaction data (display fields)
 */
typedef struct {
    uint8_t  version;
    uint64_t chain_id;
    uint8_t  sender[ADDRESS_LEN];
    uint64_t nonce;
    uint64_t gas_price;
    uint64_t gas_limit;
    uint8_t  tx_type;

    /* Transfer-specific */
    uint8_t  recipient[ADDRESS_LEN];
    uint64_t amount;                       /* TODO: Upgrade to u128 if needed */

    /* Computed fields for display */
    bool     fee_overflow;                 /* True if gas_price * gas_limit overflows */
    uint64_t fee_low;                      /* Low 64 bits of fee */
    uint64_t fee_high;                     /* High 64 bits of fee (for 128-bit result) */
} tx_parsed_t;

/*
 * Transaction parser context (streaming)
 */
typedef struct {
    tx_parse_state_t state;
    uint8_t          field_offset;         /* Current offset within the field being parsed */
    uint8_t          scratch[32];          /* Scratch buffer for partial field accumulation */
    tx_parsed_t      parsed;               /* Accumulated parsed values */
    size_t           total_consumed;       /* Total bytes consumed so far */
} tx_parser_ctx_t;

/*
 * Signing session state
 */
typedef struct {
    bool            initialized;           /* Session active flag */
    bip32_path_t    path;                  /* Derivation path for signing key */
    sum_blake3_ctx_t tx_hash_ctx;          /* Streaming hash context */
    tx_parser_ctx_t parser;                /* Streaming parser context */
    size_t          total_received;        /* Total tx bytes received */
    bool            last_chunk_received;   /* True when P2 indicates last chunk */
} sign_session_t;

/*
 * UI confirmation result
 */
typedef enum {
    UI_RESULT_NONE = 0,
    UI_RESULT_APPROVED,
    UI_RESULT_REJECTED
} ui_result_t;

/*
 * Global application state
 */
typedef struct {
    /* Current signing session */
    sign_session_t  sign_session;

    /* UI state */
    ui_result_t     ui_result;

    /* Temporary buffers */
    uint8_t         pubkey[PUBKEY_LEN];
    uint8_t         address_bytes[ADDRESS_LEN];
    char            address_str[ADDRESS_BASE58_MAX_LEN];
    uint8_t         hash[HASH_LEN];
    uint8_t         signature[SIGNATURE_LEN];
} app_state_t;

/*
 * Global state instance (defined in main.c)
 */
extern app_state_t G_app_state;

/*
 * Macro to access global state
 */
#define G_state G_app_state

/*
 * Secure memory zeroization
 */
#ifdef HAVE_BOLOS_SDK
#define SECURE_ZEROIZE(ptr, len) explicit_bzero((ptr), (len))
#else
static inline void _secure_zeroize(void *ptr, size_t len) {
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (len--) *p++ = 0;
}
#define SECURE_ZEROIZE(ptr, len) _secure_zeroize((ptr), (len))
#endif

/*
 * Helper to reset signing session
 */
static inline void reset_sign_session(void) {
    SECURE_ZEROIZE(&G_state.sign_session, sizeof(sign_session_t));
    G_state.sign_session.initialized = false;
}

#endif /* GLOBALS_H */
