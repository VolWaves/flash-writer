
#ifndef _FLASH_DRV_H_
#define _FLASH_DRV_H_

#include <stdint.h>
#include <stddef.h>
void flash_init(void);
uint8_t is_busy(void);
uint8_t* flash_ID(void);
void flash_CE(void);
void flash_write(uint32_t addr, const uint8_t* content, size_t len);
void flash_read(uint32_t addr, uint8_t* dst, size_t len);

#endif
