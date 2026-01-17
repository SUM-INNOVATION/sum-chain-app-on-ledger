/*
 * SUM Chain Ledger App - APDU Command Handlers Implementation
 */

#include "apdu_handlers.h"
#include "crypto.h"
#include "address.h"
#include "tx_parser.h"
#include "tx_display.h"
#include "crypto/sum_blake3.h"
#include <string.h>

/* Application name */
static const char APP_NAME[] = "SUM Chain";

uint16_t handle_get_version(const apdu_t *apdu, uint8_t **tx) {
    (void)apdu;

    if (tx == NULL || *tx == NULL) {
        return SW_INTERNAL_ERROR;
    }

    /* Return version: major, minor, patch */
    (*tx)[0] = APPVERSION_MAJOR;
    (*tx)[1] = APPVERSION_MINOR;
    (*tx)[2] = APPVERSION_PATCH;
    *tx += 3;

    return SW_OK;
}

uint16_t handle_get_app_name(const apdu_t *apdu, uint8_t **tx) {
    (void)apdu;

    if (tx == NULL || *tx == NULL) {
        return SW_INTERNAL_ERROR;
    }

    size_t name_len = strlen(APP_NAME);
    memcpy(*tx, APP_NAME, name_len);
    *tx += name_len;

    return SW_OK;
}

uint16_t handle_get_public_key(const apdu_t *apdu, uint8_t **tx) {
    bip32_path_t path;
    size_t path_bytes;

    if (apdu == NULL || tx == NULL || *tx == NULL) {
        return SW_INTERNAL_ERROR;
    }

    /* Validate data length */
    if (apdu->lc < 1) {
        return SW_WRONG_LENGTH;
    }

    /* Parse derivation path */
    path_bytes = crypto_parse_path(apdu->data, apdu->lc, &path);
    if (path_bytes == 0) {
        return SW_INVALID_PATH;
    }

    /* Validate path */
    if (!crypto_validate_path(&path)) {
        return SW_INVALID_PATH;
    }

    /* Derive public key */
    if (!crypto_derive_pubkey(&path, G_state.pubkey)) {
        SECURE_ZEROIZE(&path, sizeof(path));
        return SW_INTERNAL_ERROR;
    }

    /* Copy pubkey to output */
    memcpy(*tx, G_state.pubkey, PUBKEY_LEN);
    *tx += PUBKEY_LEN;

    /* Zeroize path */
    SECURE_ZEROIZE(&path, sizeof(path));

    return SW_OK;
}

uint16_t handle_get_address(const apdu_t *apdu, uint8_t **tx) {
    bip32_path_t path;
    size_t path_bytes;
    bool display;

    if (apdu == NULL || tx == NULL || *tx == NULL) {
        return SW_INTERNAL_ERROR;
    }

    /* P1: 0x00 = no display, 0x01 = display */
    display = (apdu->p1 == 0x01);

    /* Validate data length */
    if (apdu->lc < 1) {
        return SW_WRONG_LENGTH;
    }

    /* Parse derivation path */
    path_bytes = crypto_parse_path(apdu->data, apdu->lc, &path);
    if (path_bytes == 0) {
        return SW_INVALID_PATH;
    }

    /* Validate path */
    if (!crypto_validate_path(&path)) {
        SECURE_ZEROIZE(&path, sizeof(path));
        return SW_INVALID_PATH;
    }

    /* Derive address */
    if (!sumchain_get_address_for_path(&path, display, G_state.address_str, sizeof(G_state.address_str))) {
        SECURE_ZEROIZE(&path, sizeof(path));
        return SW_INTERNAL_ERROR;
    }

    /* Copy address string to output (excluding null terminator) */
    size_t addr_len = strlen(G_state.address_str);
    memcpy(*tx, G_state.address_str, addr_len);
    *tx += addr_len;

    /* Zeroize path */
    SECURE_ZEROIZE(&path, sizeof(path));

    return SW_OK;
}

/*
 * INS_SIGN_TX handler - streaming transaction signing
 *
 * Flow:
 * 1. First chunk (P1=0x00): Parse path, init session, start hashing/parsing tx data
 * 2. Continuation chunks (P1=0x80): Continue hashing/parsing
 * 3. Last chunk (P2=0x00): Finalize parsing, display for approval, sign and return
 */
uint16_t handle_sign_tx(const apdu_t *apdu, uint8_t **tx) {
    sign_session_t *session = &G_state.sign_session;

    if (apdu == NULL || tx == NULL || *tx == NULL) {
        return SW_INTERNAL_ERROR;
    }

    bool is_first = (apdu->p1 == P1_FIRST_CHUNK);
    bool is_more  = (apdu->p2 == P2_MORE_CHUNKS);

    /* Validate P1/P2 combinations */
    if (apdu->p1 != P1_FIRST_CHUNK && apdu->p1 != P1_MORE_CHUNK) {
        reset_sign_session();
        return SW_INVALID_P1P2;
    }
    if (apdu->p2 != P2_LAST_CHUNK && apdu->p2 != P2_MORE_CHUNKS) {
        reset_sign_session();
        return SW_INVALID_P1P2;
    }

    /*
     * First chunk handling
     */
    if (is_first) {
        /* Reset any existing session */
        reset_sign_session();

        /* Validate minimum length for path */
        if (apdu->lc < 1) {
            return SW_WRONG_LENGTH;
        }

        /* Parse derivation path from start of data */
        size_t path_bytes = crypto_parse_path(apdu->data, apdu->lc, &session->path);
        if (path_bytes == 0) {
            reset_sign_session();
            return SW_INVALID_PATH;
        }

        /* Validate path */
        if (!crypto_validate_path(&session->path)) {
            reset_sign_session();
            return SW_INVALID_PATH;
        }

        /* Initialize hash context */
        sum_blake3_init(&session->tx_hash_ctx);

        /* Initialize parser */
        tx_parser_init(&session->parser);

        /* Mark session as initialized */
        session->initialized = true;
        session->total_received = 0;
        session->last_chunk_received = !is_more;

        /* Process remaining data after path as tx bytes */
        const uint8_t *tx_data = apdu->data + path_bytes;
        size_t tx_len = apdu->lc - path_bytes;

        if (tx_len > 0) {
            /* Check against max tx size */
            if (session->total_received + tx_len > MAX_TX_SIZE) {
                reset_sign_session();
                return SW_TX_TOO_LARGE;
            }

            /* Feed to hash context */
            sum_blake3_update(&session->tx_hash_ctx, tx_data, tx_len);

            /* Feed to parser */
            size_t consumed = tx_parser_consume(&session->parser, tx_data, tx_len);
            if (consumed != tx_len || tx_parser_has_error(&session->parser)) {
                reset_sign_session();
                return SW_TX_PARSE_ERROR;
            }

            session->total_received += tx_len;
        }
    }
    /*
     * Continuation chunk handling
     */
    else {
        /* Must have an active session */
        if (!session->initialized) {
            return SW_SESSION_ERROR;
        }

        /* Already received last chunk - error */
        if (session->last_chunk_received) {
            reset_sign_session();
            return SW_SESSION_ERROR;
        }

        session->last_chunk_received = !is_more;

        if (apdu->lc > 0) {
            /* Check against max tx size */
            if (session->total_received + apdu->lc > MAX_TX_SIZE) {
                reset_sign_session();
                return SW_TX_TOO_LARGE;
            }

            /* Feed to hash context */
            sum_blake3_update(&session->tx_hash_ctx, apdu->data, apdu->lc);

            /* Feed to parser */
            size_t consumed = tx_parser_consume(&session->parser, apdu->data, apdu->lc);
            if (consumed != apdu->lc || tx_parser_has_error(&session->parser)) {
                reset_sign_session();
                return SW_TX_PARSE_ERROR;
            }

            session->total_received += apdu->lc;
        }
    }

    /*
     * If this was the last chunk, finalize and sign
     */
    if (!is_more) {
        /* Ensure parsing completed successfully */
        if (!tx_parser_is_done(&session->parser)) {
            reset_sign_session();
            return SW_TX_PARSE_ERROR;
        }

        /* Get parsed data */
        const tx_parsed_t *parsed = tx_parser_get_parsed(&session->parser);
        if (parsed == NULL) {
            reset_sign_session();
            return SW_INTERNAL_ERROR;
        }

        /* Format for display */
        tx_display_t display;
        if (!tx_display_format(parsed, &display)) {
            reset_sign_session();
            return SW_INTERNAL_ERROR;
        }

        /* If fee overflowed, reject for safety */
        if (parsed->fee_overflow) {
            reset_sign_session();
            return SW_TX_OVERFLOW;
        }

        /* Show approval UI and wait for user decision */
        ui_result_t result = tx_display_show_approval(&display);
        if (result != UI_RESULT_APPROVED) {
            reset_sign_session();
            return SW_USER_REJECTED;
        }

        /* User approved - finalize hash and sign */
        sum_blake3_finalize32(&session->tx_hash_ctx, G_state.hash);

        /* Sign the hash */
        if (!crypto_sign_hash(&session->path, G_state.hash, G_state.signature)) {
            SECURE_ZEROIZE(G_state.hash, sizeof(G_state.hash));
            reset_sign_session();
            return SW_INTERNAL_ERROR;
        }

        /* Copy signature to output */
        memcpy(*tx, G_state.signature, SIGNATURE_LEN);
        *tx += SIGNATURE_LEN;

        /* Cleanup */
        SECURE_ZEROIZE(G_state.hash, sizeof(G_state.hash));
        SECURE_ZEROIZE(G_state.signature, sizeof(G_state.signature));
        reset_sign_session();

        return SW_OK;
    }

    /* More chunks expected - return OK with no data */
    return SW_OK;
}

uint16_t apdu_dispatch(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                       uint8_t lc, uint8_t *data, uint8_t **tx) {
    apdu_t apdu = {
        .cla = cla,
        .ins = ins,
        .p1 = p1,
        .p2 = p2,
        .lc = lc,
        .data = data
    };

    /* Check CLA */
    if (cla != CLA_SUMCHAIN) {
        return SW_CLA_NOT_SUPPORTED;
    }

    /* Dispatch based on INS */
    switch (ins) {
        case INS_GET_VERSION:
            return handle_get_version(&apdu, tx);

        case INS_GET_APP_NAME:
            return handle_get_app_name(&apdu, tx);

        case INS_GET_PUBLIC_KEY:
            return handle_get_public_key(&apdu, tx);

        case INS_GET_ADDRESS:
            return handle_get_address(&apdu, tx);

        case INS_SIGN_TX:
            return handle_sign_tx(&apdu, tx);

        default:
            return SW_INS_NOT_SUPPORTED;
    }
}
