# SUM Chain Ledger App

A Ledger hardware wallet application for the SUM Chain blockchain. This app enables secure key management, address derivation, and transaction signing on Ledger Nano S+, Nano X, Stax, and Flex devices.

## Features

- Ed25519 key derivation using BIP32-Ed25519 (SLIP-0010)
- BLAKE3 hashing for address derivation and transaction signing
- Address format: Base58 encoded BLAKE3 hash truncation
- Streaming transaction parsing (no full-tx RAM buffering)
- On-device transaction review before signing

## Architecture

### Cryptographic Components

| Component | Algorithm | Notes |
|-----------|-----------|-------|
| Key derivation | BIP32-Ed25519 (SLIP-0010) | Hardened paths only |
| Public key | Ed25519 | 32-byte compressed |
| Hashing | BLAKE3 | Official reference v1.8.3, portable backend |
| Address | BLAKE3(pubkey)[12:32] | 20 bytes, Base58 encoded |
| Signing | Ed25519 | 64-byte signature |

### Address Derivation

```
pubkey (32 bytes) -> BLAKE3 hash (32 bytes) -> bytes[12:31] (20 bytes) -> Base58
```

The derivation path follows the pattern:
```
m/44'/12345'/account'/change'/index'
```

Note: `12345'` is a placeholder coin type. Replace with the registered SLIP-0044 coin type for SUM Chain.

### Transaction Format

Transfer transactions (tx_type = 0x00):

| Field | Size | Encoding |
|-------|------|----------|
| version | 1 byte | uint8 |
| chain_id | 8 bytes | uint64 LE |
| sender | 20 bytes | raw |
| nonce | 8 bytes | uint64 LE |
| gas_price | 8 bytes | uint64 LE |
| gas_limit | 8 bytes | uint64 LE |
| tx_type | 1 byte | 0x00 for transfer |
| recipient | 20 bytes | raw |
| amount | 8 bytes | uint64 LE |

Total: 82 bytes for a transfer transaction.

## APDU Commands

| INS | Command | Description |
|-----|---------|-------------|
| 0x00 | GET_VERSION | Returns app version (3 bytes: major, minor, patch) |
| 0x01 | GET_APP_NAME | Returns "SUM Chain" |
| 0x02 | GET_PUBLIC_KEY | Derives and returns 32-byte public key |
| 0x03 | GET_ADDRESS | Derives and returns Base58 address |
| 0x04 | SIGN_TX | Signs a transaction (streaming) |

### GET_PUBLIC_KEY / GET_ADDRESS

Request:
```
CLA: 0xE0
INS: 0x02 (pubkey) or 0x03 (address)
P1:  0x00 (no display) or 0x01 (display on device)
P2:  0x00
Data: [path_len:1] [path[0]:4 BE] [path[1]:4 BE] ...
```

### SIGN_TX

Chunked transaction signing with streaming hash:

First chunk:
```
CLA: 0xE0
INS: 0x04
P1:  0x00 (first)
P2:  0x00 (last) or 0x80 (more)
Data: [path...] [tx_bytes...]
```

Continuation chunks:
```
CLA: 0xE0
INS: 0x04
P1:  0x80 (continuation)
P2:  0x00 (last) or 0x80 (more)
Data: [tx_bytes...]
```

Response (on last chunk, after user approval):
```
[signature:64 bytes] [SW:2 bytes]
```

## Building

### Prerequisites

- Ledger BOLOS SDK (set `BOLOS_SDK` environment variable)
- ARM GCC toolchain
- Python 3 (for Ledger tools)

### Build for Nano S+

```bash
export BOLOS_SDK=/path/to/sdk
make TARGET=nanos2
```

### Build for Nano X

```bash
export BOLOS_SDK=/path/to/sdk
make TARGET=nanox
```

### Run Unit Tests (Host)

```bash
cd tests
make test
```

## Project Structure

```
sumchain-ledger-app/
  src/
    main.c              # Application entry point
    globals.h           # Global state and definitions
    crypto.c/h          # Ed25519 key derivation and signing
    address.c/h         # Address derivation and Base58 encoding
    apdu_handlers.c/h   # APDU command handlers
    tx_parser.c/h       # Streaming transaction parser
    tx_display.c/h      # Transaction display formatting
    crypto/
      sum_blake3.c/h    # BLAKE3 wrapper
      blake3/           # BLAKE3 portable implementation
  tests/
    test_blake3.c       # BLAKE3 unit tests
    test_address.c      # Address derivation tests
    test_tx_parser.c    # Transaction parser tests
  icons/                # Application icons
  Makefile
```

## Security Considerations

### Implemented

- All APDU input lengths validated before access
- Derivation path validation (hardened-only for Ed25519)
- Private key material zeroized immediately after use
- Hash context zeroized after finalization
- Session state cleared on errors
- No dynamic memory allocation
- Streaming parser prevents full-tx RAM buffering
- Fee overflow detection (128-bit multiplication)
- On-device display required before signing

### Sensitive Data Handling

This repository contains no private keys, seed phrases, or credentials. The `.gitignore` file is configured to prevent accidental commits of:

- Private key files (*.pem, *.key, etc.)
- Seed phrase files (seed.txt, mnemonic.txt, etc.)
- Environment files with secrets (.env)
- Wallet files and keystores

Never commit test seeds or real credentials to this repository.

## Testing with Speculos

```bash
# Install Speculos
pip install speculos

# Run the app in emulator
speculos --model nanosp bin/app.elf

# In another terminal, send APDUs
echo "E000000000" | speculos-client
```

## Known Limitations and TODOs

1. **Coin type**: Using placeholder `12345'`. Update to registered SLIP-0044 type.
2. **Amount width**: Currently uint64. May need uint128 for large token amounts.
3. **Transaction types**: Only Transfer (0x00) supported. Add other types as needed.
4. **Endianness**: Assuming little-endian for all multi-byte integers.
5. **Icons**: Placeholder instructions provided. Generate actual bitmap icons.

## License

See LICENSE file.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Ensure all tests pass
4. Submit a pull request

Security-sensitive changes require additional review.
