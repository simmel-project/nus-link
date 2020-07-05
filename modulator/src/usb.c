#include <stdint.h>

#include "nrfx.h"
#include "nrfx_power.h"

#include "usb.h"

// tinyusb function that handles power event (detected, ready, removed)
// We must call it within SD's SOC event handler, or set it as power event
// handler if SD is not enabled.
void tusb_hal_nrf_power_event(uint32_t event);

void usb_init(void) {
    // Priorities 0, 1, 4 (nRF52) are reserved for SoftDevice
    // 2 is highest for application
    NVIC_SetPriority(USBD_IRQn, 2);

    // USB power may already be ready at this time -> no event generated
    // We need to invoke the handler based on the status initially
    uint32_t usb_reg;

    // Power module init
    const nrfx_power_config_t pwr_cfg = {0};
    nrfx_power_init(&pwr_cfg);

    // Register tusb function as USB power handler
    const nrfx_power_usbevt_config_t config = {
        .handler = (nrfx_power_usb_event_handler_t)tusb_hal_nrf_power_event};
    nrfx_power_usbevt_init(&config);

    nrfx_power_usbevt_enable();

    usb_reg = NRF_POWER->USBREGSTATUS;

    if (usb_reg & POWER_USBREGSTATUS_VBUSDETECT_Msk)
        tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_DETECTED);
    if (usb_reg & POWER_USBREGSTATUS_OUTPUTRDY_Msk)
        tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_READY);
}

// // echo to either Serial0 or Serial1
// // with Serial0 as all lower case, Serial1 as all upper case
// static void echo_serial_port(uint8_t itf, uint8_t buf[], uint32_t count) {
//     for (uint32_t i = 0; i < count; i++) {
//         if (itf == 0) {
//             // echo back 1st port as lower case
//             // if (isupper(buf[i])) buf[i] += 'a' - 'A';
//             buf[i] += 'a' - 'A';
//         } else {
//             // echo back additional ports as upper case
//             buf[i] -= 'a' - 'A';
//             // if (islower(buf[i])) buf[i] -= 'a' - 'A';
//         }

//         tud_cdc_n_write_char(itf, buf[i]);

//         if (buf[i] == '\r') tud_cdc_n_write_char(itf, '\n');
//     }
//     tud_cdc_n_write_flush(itf);
// }

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void) {
    uint8_t itf;

    for (itf = 0; itf < CFG_TUD_CDC; itf++) {
        if (tud_cdc_n_connected(itf)) {
            if (tud_cdc_n_available(itf)) {
                uint8_t buf[64];

                (void) tud_cdc_n_read(itf, buf, sizeof(buf));

                // // echo back to both serial ports
                // echo_serial_port(0, buf, count);
                // echo_serial_port(1, buf, count);
            }
        }
    }
}

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USBD_IRQHandler(void)
{
  tud_int_handler(0);
}
