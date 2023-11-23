
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/sync.h"

#include "scheduler/uevent.h"
#include "scheduler/scheduler.h"

#include "platform.h"

#include "tusb_config.h"

#include "ws2812_drv.h"

#define U32RGB(r, g, b) (((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b))

#include "pico/sync.h"
critical_section_t scheduler_lock;
static __inline void CRITICAL_REGION_INIT(void) {
	critical_section_init(&scheduler_lock);
}
static __inline void CRITICAL_REGION_ENTER(void) {
	critical_section_enter_blocking(&scheduler_lock);
}
static __inline void CRITICAL_REGION_EXIT(void) {
	critical_section_exit(&scheduler_lock);
}

bool log_silent = false;
uint32_t flash_busy_timeout = 0;

bool timer_4hz_callback(struct repeating_timer* t) {
	if(!log_silent) {
		LOG_RAW("At %lld ms\n", time_us_64() / 1000);
	}
	uevt_bc_e(UEVT_TIMER_4HZ);
	return true;
}

enum {
	ERR_CHECK,
	ERR_ID,
	ERR_ERASE,
	ERR_TIMEOUT,
} err_type;

typedef enum {
	LED_IDLE,
	LED_WORKING,
	LED_WORKING_ALT,
	LED_SUCCESS,
	LED_ERROR
} eLED_STATE;
eLED_STATE led_state = LED_IDLE;
uint8_t led_timeout = 0;

void __not_in_flash_func(led_set_state)(eLED_STATE state) {
	led_state = state;
	if(state != LED_IDLE) {
		led_timeout = 8;
	} else {
		led_timeout = 0;
		ws2812_setpixel(U32RGB(18, 18, 18));
	}
	switch(state) {
		case LED_WORKING:
			ws2812_setpixel(U32RGB(22, 22, 0));
			break;
		case LED_WORKING_ALT:
			ws2812_setpixel(U32RGB(22, 0, 22));
			break;
		case LED_SUCCESS:
			ws2812_setpixel(U32RGB(0, 30, 0));
			break;
		case LED_ERROR:
			ws2812_setpixel(U32RGB(30, 0, 0));
			break;
	}
}
void led_blink_routine(void) {
	static uint8_t _tick = 0;
	_tick += 1;
	if(led_timeout > 0) {
		led_timeout -= 1;
		if(led_timeout == 0) {
			led_state = LED_IDLE;
			ws2812_setpixel(U32RGB(0, 0, 0));
		}
	}
	if(led_state == LED_IDLE) {
		if(_tick & 0x1) {
			ws2812_setpixel(U32RGB(18, 18, 18));
		} else {
			ws2812_setpixel(U32RGB(0, 0, 0));
		}
	}
}

void temperature_routine(void) {
	static uint16_t tick = 0;
	static float _t = 0;
	if(tick > 8) {
		tick = 0;
		int32_t t_adc = adc_read();
		_t = 27.0 - ((t_adc - 876) / 2.136f);
		uevt_bc(UEVT_ADC_TEMPERATURE_RESULT, &_t);
	}
	tick += 1;
}
#include "flash_drv.h"

void test_routine(void) {
	static uint16_t flag = 0;
	flag += 1;
	if(flag == 6) {
		uevt_bc_e(UEVT_CTL_FLASH_START);
	}
}

#include "assets.h"
struct repeating_timer flash_timer;
static uint8_t flash_assets_n = 0;
static uint32_t flash_assets_offset = 0;
static const uint32_t flash_assets_length = 25600;
static uint32_t flash_addr = 0;
static uint32_t flash_check_addr;
static const uint8_t* flash_check_src;
static uint32_t flash_check_len;
enum {
	FLASH_CHECK,
	FLASH_CONTINUE
} flash_routine_next = FLASH_CHECK;
bool flash_routine_callback(struct repeating_timer* t) {
	if(!is_busy()) {
		flash_busy_timeout = 0;
		cancel_repeating_timer(t);
		if(flash_routine_next == FLASH_CHECK) {
			uevt_bc_e(UEVT_CTL_FLASH_CHECK);
		} else if(flash_routine_next == FLASH_CONTINUE) {
			uevt_bc_e(UEVT_CTL_FLASH_CONTINUE);
		}
		return false;
	}
	flash_busy_timeout += 1;
	if(flash_busy_timeout > 1000) {
		flash_busy_timeout = 0;
		cancel_repeating_timer(t);
		err_type = ERR_TIMEOUT;
		uevt_bc_e(UEVT_FLASH_ERROR);
		return false;
	}
	return true;
}

void __not_in_flash_func(main_handler)(uevt_t* evt) {
	static uint8_t flash_flag = 0;
	switch(evt->evt_id) {
		case UEVT_FLASH_ERROR:
			LOG_RAW("FLASH ERROR[%d]!!!\n", err_type);
			led_set_state(LED_ERROR);
			log_silent = false;
			break;
		case UEVT_FLASH_SUCCESS:
			LOG_RAW("FLASH SUCCEED!!!\n");
			led_set_state(LED_SUCCESS);
			log_silent = false;
			break;
		case UEVT_CTL_FLASH_START:
			log_silent = true;
			flash_addr = 0;
			flash_assets_offset = 0;
			flash_assets_n = 0;
			led_set_state(LED_WORKING);
			uint8_t* id = flash_ID();
			LOG_RAW("FLASH ID = %02X%02X%02X\n", id[0], id[1], id[2]);
			flash_CE();
			sleep_us(50);
			if(is_busy()) {
				uevt_bc_e(UEVT_CTL_FLASH_CONTINUE);
			} else {
				err_type = ERR_ERASE;
				uevt_bc_e(UEVT_FLASH_ERROR);
			}
			break;
		case UEVT_CTL_FLASH_CHECK:
			if(is_busy()) {
				flash_routine_next = FLASH_CHECK;
				add_repeating_timer_us(1500, flash_routine_callback, NULL, &flash_timer);
				break;
			}
			uint8_t check_buf[0x100];
			uint8_t* check_dst = check_buf;
			flash_read(flash_check_addr, check_buf, flash_check_len);
			for(size_t i = 0; i < 0x100; i++) {
				if(*flash_check_src++ != *check_dst++) {
					err_type = ERR_CHECK;
					uevt_bc_e(UEVT_FLASH_ERROR);
					return;
				}
			}
			uevt_bc_e(UEVT_CTL_FLASH_CONTINUE);
			break;
		case UEVT_CTL_FLASH_CONTINUE:
			if(flash_assets_n >= 31) {
				uevt_bc_e(UEVT_FLASH_SUCCESS);
				break;
			}
			if(flash_flag & 0x1) {
				led_set_state(LED_WORKING_ALT);
			} else {
				led_set_state(LED_WORKING);
			}
			if(is_busy()) {
				flash_routine_next = FLASH_CONTINUE;
				add_repeating_timer_us(1500, flash_routine_callback, NULL, &flash_timer);
			} else {
				uint32_t current_write_length = flash_assets_length - flash_assets_offset > 0x100 ? 0x100 : flash_assets_length - flash_assets_offset;
				flash_write(flash_addr + flash_assets_offset, &(fire_array[flash_assets_n]->map[flash_assets_offset]), current_write_length);
				flash_check_addr = flash_addr + flash_assets_offset;
				flash_check_src = &(fire_array[flash_assets_n]->map[flash_assets_offset]);
				flash_check_len = current_write_length;
				flash_assets_offset += 0x100;
				if(flash_assets_offset >= flash_assets_length) {
					LOG_RAW("ASSETS[%d] at 0x%06x\n", flash_assets_n, flash_addr);
					flash_assets_n += 1;
					flash_addr += flash_assets_offset;
					flash_assets_offset = 0;
					flash_flag += 1;
				}
				uevt_bc_e(UEVT_CTL_FLASH_CHECK);
			}
			break;

		case UEVT_TIMER_4HZ:
			// test_routine();
			led_blink_routine();
			temperature_routine();
			break;
		case UEVT_ADC_TEMPERATURE_RESULT:
			if(!log_silent) {
				LOG_RAW("Temperature is %0.1f\n", *((float*)(evt->content)));
			}
			break;
	}
}

void uevt_log(char* str) {
	LOG_RAW("%s\n", str);
}

static char serial_fifo[16];
static uint8_t serial_wp = 0;
uint8_t serial_got(const char* str) {
	uint8_t len = strlen(str);
	for(uint8_t i = 1; i <= len; i++) {
		if(serial_fifo[serial_wp + (0x10 - i) & 0xF] != str[len - i]) {
			return 0;
		}
	}
	return 1;
}
#include "pico/bootrom.h"
void serial_receive(uint8_t const* buffer, uint16_t bufsize) {
	for(uint16_t i = 0; i < bufsize; i++) {
		if((*buffer == 0x0A) || (*buffer == 0x0D)) {
			if(serial_got("BOOTLOADER")) {
				ws2812_setpixel(U32RGB(0, 0, 10));
				reset_usb_boot(0, 0);
			}
			if(serial_got("FLASH")) {
				uevt_bc_e(UEVT_CTL_FLASH_START);
			}
		} else {
			serial_fifo[serial_wp++ & 0xF] = *buffer++;
		}
	}
}

extern void cdc_task(void);
int __not_in_flash_func(main)() {
	CRITICAL_REGION_INIT();
	app_sched_init();
	user_event_init();
	user_event_handler_regist(main_handler);

	adc_init();
	adc_set_temp_sensor_enabled(true);
	adc_select_input(4);

	ws2812_setup();
	flash_init();

	struct repeating_timer timer;
	add_repeating_timer_ms(250, timer_4hz_callback, NULL, &timer);
	tusb_init();
	cdc_log_init();
	while(true) {
		app_sched_execute();
		tud_task();
		cdc_task();
		__wfi();
	}
}
