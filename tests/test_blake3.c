/*
 * SUM Chain Ledger App - BLAKE3 Unit Tests
 *
 * Note: These tests verify internal consistency of the BLAKE3 implementation.
 * The implementation should be validated against official test vectors
 * before production use.
 */

#include "test_utils.h"
#include "sum_blake3.h"
#include <string.h>

void test_blake3_deterministic(void) {
    /* Same input should always produce same hash */
    uint8_t hash1[32], hash2[32];
    const uint8_t data[] = "test data for hashing";

    sum_blake3_hash(data, sizeof(data) - 1, hash1);
    sum_blake3_hash(data, sizeof(data) - 1, hash2);

    TEST_ASSERT_MEM_EQ(hash1, hash2, 32, "BLAKE3 deterministic output");
}

void test_blake3_different_inputs(void) {
    /* Different inputs should produce different hashes */
    uint8_t hash1[32], hash2[32];

    sum_blake3_hash((const uint8_t *)"input1", 6, hash1);
    sum_blake3_hash((const uint8_t *)"input2", 6, hash2);

    TEST_ASSERT_TRUE(memcmp(hash1, hash2, 32) != 0, "BLAKE3 different inputs -> different hashes");
}

void test_blake3_incremental_small(void) {
    /* Test that incremental hashing matches one-shot for small input */
    const uint8_t data[] = "The quick brown fox jumps over the lazy dog";
    size_t len = strlen((char *)data);

    /* One-shot hash */
    uint8_t hash_oneshot[32];
    sum_blake3_hash(data, len, hash_oneshot);

    /* Incremental hash - split at various points */
    sum_blake3_ctx_t ctx;
    uint8_t hash_inc[32];

    sum_blake3_init(&ctx);
    sum_blake3_update(&ctx, data, 10);
    sum_blake3_update(&ctx, data + 10, 20);
    sum_blake3_update(&ctx, data + 30, len - 30);
    sum_blake3_finalize32(&ctx, hash_inc);

    TEST_ASSERT_MEM_EQ(hash_oneshot, hash_inc, 32, "BLAKE3 incremental matches one-shot (small)");
}

void test_blake3_incremental_single_bytes(void) {
    /* Test incremental with single byte updates */
    const uint8_t data[] = "Hello World!";
    size_t len = strlen((char *)data);

    /* One-shot hash */
    uint8_t hash_oneshot[32];
    sum_blake3_hash(data, len, hash_oneshot);

    /* Single byte updates */
    sum_blake3_ctx_t ctx;
    uint8_t hash_inc[32];

    sum_blake3_init(&ctx);
    for (size_t i = 0; i < len; i++) {
        sum_blake3_update(&ctx, data + i, 1);
    }
    sum_blake3_finalize32(&ctx, hash_inc);

    TEST_ASSERT_MEM_EQ(hash_oneshot, hash_inc, 32, "BLAKE3 single-byte incremental");
}

void test_blake3_empty_updates(void) {
    /* Empty updates should not affect the hash */
    const uint8_t data[] = "test";
    size_t len = strlen((char *)data);

    uint8_t hash_normal[32], hash_with_empty[32];

    sum_blake3_hash(data, len, hash_normal);

    sum_blake3_ctx_t ctx;
    sum_blake3_init(&ctx);
    sum_blake3_update(&ctx, NULL, 0);  /* Empty update */
    sum_blake3_update(&ctx, data, len);
    sum_blake3_update(&ctx, NULL, 0);  /* Another empty update */
    sum_blake3_finalize32(&ctx, hash_with_empty);

    TEST_ASSERT_MEM_EQ(hash_normal, hash_with_empty, 32, "BLAKE3 empty updates don't affect hash");
}

void test_blake3_reset(void) {
    sum_blake3_ctx_t ctx;
    uint8_t hash1[32], hash2[32];

    /* First hash */
    sum_blake3_init(&ctx);
    sum_blake3_update(&ctx, (const uint8_t *)"test1", 5);
    sum_blake3_finalize32(&ctx, hash1);

    /* Reset and hash different data */
    sum_blake3_init(&ctx);  /* Re-init since finalize marks as not initialized */
    sum_blake3_update(&ctx, (const uint8_t *)"test2", 5);
    sum_blake3_finalize32(&ctx, hash2);

    /* Hashes should be different */
    TEST_ASSERT_TRUE(memcmp(hash1, hash2, 32) != 0, "BLAKE3 reset produces different hash");
}

void test_blake3_medium_input(void) {
    /* Test with data around one block size (64 bytes) */
    uint8_t data[100];
    memset(data, 0xAB, sizeof(data));

    /* One-shot */
    uint8_t hash_oneshot[32];
    sum_blake3_hash(data, sizeof(data), hash_oneshot);

    /* Incremental across block boundary */
    sum_blake3_ctx_t ctx;
    uint8_t hash_inc[32];

    sum_blake3_init(&ctx);
    sum_blake3_update(&ctx, data, 30);
    sum_blake3_update(&ctx, data + 30, 40);
    sum_blake3_update(&ctx, data + 70, 30);
    sum_blake3_finalize32(&ctx, hash_inc);

    TEST_ASSERT_MEM_EQ(hash_oneshot, hash_inc, 32, "BLAKE3 medium input incremental");
}

void test_blake3_block_boundary(void) {
    /* Test exactly at block boundary (64 bytes) */
    uint8_t data[64];
    memset(data, 0xCD, sizeof(data));

    uint8_t hash_oneshot[32];
    sum_blake3_hash(data, sizeof(data), hash_oneshot);

    sum_blake3_ctx_t ctx;
    uint8_t hash_inc[32];

    sum_blake3_init(&ctx);
    sum_blake3_update(&ctx, data, 32);
    sum_blake3_update(&ctx, data + 32, 32);
    sum_blake3_finalize32(&ctx, hash_inc);

    TEST_ASSERT_MEM_EQ(hash_oneshot, hash_inc, 32, "BLAKE3 block boundary incremental");
}

void test_blake3_chunk_boundary(void) {
    /* Test at chunk boundary (1024 bytes) */
    uint8_t data[1024];
    memset(data, 0xEF, sizeof(data));

    uint8_t hash_oneshot[32];
    sum_blake3_hash(data, sizeof(data), hash_oneshot);

    sum_blake3_ctx_t ctx;
    uint8_t hash_inc[32];

    sum_blake3_init(&ctx);
    sum_blake3_update(&ctx, data, 512);
    sum_blake3_update(&ctx, data + 512, 512);
    sum_blake3_finalize32(&ctx, hash_inc);

    TEST_ASSERT_MEM_EQ(hash_oneshot, hash_inc, 32, "BLAKE3 chunk boundary incremental");
}

void test_blake3_zeroize(void) {
    sum_blake3_ctx_t ctx;
    sum_blake3_init(&ctx);
    sum_blake3_update(&ctx, (const uint8_t *)"secret data", 11);

    /* Zeroize */
    sum_blake3_zeroize(&ctx);

    /* Check that ctx is zeroed */
    uint8_t zeros[sizeof(sum_blake3_ctx_t)];
    memset(zeros, 0, sizeof(zeros));

    TEST_ASSERT_MEM_EQ(&ctx, zeros, sizeof(ctx), "BLAKE3 context zeroized");
}

void test_blake3_output_length(void) {
    /* Verify we always get 32 bytes */
    uint8_t hash[32];
    memset(hash, 0xFF, sizeof(hash));

    sum_blake3_hash((const uint8_t *)"x", 1, hash);

    /* At least some bytes should have changed from 0xFF */
    int changes = 0;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != 0xFF) changes++;
    }

    TEST_ASSERT_TRUE(changes >= 20, "BLAKE3 produces 32-byte output with good distribution");
}

void run_blake3_tests(void) {
    TEST_SUITE_START("BLAKE3");

    test_blake3_deterministic();
    test_blake3_different_inputs();
    test_blake3_incremental_small();
    test_blake3_incremental_single_bytes();
    test_blake3_empty_updates();
    test_blake3_reset();
    test_blake3_medium_input();
    test_blake3_block_boundary();
    test_blake3_chunk_boundary();
    test_blake3_zeroize();
    test_blake3_output_length();

    TEST_SUITE_END();
}
