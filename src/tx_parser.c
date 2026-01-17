/*
 * SUM Chain Ledger App - Streaming Transaction Parser Implementation
 *
 * Transaction format (all multi-byte integers are little-endian):
 *   version      : 1 byte
 *   chain_id     : 8 bytes (u64 LE)
 *   sender       : 20 bytes
 *   nonce        : 8 bytes (u64 LE)
 *   gas_price    : 8 bytes (u64 LE)
 *   gas_limit    : 8 bytes (u64 LE)
 *   tx_type      : 1 byte
 *
 * For tx_type == 0x00 (Transfer):
 *   recipient    : 20 bytes
 *   amount       : 8 bytes (u64 LE)  [TODO: upgrade to u128 if needed]
 *
 * Total for Transfer: 1 + 8 + 20 + 8 + 8 + 8 + 1 + 20 + 8 = 82 bytes
 */

#include "tx_parser.h"
#include <string.h>

/* Field sizes */
#define FIELD_SIZE_VERSION    1
#define FIELD_SIZE_CHAIN_ID   8
#define FIELD_SIZE_SENDER     20
#define FIELD_SIZE_NONCE      8
#define FIELD_SIZE_GAS_PRICE  8
#define FIELD_SIZE_GAS_LIMIT  8
#define FIELD_SIZE_TX_TYPE    1
#define FIELD_SIZE_RECIPIENT  20
#define FIELD_SIZE_AMOUNT     8   /* TODO: 16 for u128 */

/* Helper: read u64 little-endian from buffer */
static uint64_t read_u64_le(const uint8_t *buf) {
    return ((uint64_t)buf[0])
         | ((uint64_t)buf[1] << 8)
         | ((uint64_t)buf[2] << 16)
         | ((uint64_t)buf[3] << 24)
         | ((uint64_t)buf[4] << 32)
         | ((uint64_t)buf[5] << 40)
         | ((uint64_t)buf[6] << 48)
         | ((uint64_t)buf[7] << 56);
}

/* Get the size of the current field being parsed */
static size_t get_field_size(tx_parse_state_t state) {
    switch (state) {
        case TX_PARSE_STATE_VERSION:    return FIELD_SIZE_VERSION;
        case TX_PARSE_STATE_CHAIN_ID:   return FIELD_SIZE_CHAIN_ID;
        case TX_PARSE_STATE_SENDER:     return FIELD_SIZE_SENDER;
        case TX_PARSE_STATE_NONCE:      return FIELD_SIZE_NONCE;
        case TX_PARSE_STATE_GAS_PRICE:  return FIELD_SIZE_GAS_PRICE;
        case TX_PARSE_STATE_GAS_LIMIT:  return FIELD_SIZE_GAS_LIMIT;
        case TX_PARSE_STATE_TX_TYPE:    return FIELD_SIZE_TX_TYPE;
        case TX_PARSE_STATE_RECIPIENT:  return FIELD_SIZE_RECIPIENT;
        case TX_PARSE_STATE_AMOUNT:     return FIELD_SIZE_AMOUNT;
        default:                        return 0;
    }
}

/* Process a complete field from the scratch buffer */
static bool process_complete_field(tx_parser_ctx_t *ctx) {
    tx_parsed_t *p = &ctx->parsed;

    switch (ctx->state) {
        case TX_PARSE_STATE_VERSION:
            p->version = ctx->scratch[0];
            /* Version validation: only version 1 supported for now */
            if (p->version != 1) {
                return false;
            }
            ctx->state = TX_PARSE_STATE_CHAIN_ID;
            break;

        case TX_PARSE_STATE_CHAIN_ID:
            p->chain_id = read_u64_le(ctx->scratch);
            ctx->state = TX_PARSE_STATE_SENDER;
            break;

        case TX_PARSE_STATE_SENDER:
            memcpy(p->sender, ctx->scratch, ADDRESS_LEN);
            ctx->state = TX_PARSE_STATE_NONCE;
            break;

        case TX_PARSE_STATE_NONCE:
            p->nonce = read_u64_le(ctx->scratch);
            ctx->state = TX_PARSE_STATE_GAS_PRICE;
            break;

        case TX_PARSE_STATE_GAS_PRICE:
            p->gas_price = read_u64_le(ctx->scratch);
            ctx->state = TX_PARSE_STATE_GAS_LIMIT;
            break;

        case TX_PARSE_STATE_GAS_LIMIT:
            p->gas_limit = read_u64_le(ctx->scratch);
            ctx->state = TX_PARSE_STATE_TX_TYPE;
            break;

        case TX_PARSE_STATE_TX_TYPE:
            p->tx_type = ctx->scratch[0];
            /* Route to tx-type-specific fields */
            if (p->tx_type == TX_TYPE_TRANSFER) {
                ctx->state = TX_PARSE_STATE_RECIPIENT;
            } else {
                /* Unsupported tx type */
                return false;
            }
            break;

        case TX_PARSE_STATE_RECIPIENT:
            memcpy(p->recipient, ctx->scratch, ADDRESS_LEN);
            ctx->state = TX_PARSE_STATE_AMOUNT;
            break;

        case TX_PARSE_STATE_AMOUNT:
            p->amount = read_u64_le(ctx->scratch);
            /* Parsing complete */
            ctx->state = TX_PARSE_STATE_DONE;
            /* Compute fee */
            tx_parser_compute_fee(p);
            break;

        default:
            return false;
    }

    /* Reset field offset for next field */
    ctx->field_offset = 0;
    return true;
}

void tx_parser_init(tx_parser_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    memset(ctx, 0, sizeof(tx_parser_ctx_t));
    ctx->state = TX_PARSE_STATE_VERSION;
    ctx->field_offset = 0;
    ctx->total_consumed = 0;
}

void tx_parser_reset(tx_parser_ctx_t *ctx) {
    tx_parser_init(ctx);
}

size_t tx_parser_consume(tx_parser_ctx_t *ctx, const uint8_t *data, size_t data_len) {
    if (ctx == NULL || data == NULL) {
        return 0;
    }

    if (ctx->state == TX_PARSE_STATE_DONE || ctx->state == TX_PARSE_STATE_ERROR) {
        return 0;
    }

    size_t consumed = 0;

    while (consumed < data_len && ctx->state != TX_PARSE_STATE_DONE && ctx->state != TX_PARSE_STATE_ERROR) {
        /* Check for maximum transaction size */
        if (ctx->total_consumed >= MAX_TX_SIZE) {
            ctx->state = TX_PARSE_STATE_ERROR;
            return consumed;
        }

        size_t field_size = get_field_size(ctx->state);
        if (field_size == 0) {
            ctx->state = TX_PARSE_STATE_ERROR;
            return consumed;
        }

        /* How many bytes still needed for current field */
        size_t needed = field_size - ctx->field_offset;
        size_t available = data_len - consumed;
        size_t take = (available < needed) ? available : needed;

        /* Bounds check on scratch buffer */
        if (ctx->field_offset + take > sizeof(ctx->scratch)) {
            ctx->state = TX_PARSE_STATE_ERROR;
            return consumed;
        }

        /* Copy into scratch buffer */
        memcpy(&ctx->scratch[ctx->field_offset], &data[consumed], take);
        ctx->field_offset += (uint8_t)take;
        consumed += take;
        ctx->total_consumed += take;

        /* If field complete, process it */
        if (ctx->field_offset >= field_size) {
            if (!process_complete_field(ctx)) {
                ctx->state = TX_PARSE_STATE_ERROR;
                return consumed;
            }
        }
    }

    return consumed;
}

bool tx_parser_is_done(const tx_parser_ctx_t *ctx) {
    return ctx != NULL && ctx->state == TX_PARSE_STATE_DONE;
}

bool tx_parser_has_error(const tx_parser_ctx_t *ctx) {
    return ctx != NULL && ctx->state == TX_PARSE_STATE_ERROR;
}

const tx_parsed_t *tx_parser_get_parsed(const tx_parser_ctx_t *ctx) {
    if (ctx == NULL) {
        return NULL;
    }
    return &ctx->parsed;
}

void tx_parser_compute_fee(tx_parsed_t *parsed) {
    if (parsed == NULL) {
        return;
    }

    /*
     * Compute fee = gas_price * gas_limit with 128-bit intermediate
     * to detect overflow. We split each 64-bit value into two 32-bit halves.
     */
    uint64_t a = parsed->gas_price;
    uint64_t b = parsed->gas_limit;

    uint32_t a_lo = (uint32_t)a;
    uint32_t a_hi = (uint32_t)(a >> 32);
    uint32_t b_lo = (uint32_t)b;
    uint32_t b_hi = (uint32_t)(b >> 32);

    /* 128-bit product: (a_hi*2^32 + a_lo) * (b_hi*2^32 + b_lo) */
    uint64_t lo_lo = (uint64_t)a_lo * b_lo;
    uint64_t lo_hi = (uint64_t)a_lo * b_hi;
    uint64_t hi_lo = (uint64_t)a_hi * b_lo;
    uint64_t hi_hi = (uint64_t)a_hi * b_hi;

    /* Combine with carries */
    uint64_t mid = lo_hi + hi_lo;
    uint64_t carry_from_mid = (mid < lo_hi) ? 1ULL : 0ULL;  /* Did mid overflow? */

    uint64_t result_lo = lo_lo + (mid << 32);
    uint64_t carry_to_hi = (result_lo < lo_lo) ? 1ULL : 0ULL;

    uint64_t result_hi = hi_hi + (mid >> 32) + (carry_from_mid << 32) + carry_to_hi;

    parsed->fee_low = result_lo;
    parsed->fee_high = result_hi;
    parsed->fee_overflow = (result_hi != 0);
}

void tx_parser_zeroize(tx_parser_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    SECURE_ZEROIZE(ctx, sizeof(tx_parser_ctx_t));
}
