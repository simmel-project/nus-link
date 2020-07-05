#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nrf.h"
#include "nrfx.h"
#include "nrfx_spi.h"


#define SPI_MOSI 9
#define SPI_MISO (4) // P1
#define SPI_SCK 10
#define SPI_CSn (6) // P1
void spi_init(void) {
  NRF_P1->OUTSET = (1 << SPI_CSn);
  NRF_P1->DIRSET = (1 << SPI_CSn);

  NRF_P0->DIRSET = (1 << SPI_SCK) | (1 << SPI_MOSI);
  NRF_P1->DIRCLR = (1 << SPI_MISO);

  NRF_SPI1->ENABLE = 1UL;
  NRF_SPI1->PSELSCK = SPI_SCK;
  NRF_SPI1->PSELMOSI = SPI_MOSI;
  NRF_SPI1->PSELMISO = 32 + SPI_MISO;
}

void spi_deinit(void) {
  NRF_SPI1->ENABLE = 0;

  // unsigned int i;
  // for (i = 0; i < 32; i++) {
  //   NRF_P0->PIN_CNF[i] |= 0x80000000;
  //   NRF_P1->PIN_CNF[i] |= 0x80000000;
  // }
}

void spi_select(void) { NRF_P1->OUTCLR = (1 << SPI_CSn); }

uint8_t spi_xfer(uint8_t data) {
  NRF_SPI1->TXD = data;
  while (NRF_SPI1->EVENTS_READY == 0)
    ;
  NRF_SPI1->EVENTS_READY = 0;
  return NRF_SPI1->RXD;
}

void spi_deselect(void) { NRF_P1->OUTSET = (1 << SPI_CSn); }
