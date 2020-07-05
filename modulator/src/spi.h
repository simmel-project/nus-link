#ifndef __SPI_H__
#define __SPI_H__

void spi_init(void);
void spi_deinit(void);
void spi_select(void);
void spi_deselect(void);
uint8_t spi_xfer(uint8_t data);

#endif /* __SPI_H__ */
