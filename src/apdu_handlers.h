/*
 * SUM Chain Ledger App - APDU Command Handlers
 */

#ifndef APDU_HANDLERS_H
#define APDU_HANDLERS_H

#include <stdint.h>
#include <stddef.h>
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * APDU buffer structure for convenience.
 */
typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc;
    uint8_t *data;
} apdu_t;

/*
 * Handle INS_GET_VERSION (0x00)
 * Returns the application version.
 *
 * @param apdu   Parsed APDU structure.
 * @param tx     Output buffer pointer (will be incremented).
 * @return Status word.
 */
uint16_t handle_get_version(const apdu_t *apdu, uint8_t **tx);

/*
 * Handle INS_GET_APP_NAME (0x01)
 * Returns the application name.
 *
 * @param apdu   Parsed APDU structure.
 * @param tx     Output buffer pointer (will be incremented).
 * @return Status word.
 */
uint16_t handle_get_app_name(const apdu_t *apdu, uint8_t **tx);

/*
 * Handle INS_GET_PUBLIC_KEY (0x02)
 * Derives and returns the public key for the given BIP32 path.
 *
 * Data format: [path_len:1] [path[0]:4 BE] [path[1]:4 BE] ...
 *
 * @param apdu   Parsed APDU structure.
 * @param tx     Output buffer pointer (will be incremented).
 * @return Status word.
 */
uint16_t handle_get_public_key(const apdu_t *apdu, uint8_t **tx);

/*
 * Handle INS_GET_ADDRESS (0x03)
 * Derives and returns the address for the given BIP32 path.
 * P1 = 0x00: Don't display on device
 * P1 = 0x01: Display on device for confirmation
 *
 * Data format: [path_len:1] [path[0]:4 BE] [path[1]:4 BE] ...
 *
 * @param apdu   Parsed APDU structure.
 * @param tx     Output buffer pointer (will be incremented).
 * @return Status word.
 */
uint16_t handle_get_address(const apdu_t *apdu, uint8_t **tx);

/*
 * Handle INS_SIGN_TX (0x04)
 * Signs a transaction using streaming BLAKE3 hash.
 *
 * P1 = 0x00: First chunk (includes derivation path)
 * P1 = 0x80: Continuation chunk
 *
 * P2 = 0x00: Last chunk
 * P2 = 0x80: More chunks to follow
 *
 * First chunk data format:
 *   [path_len:1] [path[0]:4 BE] ... [tx_bytes...]
 *
 * Continuation chunk data format:
 *   [tx_bytes...]
 *
 * @param apdu   Parsed APDU structure.
 * @param tx     Output buffer pointer (will be incremented).
 * @return Status word.
 */
uint16_t handle_sign_tx(const apdu_t *apdu, uint8_t **tx);

/*
 * Dispatch an APDU to the appropriate handler.
 *
 * @param cla    Class byte.
 * @param ins    Instruction byte.
 * @param p1     P1 parameter.
 * @param p2     P2 parameter.
 * @param lc     Data length.
 * @param data   Data buffer.
 * @param tx     Output buffer pointer.
 * @return Status word.
 */
uint16_t apdu_dispatch(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                       uint8_t lc, uint8_t *data, uint8_t **tx);

#ifdef __cplusplus
}
#endif

#endif /* APDU_HANDLERS_H */
