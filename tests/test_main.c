/*
 * SUM Chain Ledger App - Test Runner
 */

#include <stdio.h>

/* Test result counters - definition */
int g_tests_run = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;

#include "test_utils.h"

/* Test suite declarations */
extern void run_blake3_tests(void);
extern void run_address_tests(void);
extern void run_tx_parser_tests(void);

int main(void) {
    printf("SUM Chain Ledger App - Unit Tests\n");
    printf("========================================\n");

    run_blake3_tests();
    run_address_tests();
    run_tx_parser_tests();

    print_test_summary();

    return (g_tests_failed > 0) ? 1 : 0;
}
