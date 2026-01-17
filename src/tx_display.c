/*
 * SUM Chain Ledger App - Transaction Display Implementation
 */

#include "tx_display.h"
#include "address.h"
#include <string.h>

size_t format_u64_decimal(uint64_t value, char *out, size_t out_len) {
    if (out == NULL || out_len == 0) {
        return 0;
    }

    /* Handle zero case */
    if (value == 0) {
        if (out_len < 2) {
            out[0] = '\0';
            return 0;
        }
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }

    /* Build digits in reverse */
    char buf[24];  /* Max u64 is 20 digits */
    size_t pos = 0;

    while (value > 0 && pos < sizeof(buf)) {
        buf[pos++] = '0' + (char)(value % 10);
        value /= 10;
    }

    /* Check output buffer size */
    if (pos + 1 > out_len) {
        out[0] = '\0';
        return 0;
    }

    /* Reverse into output */
    for (size_t i = 0; i < pos; i++) {
        out[i] = buf[pos - 1 - i];
    }
    out[pos] = '\0';

    return pos;
}

/*
 * Format a 128-bit fee (low, high) as decimal string.
 * If overflow flag is set, return "Overflow".
 */
static size_t format_fee(uint64_t fee_low, uint64_t fee_high, bool overflow,
                         char *out, size_t out_len) {
    if (out == NULL || out_len == 0) {
        return 0;
    }

    if (overflow) {
        const char *msg = "Overflow";
        size_t len = strlen(msg);
        if (len + 1 > out_len) {
            out[0] = '\0';
            return 0;
        }
        memcpy(out, msg, len + 1);
        return len;
    }

    /* If high part is zero, just format the low part */
    if (fee_high == 0) {
        return format_u64_decimal(fee_low, out, out_len);
    }

    /*
     * 128-bit decimal conversion.
     * We need to handle up to 39 digits (2^128 - 1 ≈ 3.4e38).
     * Use repeated division by 10 on 128-bit value.
     */
    uint64_t lo = fee_low;
    uint64_t hi = fee_high;
    char buf[48];
    size_t pos = 0;

    while ((lo != 0 || hi != 0) && pos < sizeof(buf)) {
        /* Divide (hi:lo) by 10, get remainder */
        uint64_t hi_div = hi / 10;
        uint64_t hi_rem = hi % 10;

        /* Bring remainder into low part: (hi_rem * 2^64 + lo) / 10 */
        /* hi_rem * 2^64 = hi_rem << 64, but we need to handle this carefully */
        /* lo_new = ((hi_rem * 2^64) + lo) / 10 */
        /* = (hi_rem * 0x1999999999999999 + hi_rem * 6 + lo) / 10  (since 2^64 / 10 = 0x1999...9 * 10 + 6) */

        /* Simpler approach: use 128-bit arithmetic */
        /* (hi_rem << 64 | lo) / 10 */
        /* Since hi_rem < 10, (hi_rem << 64) < 2^68, which fits in 128 bits */

        /* We'll compute: quotient = (hi_rem * (2^64 / 10)) + ((hi_rem * (2^64 % 10) + lo) / 10) */
        /* 2^64 = 1844674407370955161 * 10 + 6 */
        const uint64_t q_factor = 1844674407370955161ULL;
        const uint64_t r_factor = 6;

        uint64_t lo_contrib = hi_rem * r_factor + lo;
        uint64_t lo_div = lo_contrib / 10;
        uint64_t lo_rem = lo_contrib % 10;

        uint64_t lo_new = hi_rem * q_factor + lo_div;

        buf[pos++] = '0' + (char)lo_rem;

        hi = hi_div;
        lo = lo_new;
    }

    /* Check output buffer size */
    if (pos + 1 > out_len) {
        out[0] = '\0';
        return 0;
    }

    /* Reverse into output */
    for (size_t i = 0; i < pos; i++) {
        out[i] = buf[pos - 1 - i];
    }
    out[pos] = '\0';

    return pos;
}

size_t format_address(const uint8_t addr20[20], char *out, size_t out_len) {
    return sumchain_address_to_base58(addr20, out, out_len);
}

bool tx_display_format(const tx_parsed_t *parsed, tx_display_t *display) {
    if (parsed == NULL || display == NULL) {
        return false;
    }

    memset(display, 0, sizeof(tx_display_t));

    /* Format amount */
    if (format_u64_decimal(parsed->amount, display->amount, sizeof(display->amount)) == 0) {
        return false;
    }

    /* Format recipient */
    if (format_address(parsed->recipient, display->recipient, sizeof(display->recipient)) == 0) {
        return false;
    }

    /* Format fee */
    if (format_fee(parsed->fee_low, parsed->fee_high, parsed->fee_overflow,
                   display->fee, sizeof(display->fee)) == 0) {
        return false;
    }

    /* Format chain_id */
    if (format_u64_decimal(parsed->chain_id, display->chain_id, sizeof(display->chain_id)) == 0) {
        return false;
    }

    /* Format sender */
    if (format_address(parsed->sender, display->sender, sizeof(display->sender)) == 0) {
        return false;
    }

    /* Format nonce */
    if (format_u64_decimal(parsed->nonce, display->nonce, sizeof(display->nonce)) == 0) {
        return false;
    }

    return true;
}

#ifdef HAVE_BOLOS_SDK

#include "ux.h"
#include "os.h"

/* UX flow for transaction approval (Nano S+/X style) */

/* Display buffers */
static char g_title[32];
static char g_value[64];

/* Current step in display flow */
static uint8_t g_display_step;
static tx_display_t *g_display_ptr;

/* Forward declarations */
static void display_next_step(void);

/* UX step definitions */
UX_STEP_NOCB(
    ux_tx_review_step,
    pnn,
    {
        &C_icon_eye,
        "Review",
        "Transaction",
    });

UX_STEP_NOCB(
    ux_tx_chain_step,
    bnnn_paging,
    {
        .title = "Chain ID",
        .text = g_display_ptr->chain_id,
    });

UX_STEP_NOCB(
    ux_tx_recipient_step,
    bnnn_paging,
    {
        .title = "To",
        .text = g_display_ptr->recipient,
    });

UX_STEP_NOCB(
    ux_tx_amount_step,
    bnnn_paging,
    {
        .title = "Amount",
        .text = g_display_ptr->amount,
    });

UX_STEP_NOCB(
    ux_tx_fee_step,
    bnnn_paging,
    {
        .title = "Max Fee",
        .text = g_display_ptr->fee,
    });

UX_STEP_CB(
    ux_tx_approve_step,
    pb,
    G_state.ui_result = UI_RESULT_APPROVED; ux_flow_over(),
    {
        &C_icon_validate_14,
        "Approve",
    });

UX_STEP_CB(
    ux_tx_reject_step,
    pb,
    G_state.ui_result = UI_RESULT_REJECTED; ux_flow_over(),
    {
        &C_icon_crossmark,
        "Reject",
    });

UX_FLOW(ux_tx_flow,
    &ux_tx_review_step,
    &ux_tx_chain_step,
    &ux_tx_recipient_step,
    &ux_tx_amount_step,
    &ux_tx_fee_step,
    &ux_tx_approve_step,
    &ux_tx_reject_step);

ui_result_t tx_display_show_approval(const tx_display_t *display) {
    if (display == NULL) {
        return UI_RESULT_REJECTED;
    }

    /* Store pointer for UX macros */
    g_display_ptr = (tx_display_t *)display;
    G_state.ui_result = UI_RESULT_NONE;

    /* If fee overflow, auto-reject for safety */
    if (strncmp(display->fee, "Overflow", 8) == 0) {
        return UI_RESULT_REJECTED;
    }

    /* Start UX flow */
    ux_flow_init(0, ux_tx_flow, NULL);

    /* Wait for user interaction (handled by event loop) */
    /* The result will be set by the callback and returned when flow completes */

    return G_state.ui_result;
}

#else
/* Stub for host-side testing */

ui_result_t tx_display_show_approval(const tx_display_t *display) {
    (void)display;
    /* In test mode, auto-approve */
    return UI_RESULT_APPROVED;
}

#endif /* HAVE_BOLOS_SDK */
