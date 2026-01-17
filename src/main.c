/*
 * SUM Chain Ledger App - Main Entry Point
 */

#include "globals.h"
#include "apdu_handlers.h"
#include <string.h>

#ifdef HAVE_BOLOS_SDK
#include "os.h"
#include "cx.h"
#include "ux.h"
#include "os_io_seproxyhal.h"

/* Global state */
app_state_t G_app_state;

/* UX state */
ux_state_t G_ux;
bolos_ux_params_t G_ux_params;

/* I/O buffer */
uint8_t G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

/* Idle menu */
UX_STEP_NOCB(
    ux_idle_step_ready,
    pnn,
    {
        &C_sumchain_icon,
        "SUM Chain",
        "is ready",
    });

UX_STEP_NOCB(
    ux_idle_step_version,
    bn,
    {
        "Version",
        APPVERSION,
    });

UX_STEP_CB(
    ux_idle_step_quit,
    pb,
    os_sched_exit(-1),
    {
        &C_icon_dashboard_x,
        "Quit",
    });

UX_FLOW(ux_idle_flow,
    &ux_idle_step_ready,
    &ux_idle_step_version,
    &ux_idle_step_quit,
    FLOW_LOOP);

/* Button push callback */
void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default(element);
}

/* Return to idle menu */
static void ui_idle(void) {
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
}

/* General status callback */
uint8_t io_event(uint8_t channel) {
    (void)channel;

    switch (G_io_seproxyhal_spi_buffer[0]) {
        case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
            UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
            break;

        case SEPROXYHAL_TAG_STATUS_EVENT:
            if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&
                !(U4BE(G_io_seproxyhal_spi_buffer, 3) &
                  SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
                THROW(EXCEPTION_IO_RESET);
            }
            /* Fall through */
            __attribute__((fallthrough));

        case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
            UX_DISPLAYED_EVENT({});
            break;

        case SEPROXYHAL_TAG_TICKER_EVENT:
            UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {});
            break;

        default:
            UX_DEFAULT_EVENT();
            break;
    }

    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    return 1;
}

/* APDU handling */
static void app_main(void) {
    volatile unsigned int rx = 0;
    volatile unsigned int tx = 0;
    volatile unsigned int flags = 0;

    /* Reset signing session on startup */
    reset_sign_session();

    /* Exchange APDUs */
    for (;;) {
        volatile unsigned short sw = 0;

        BEGIN_TRY {
            TRY {
                rx = tx;
                tx = 0;
                rx = io_exchange(CHANNEL_APDU | flags, rx);
                flags = 0;

                /* Minimum APDU length check */
                if (rx < 4) {
                    THROW(SW_WRONG_LENGTH);
                }

                /* Extract APDU fields */
                uint8_t cla = G_io_apdu_buffer[0];
                uint8_t ins = G_io_apdu_buffer[1];
                uint8_t p1  = G_io_apdu_buffer[2];
                uint8_t p2  = G_io_apdu_buffer[3];
                uint8_t lc  = 0;
                uint8_t *data = NULL;

                if (rx > 4) {
                    lc = G_io_apdu_buffer[4];
                    /* Validate Lc against received data */
                    if (rx < 5 + lc) {
                        THROW(SW_WRONG_LENGTH);
                    }
                    data = G_io_apdu_buffer + 5;
                }

                /* Dispatch APDU */
                uint8_t *tx_ptr = G_io_apdu_buffer;
                sw = apdu_dispatch(cla, ins, p1, p2, lc, data, &tx_ptr);
                tx = tx_ptr - G_io_apdu_buffer;

                THROW(sw);
            }
            CATCH(EXCEPTION_IO_RESET) {
                THROW(EXCEPTION_IO_RESET);
            }
            CATCH_OTHER(e) {
                switch (e & 0xF000) {
                    case 0x6000:
                    case 0x9000:
                        sw = e;
                        break;
                    default:
                        sw = 0x6800 | (e & 0x7FF);
                        break;
                }
                /* Clear session on error (except for continuation chunks) */
                if (sw != SW_OK) {
                    /* Only reset if it's a real error, not a user rejection */
                    if (sw != SW_USER_REJECTED) {
                        /* Session reset handled in handlers */
                    }
                }
            }
            FINALLY {
                /* Append status word */
                G_io_apdu_buffer[tx++] = sw >> 8;
                G_io_apdu_buffer[tx++] = sw & 0xFF;
            }
        }
        END_TRY;
    }
}

/* Application entry point */
__attribute__((section(".boot"))) int main(void) {
    __asm volatile("cpsie i");

    /* Ensure exception will work as planned */
    os_boot();

    for (;;) {
        UX_INIT();

        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

#ifdef TARGET_NANOX
                /* Initialize BLE if available */
                G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif

                USB_power(0);
                USB_power(1);

                ui_idle();

#ifdef HAVE_BLE
                BLE_power(0, NULL);
                BLE_power(1, NULL);
#endif

                app_main();
            }
            CATCH(EXCEPTION_IO_RESET) {
                /* Reset IO and UX */
                continue;
            }
            CATCH_ALL {
                /* Exit on other exceptions */
                break;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

    return 0;
}

#else
/* Host-side test stub */

app_state_t G_app_state;

int main(void) {
    /* For host testing, just return */
    return 0;
}

#endif /* HAVE_BOLOS_SDK */
