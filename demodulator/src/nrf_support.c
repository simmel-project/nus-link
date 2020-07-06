#include <stdint.h>

#define BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS 0x0007E000
// This must match the value in `nrf52833_s140_v7.ld`
#define BOOTLOADER_REGION_START                                                \
    0x00070000 /**< This field should correspond to start address of the       \
                  bootloader, found in UICR.RESERVED, 0x10001014, register.    \
                  This value is used for sanity check, so the bootloader will  \
                  fail immediately if this value differs from runtime value.   \
                  The value is used to determine max application size for      \
                  updating. */
#define CODE_PAGE_SIZE                                                         \
    0x1000 /**< Size of a flash codepage. Used for size of the reserved flash  \
              space in the bootloader region. Will be runtime checked against  \
              NRF_UICR->CODEPAGESIZE to ensure the region is correct. */
uint8_t m_mbr_params_page[CODE_PAGE_SIZE] __attribute__((section(
    ".mbrParamsPage"))); /**< This variable reserves a codepage for mbr
                            parameters, to ensure the compiler doesn't locate
                            any code or variables at his location. */
volatile uint32_t m_uicr_mbr_params_page_address
    __attribute__((section(".uicrMbrParamsPageAddress"))) =
        BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS;

uint8_t m_boot_settings[CODE_PAGE_SIZE] __attribute__((
    section(".bootloaderSettings"))); /**< This variable reserves a codepage for
                                         bootloader specific settings, to ensure
                                         the compiler doesn't locate any code or
                                         variables at his location. */
volatile uint32_t m_uicr_bootloader_start_address __attribute__((
    section(".uicrBootStartAddress"))) =
    BOOTLOADER_REGION_START; /**< This variable ensures that the linker script
                                will write the bootloader start address to the
                                UICR register. This value will be written in the
                                HEX file and thus written to UICR when the
                                bootloader is flashed into the chip. */

