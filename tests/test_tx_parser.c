/*
 * SUM Chain Ledger App - Transaction Parser Unit Tests
 */

#include "test_utils.h"
#include "tx_parser.h"
#include "globals.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* Global state for tests (normally in main.c) */
app_state_t G_app_state;

/* Helper to build a Transfer transaction */
static size_t build_transfer_tx(uint8_t *buf, size_t buf_len,
                                uint8_t version,
                                uint64_t chain_id,
                                const uint8_t sender[20],
                                uint64_t nonce,
                                uint64_t gas_price,
                                uint64_t gas_limit,
                                const uint8_t recipient[20],
                                uint64_t amount) {
    if (buf_len < 82) return 0;  /* Minimum Transfer tx size */

    size_t pos = 0;

    /* Version (1 byte) */
    buf[pos++] = version;

    /* Chain ID (8 bytes LE) */
    for (int i = 0; i < 8; i++) {
        buf[pos++] = (uint8_t)(chain_id >> (i * 8));
    }

    /* Sender (20 bytes) */
    memcpy(&buf[pos], sender, 20);
    pos += 20;

    /* Nonce (8 bytes LE) */
    for (int i = 0; i < 8; i++) {
        buf[pos++] = (uint8_t)(nonce >> (i * 8));
    }

    /* Gas price (8 bytes LE) */
    for (int i = 0; i < 8; i++) {
        buf[pos++] = (uint8_t)(gas_price >> (i * 8));
    }

    /* Gas limit (8 bytes LE) */
    for (int i = 0; i < 8; i++) {
        buf[pos++] = (uint8_t)(gas_limit >> (i * 8));
    }

    /* Tx type (1 byte) - Transfer = 0x00 */
    buf[pos++] = TX_TYPE_TRANSFER;

    /* Recipient (20 bytes) */
    memcpy(&buf[pos], recipient, 20);
    pos += 20;

    /* Amount (8 bytes LE) */
    for (int i = 0; i < 8; i++) {
        buf[pos++] = (uint8_t)(amount >> (i * 8));
    }

    return pos;
}

void test_parser_simple_transfer(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0x11, sizeof(sender));
    memset(recipient, 0x22, sizeof(recipient));

    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        1,           /* version */
        1,           /* chain_id */
        sender,
        42,          /* nonce */
        1000,        /* gas_price */
        21000,       /* gas_limit */
        recipient,
        1000000      /* amount */
    );

    tx_parser_ctx_t ctx;
    tx_parser_init(&ctx);

    size_t consumed = tx_parser_consume(&ctx, tx, tx_len);

    TEST_ASSERT_EQ(consumed, tx_len, "Parser consumed all bytes");
    TEST_ASSERT_TRUE(tx_parser_is_done(&ctx), "Parser completed");
    TEST_ASSERT_FALSE(tx_parser_has_error(&ctx), "Parser no error");

    const tx_parsed_t *p = tx_parser_get_parsed(&ctx);
    TEST_ASSERT_EQ(p->version, 1, "Version correct");
    TEST_ASSERT_EQ(p->chain_id, 1, "Chain ID correct");
    TEST_ASSERT_MEM_EQ(p->sender, sender, 20, "Sender correct");
    TEST_ASSERT_EQ(p->nonce, 42, "Nonce correct");
    TEST_ASSERT_EQ(p->gas_price, 1000, "Gas price correct");
    TEST_ASSERT_EQ(p->gas_limit, 21000, "Gas limit correct");
    TEST_ASSERT_EQ(p->tx_type, TX_TYPE_TRANSFER, "Tx type correct");
    TEST_ASSERT_MEM_EQ(p->recipient, recipient, 20, "Recipient correct");
    TEST_ASSERT_EQ(p->amount, 1000000, "Amount correct");

    /* Check fee calculation */
    TEST_ASSERT_FALSE(p->fee_overflow, "Fee no overflow");
    TEST_ASSERT_EQ(p->fee_low, 1000ULL * 21000ULL, "Fee correct");
}

void test_parser_streaming_chunks(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0xAA, sizeof(sender));
    memset(recipient, 0xBB, sizeof(recipient));

    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        1, 12345, sender, 100, 5000, 50000, recipient, 999999999);

    tx_parser_ctx_t ctx;
    tx_parser_init(&ctx);

    /* Feed one byte at a time */
    size_t total_consumed = 0;
    for (size_t i = 0; i < tx_len; i++) {
        size_t consumed = tx_parser_consume(&ctx, &tx[i], 1);
        total_consumed += consumed;
        if (tx_parser_has_error(&ctx)) break;
    }

    TEST_ASSERT_EQ(total_consumed, tx_len, "Streaming: all bytes consumed");
    TEST_ASSERT_TRUE(tx_parser_is_done(&ctx), "Streaming: parser completed");

    const tx_parsed_t *p = tx_parser_get_parsed(&ctx);
    TEST_ASSERT_EQ(p->chain_id, 12345, "Streaming: chain_id correct");
    TEST_ASSERT_EQ(p->amount, 999999999, "Streaming: amount correct");
}

void test_parser_random_chunk_sizes(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0xCC, sizeof(sender));
    memset(recipient, 0xDD, sizeof(recipient));

    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        1, 99, sender, 1, 100, 1000, recipient, 12345678);

    /* Parse with random chunk sizes multiple times */
    srand(42);  /* Deterministic seed for reproducibility */

    for (int trial = 0; trial < 10; trial++) {
        tx_parser_ctx_t ctx;
        tx_parser_init(&ctx);

        size_t offset = 0;
        while (offset < tx_len && !tx_parser_has_error(&ctx) && !tx_parser_is_done(&ctx)) {
            size_t remaining = tx_len - offset;
            size_t chunk = (size_t)(rand() % 20) + 1;
            if (chunk > remaining) chunk = remaining;

            size_t consumed = tx_parser_consume(&ctx, &tx[offset], chunk);
            offset += consumed;
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "Random chunks trial %d completed", trial);
        TEST_ASSERT_TRUE(tx_parser_is_done(&ctx), msg);

        const tx_parsed_t *p = tx_parser_get_parsed(&ctx);
        TEST_ASSERT_EQ(p->amount, 12345678, "Random chunks: amount consistent");
    }
}

void test_parser_invalid_version(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0x00, sizeof(sender));
    memset(recipient, 0x00, sizeof(recipient));

    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        99,  /* Invalid version */
        1, sender, 0, 0, 0, recipient, 0);

    tx_parser_ctx_t ctx;
    tx_parser_init(&ctx);

    tx_parser_consume(&ctx, tx, tx_len);

    TEST_ASSERT_TRUE(tx_parser_has_error(&ctx), "Invalid version causes error");
}

void test_parser_unsupported_tx_type(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0x00, sizeof(sender));
    memset(recipient, 0x00, sizeof(recipient));

    /* Build a normal tx then change tx_type */
    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        1, 1, sender, 0, 0, 0, recipient, 0);

    /* Tx type is at offset: 1 + 8 + 20 + 8 + 8 + 8 = 53 */
    tx[53] = 0xFF;  /* Unsupported tx type */

    tx_parser_ctx_t ctx;
    tx_parser_init(&ctx);

    tx_parser_consume(&ctx, tx, tx_len);

    TEST_ASSERT_TRUE(tx_parser_has_error(&ctx), "Unsupported tx_type causes error");
}

void test_parser_truncated_tx(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0x00, sizeof(sender));
    memset(recipient, 0x00, sizeof(recipient));

    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        1, 1, sender, 0, 0, 0, recipient, 0);

    /* Only send partial data */
    tx_parser_ctx_t ctx;
    tx_parser_init(&ctx);

    tx_parser_consume(&ctx, tx, tx_len / 2);

    TEST_ASSERT_FALSE(tx_parser_is_done(&ctx), "Truncated tx not done");
    TEST_ASSERT_FALSE(tx_parser_has_error(&ctx), "Truncated tx no error yet");
}

void test_parser_fee_overflow(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0x00, sizeof(sender));
    memset(recipient, 0x00, sizeof(recipient));

    /* Use max values to cause overflow */
    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        1, 1, sender, 0,
        0xFFFFFFFFFFFFFFFFULL,  /* max gas_price */
        0xFFFFFFFFFFFFFFFFULL,  /* max gas_limit */
        recipient, 0);

    tx_parser_ctx_t ctx;
    tx_parser_init(&ctx);

    tx_parser_consume(&ctx, tx, tx_len);

    TEST_ASSERT_TRUE(tx_parser_is_done(&ctx), "Max values tx completed");

    const tx_parsed_t *p = tx_parser_get_parsed(&ctx);
    TEST_ASSERT_TRUE(p->fee_overflow, "Fee overflow detected");
}

void test_parser_fee_no_overflow(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0x00, sizeof(sender));
    memset(recipient, 0x00, sizeof(recipient));

    /* Use values that won't overflow */
    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        1, 1, sender, 0,
        1000000000ULL,   /* 1 gwei-ish */
        21000ULL,        /* standard gas limit */
        recipient, 0);

    tx_parser_ctx_t ctx;
    tx_parser_init(&ctx);

    tx_parser_consume(&ctx, tx, tx_len);

    const tx_parsed_t *p = tx_parser_get_parsed(&ctx);
    TEST_ASSERT_FALSE(p->fee_overflow, "Normal fee no overflow");
    TEST_ASSERT_EQ(p->fee_low, 1000000000ULL * 21000ULL, "Fee calculated correctly");
}

void test_parser_zeroize(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0xEE, sizeof(sender));
    memset(recipient, 0xFF, sizeof(recipient));

    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        1, 1, sender, 0, 0, 0, recipient, 12345);

    tx_parser_ctx_t ctx;
    tx_parser_init(&ctx);
    tx_parser_consume(&ctx, tx, tx_len);

    /* Zeroize */
    tx_parser_zeroize(&ctx);

    /* Check zeroed */
    uint8_t zeros[sizeof(tx_parser_ctx_t)];
    memset(zeros, 0, sizeof(zeros));

    TEST_ASSERT_MEM_EQ(&ctx, zeros, sizeof(ctx), "Parser context zeroized");
}

void test_parser_large_values(void) {
    uint8_t tx[128];
    uint8_t sender[20], recipient[20];

    memset(sender, 0x12, sizeof(sender));
    memset(recipient, 0x34, sizeof(recipient));

    /* Test with large but valid values */
    uint64_t large_chain_id = 0x123456789ABCDEF0ULL;
    uint64_t large_nonce = 0xFEDCBA9876543210ULL;
    uint64_t large_amount = 0x1000000000000000ULL;

    size_t tx_len = build_transfer_tx(tx, sizeof(tx),
        1, large_chain_id, sender, large_nonce,
        1000, 21000, recipient, large_amount);

    tx_parser_ctx_t ctx;
    tx_parser_init(&ctx);
    tx_parser_consume(&ctx, tx, tx_len);

    TEST_ASSERT_TRUE(tx_parser_is_done(&ctx), "Large values tx completed");

    const tx_parsed_t *p = tx_parser_get_parsed(&ctx);
    TEST_ASSERT_EQ(p->chain_id, large_chain_id, "Large chain_id correct");
    TEST_ASSERT_EQ(p->nonce, large_nonce, "Large nonce correct");
    TEST_ASSERT_EQ(p->amount, large_amount, "Large amount correct");
}

void run_tx_parser_tests(void) {
    TEST_SUITE_START("Transaction Parser");

    test_parser_simple_transfer();
    test_parser_streaming_chunks();
    test_parser_random_chunk_sizes();
    test_parser_invalid_version();
    test_parser_unsupported_tx_type();
    test_parser_truncated_tx();
    test_parser_fee_overflow();
    test_parser_fee_no_overflow();
    test_parser_zeroize();
    test_parser_large_values();

    TEST_SUITE_END();
}
