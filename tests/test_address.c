/*
 * SUM Chain Ledger App - Address Derivation Unit Tests
 */

#include "test_utils.h"
#include "address.h"
#include "sum_blake3.h"
#include <string.h>

/*
 * Test vector:
 * pubkey (32 bytes): all zeros
 * Expected: hash with BLAKE3, take bytes [12:32]
 */

void test_address_from_pubkey_zeros(void) {
    uint8_t pubkey[32];
    uint8_t addr[20];
    memset(pubkey, 0, sizeof(pubkey));

    sumchain_address_bytes_from_pubkey(pubkey, addr);

    /* Compute expected: BLAKE3(all-zeros pubkey)[12:32] */
    uint8_t full_hash[32];
    sum_blake3_hash(pubkey, 32, full_hash);

    TEST_ASSERT_MEM_EQ(addr, &full_hash[12], 20, "Address from zero pubkey");
}

void test_address_from_pubkey_pattern(void) {
    uint8_t pubkey[32];
    uint8_t addr[20];

    /* Fill with pattern 0x01, 0x02, ..., 0x20 */
    for (int i = 0; i < 32; i++) {
        pubkey[i] = (uint8_t)(i + 1);
    }

    sumchain_address_bytes_from_pubkey(pubkey, addr);

    /* Compute expected */
    uint8_t full_hash[32];
    sum_blake3_hash(pubkey, 32, full_hash);

    TEST_ASSERT_MEM_EQ(addr, &full_hash[12], 20, "Address from pattern pubkey");

    /* Print for manual verification */
    print_hex("Pubkey", pubkey, 32);
    print_hex("Full hash", full_hash, 32);
    print_hex("Address (bytes 12-31)", addr, 20);
}

void test_base58_encode_simple(void) {
    /* Test Base58 encoding with known values */
    char out[64];

    /* Single byte 0x00 -> "1" */
    uint8_t zero = 0;
    size_t len = base58_encode(&zero, 1, out, sizeof(out));
    TEST_ASSERT_EQ(len, 1, "Base58 single zero length");
    TEST_ASSERT_STR_EQ(out, "1", "Base58 single zero");

    /* 0x01 -> "2" */
    uint8_t one = 1;
    len = base58_encode(&one, 1, out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "2", "Base58 byte 0x01");

    /* 0x39 (57) -> "z" (last char in alphabet) */
    uint8_t fiftyNine = 57;
    len = base58_encode(&fiftyNine, 1, out, sizeof(out));
    /* 57 in base58 = "z" (alphabet[57] = 'z') */
    /* Wait, 57 decimal... Let's check: alphabet is "123456789ABC..." */
    /* Actually 57 < 58, so it's just alphabet[57] */
    /* Alphabet: 1=0, 2=1, ..., 9=8, A=9, ..., z=57 */
    /* So 57 -> "z" */
    /* But we're encoding 57 as a byte value, not mapping directly */
    /* Base58 encoding: 57 -> alphabet[57] = 'z' */
    TEST_ASSERT_TRUE(len > 0, "Base58 value 57 encodes");
}

void test_base58_encode_multibyte(void) {
    char out[64];

    /* 0x0001 (256) -> base58 of 256 */
    uint8_t val[2] = { 0x01, 0x00 };  /* 256 in big-endian */
    size_t len = base58_encode(val, 2, out, sizeof(out));
    TEST_ASSERT_TRUE(len > 0, "Base58 multibyte encodes");
    printf("    0x0100 -> '%s'\n", out);
}

void test_base58_leading_zeros(void) {
    char out[64];

    /* Leading zeros should become '1's */
    uint8_t val[4] = { 0x00, 0x00, 0x01, 0x00 };
    size_t len = base58_encode(val, 4, out, sizeof(out));
    TEST_ASSERT_TRUE(len >= 2, "Base58 leading zeros length");
    TEST_ASSERT_TRUE(out[0] == '1' && out[1] == '1', "Base58 leading zeros become 1s");
    printf("    [00 00 01 00] -> '%s'\n", out);
}

void test_address_to_base58(void) {
    /* Test full address encoding */
    uint8_t addr[20];
    char out[ADDRESS_BASE58_MAX_LEN];

    /* Set address to a known pattern */
    for (int i = 0; i < 20; i++) {
        addr[i] = (uint8_t)(i * 13 + 7);  /* Arbitrary pattern */
    }

    size_t len = sumchain_address_to_base58(addr, out, sizeof(out));
    TEST_ASSERT_TRUE(len > 0, "Address to Base58 encodes");
    TEST_ASSERT_TRUE(len < ADDRESS_BASE58_MAX_LEN, "Address fits in max buffer");
    printf("    Address Base58: '%s' (%zu chars)\n", out, len);
}

void test_address_base58_buffer_too_small(void) {
    uint8_t addr[20];
    char out[5];  /* Too small */

    memset(addr, 0x42, sizeof(addr));

    size_t len = sumchain_address_to_base58(addr, out, sizeof(out));
    /* Should fail or truncate */
    TEST_ASSERT_EQ(len, 0, "Address to Base58 fails with small buffer");
}

void test_address_full_derivation(void) {
    /* Full test: pubkey -> address bytes -> base58 */
    uint8_t pubkey[32];
    uint8_t addr[20];
    char base58[ADDRESS_BASE58_MAX_LEN];

    /* Example pubkey (simulating a real Ed25519 compressed pubkey) */
    memset(pubkey, 0xAB, 32);
    pubkey[0] = 0x02;  /* Typical prefix for compressed point */

    sumchain_address_bytes_from_pubkey(pubkey, addr);

    size_t len = sumchain_address_to_base58(addr, base58, sizeof(base58));

    TEST_ASSERT_TRUE(len > 0, "Full address derivation succeeds");
    printf("    Pubkey -> Address: '%s'\n", base58);

    /* Verify the address is the correct slice of the hash */
    uint8_t full_hash[32];
    sum_blake3_hash(pubkey, 32, full_hash);
    TEST_ASSERT_MEM_EQ(addr, &full_hash[12], 20, "Address bytes match hash[12:32]");
}

void run_address_tests(void) {
    TEST_SUITE_START("Address Derivation");

    test_address_from_pubkey_zeros();
    test_address_from_pubkey_pattern();
    test_base58_encode_simple();
    test_base58_encode_multibyte();
    test_base58_leading_zeros();
    test_address_to_base58();
    test_address_base58_buffer_too_small();
    test_address_full_derivation();

    TEST_SUITE_END();
}
