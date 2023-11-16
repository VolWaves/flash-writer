#ifndef _USB_FUNC_H_
#define _USB_FUNC_H_
#include <stdint.h>

void cdc_task(void);
void cdc_log_print(char* str);
void cdc_log_init(void);

#endif
