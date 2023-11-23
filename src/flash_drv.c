
#include "flash_drv.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#define SPI_USE spi1
#define CS_PIN (13)

static __inline void __not_in_flash_func(spi_write_raw)(const uint8_t* src, size_t len) {
	spi_write_blocking(SPI_USE, src, len);
}

static __inline void __not_in_flash_func(spi_read_raw)(const uint8_t* src, uint8_t* read, size_t len) {
	spi_write_read_blocking(SPI_USE, src, read, len);
}

static __inline void __not_in_flash_func(spi_write)(const uint8_t* src, size_t len) {
	gpio_put(CS_PIN, false);
	spi_write_raw(src, len);
	gpio_put(CS_PIN, true);
}

static __inline void __not_in_flash_func(spi_read)(const uint8_t* src, uint8_t* read, size_t len) {
	gpio_put(CS_PIN, false);
	spi_write_read_blocking(SPI_USE, src, read, len);
	gpio_put(CS_PIN, true);
}

void flash_init(void) {
	uint8_t _a = 0x00;
	spi_init(SPI_USE, 8000000);
	spi_set_format(SPI_USE, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	gpio_set_function(12, GPIO_FUNC_SPI);
	gpio_init(CS_PIN);
	gpio_set_dir(CS_PIN, GPIO_OUT);
	gpio_put(CS_PIN, true);
	gpio_set_function(14, GPIO_FUNC_SPI);
	gpio_set_function(15, GPIO_FUNC_SPI);

	spi_write(&_a, 1);
}

const uint8_t F_WREN[] = { 0x06 };
const uint8_t F_CE[] = { 0x60 };
const uint8_t F_PE_TEST[] = { 0x81, 0x00, 0x00, 0x00 };
const uint8_t F_PP_TEST[] = { 0x02, 0x00, 0x00, 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0xAD, 0x01, 0x02, 0x03, 0x04 };
const uint8_t F_READ_TEST[] = { 0x03, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

const uint8_t F_STATU[] = { 0x05, 0xFF };
const uint8_t F_ID[] = { 0x9F, 0xFF, 0xFF, 0xFF };
uint8_t is_busy(void) {
	static uint8_t buffer[2];
	spi_read(F_STATU, buffer, 2);
	return buffer[1] & 0x01;
}

uint8_t* flash_ID(void) {
	static uint8_t buffer[4];
	spi_read(F_ID, buffer, 4);
	return &(buffer[1]);
}

void flash_CE(void) {
	spi_write(F_WREN, 1);
	spi_write(F_CE, 1);
}

void flash_write(uint32_t addr, const uint8_t* content, size_t len) {
	uint8_t head[4];
	spi_write(F_WREN, 1);
	head[0] = 0x02;
	head[1] = (addr >> 16) & 0xFF;
	head[2] = (addr >> 8) & 0xFF;
	head[3] = addr & 0xFF;
	gpio_put(CS_PIN, false);
	spi_write_raw(head, 4);
	spi_write_raw(content, len);
	gpio_put(CS_PIN, true);
}
const uint8_t dump[0x400] = { 0xFF };

void flash_read(uint32_t addr, uint8_t* dst, size_t len) {
	uint8_t head[4];
	head[0] = 0x03;
	head[1] = (addr >> 16) & 0xFF;
	head[2] = (addr >> 8) & 0xFF;
	head[3] = addr & 0xFF;

	gpio_put(CS_PIN, false);
	spi_write_raw(head, 4);
	spi_read_raw(dump, dst, len);
	gpio_put(CS_PIN, true);
}
