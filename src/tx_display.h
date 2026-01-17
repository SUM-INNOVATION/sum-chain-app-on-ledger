/*
 * SUM Chain Ledger App - Transaction Display
 * Formats transaction fields for on-device UI display.
 */

#ifndef TX_DISPLAY_H
#define TX_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum display string lengths */
#define TX_DISPLAY_AMOUNT_MAX_LEN    32   /* e.g., "18446744073709551615" + null */
#define TX_DISPLAY_FEE_MAX_LEN       40   /* "Overflow" or large number */
#define TX_DISPLAY_CHAIN_ID_MAX_LEN  24   /* Chain ID as decimal */

/*
 * Display strings for a transaction.
 */
typedef struct {
    char amount[TX_DISPLAY_AMOUNT_MAX_LEN];
    char recipient[ADDRESS_BASE58_MAX_LEN];
    char fee[TX_DISPLAY_FEE_MAX_LEN];
    char chain_id[TX_DISPLAY_CHAIN_ID_MAX_LEN];
    char sender[ADDRESS_BASE58_MAX_LEN];
    char nonce[TX_DISPLAY_AMOUNT_MAX_LEN];
} tx_display_t;

/*
 * Format the parsed transaction for display.
 *
 * @param parsed  Parsed transaction data.
 * @param display Output display strings.
 * @return true on success, false on error.
 */
bool tx_display_format(const tx_parsed_t *parsed, tx_display_t *display);

/*
 * Format a u64 value as a decimal string.
 *
 * @param value   Value to format.
 * @param out     Output buffer.
 * @param out_len Size of output buffer.
 * @return Number of characters written (excluding null), or 0 on error.
 */
size_t format_u64_decimal(uint64_t value, char *out, size_t out_len);

/*
 * Format a 20-byte address as Base58.
 *
 * @param addr20  20-byte address.
 * @param out     Output buffer.
 * @param out_len Size of output buffer.
 * @return Number of characters written (excluding null), or 0 on error.
 */
size_t format_address(const uint8_t addr20[20], char *out, size_t out_len);

/*
 * Show the transaction approval UI flow.
 * This function displays the transaction details and waits for user approval.
 *
 * @param display Formatted display strings.
 * @return UI_RESULT_APPROVED if user approved, UI_RESULT_REJECTED otherwise.
 */
ui_result_t tx_display_show_approval(const tx_display_t *display);

#ifdef __cplusplus
}
#endif

#endif /* TX_DISPLAY_H */
