#*******************************************************************************
#   SUM Chain Ledger App - Host-side Unit Tests
#*******************************************************************************

CC = gcc
CFLAGS = -Wall -Wextra -g -O0
CFLAGS += -I../src -I../src/crypto -I../src/crypto/blake3
CFLAGS += -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512

# Source files from app
APP_SOURCES = \
    ../src/crypto/blake3/blake3.c \
    ../src/crypto/blake3/blake3_portable.c \
    ../src/crypto/blake3/blake3_dispatch.c \
    ../src/crypto/sum_blake3.c \
    ../src/address.c \
    ../src/tx_parser.c \
    ../src/tx_display.c \
    ../src/crypto.c

# Test sources
TEST_SOURCES = \
    test_blake3.c \
    test_address.c \
    test_tx_parser.c \
    test_main.c

# Objects
APP_OBJECTS = $(APP_SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

# Test binary
TEST_BIN = run_tests

.PHONY: all clean test

all: $(TEST_BIN)

$(TEST_BIN): $(APP_OBJECTS) $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(APP_OBJECTS) $(TEST_OBJECTS) $(TEST_BIN)
	rm -f ../src/*.o ../src/crypto/*.o ../src/crypto/blake3/*.o
