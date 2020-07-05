/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef SIMMEL_H
#define SIMMEL_H

/*------------------------------------------------------------------*/
/* LED
 *------------------------------------------------------------------*/
#define LEDS_NUMBER         1
#define LED_PRIMARY_PIN     6
#define LED_STATE_ON        1

//--------------------------------------------------------------------+
// Short these together to force bootloader entry
//--------------------------------------------------------------------+
#define LOOPBACK_TX_PIN         32
#define LOOPBACK_RX_PIN         24

//--------------------------------------------------------------------+
// USB
//--------------------------------------------------------------------+
#define USB_MANUFACTURER    "Betrusted"
#define USB_MODEL           "Simmel"
#define UF2_VOLUME_LABEL    "SIMMEL_BOOT"
#define UF2_PRODUCT_NAME    "Simmel"
#define UF2_BOARD_ID        "nRF52833-simmel-v1"
#define UF2_INDEX_URL       "https://github.com/simmel-project/"
#define USB_DESC_VID        0xC051
#define USB_DESC_UF2_PID    0x1209

#endif // SIMMEL_H
