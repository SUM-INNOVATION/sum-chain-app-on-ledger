/*
 * SUM Chain Ledger App - Streaming Transaction Parser
 * Parses transaction fields incrementally without buffering the entire tx.
 */

#ifndef TX_PARSER_H
#define TX_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize the transaction parser context.
 *
 * @param ctx Parser context to initialize.
 */
void tx_parser_init(tx_parser_ctx_t *ctx);

/*
 * Reset the parser to initial state.
 *
 * @param ctx Parser context.
 */
void tx_parser_reset(tx_parser_ctx_t *ctx);

/*
 * Feed data to the streaming parser.
 * Can be called multiple times with chunks of the transaction.
 *
 * @param ctx      Parser context.
 * @param data     Input data chunk.
 * @param data_len Length of input data.
 * @return Number of bytes consumed, or 0 on error (check ctx->state).
 */
size_t tx_parser_consume(tx_parser_ctx_t *ctx, const uint8_t *data, size_t data_len);

/*
 * Check if parsing is complete (all required fields received).
 *
 * @param ctx Parser context.
 * @return true if parsing completed successfully.
 */
bool tx_parser_is_done(const tx_parser_ctx_t *ctx);

/*
 * Check if parser is in error state.
 *
 * @param ctx Parser context.
 * @return true if an error occurred during parsing.
 */
bool tx_parser_has_error(const tx_parser_ctx_t *ctx);

/*
 * Get the parsed transaction data.
 * Only valid after tx_parser_is_done() returns true.
 *
 * @param ctx Parser context.
 * @return Pointer to parsed transaction data.
 */
const tx_parsed_t *tx_parser_get_parsed(const tx_parser_ctx_t *ctx);

/*
 * Compute the fee from gas_price * gas_limit with overflow detection.
 * Updates parsed->fee_low, parsed->fee_high, and parsed->fee_overflow.
 *
 * @param parsed Parsed transaction data to update.
 */
void tx_parser_compute_fee(tx_parsed_t *parsed);

/*
 * Securely zeroize the parser context.
 *
 * @param ctx Parser context to zeroize.
 */
void tx_parser_zeroize(tx_parser_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TX_PARSER_H */
