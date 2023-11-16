
#ifndef _WS2812_DRV_H_
#define _WS2812_DRV_H_
#include <stdint.h>
void ws2812_setup(void);
void ws2812_setpixel(uint32_t pixel_grb);

#endif
