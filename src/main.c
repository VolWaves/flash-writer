
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/sync.h"

#include "scheduler/uevent.h"
#include "scheduler/scheduler.h"

#include "platform.h"

#include "tusb_config.h"

#include "ws2812_drv.h"

#define LED_PIN PICO_DEFAULT_LED_PIN

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

bool timer_4hz_callback(struct repeating_timer* t) {
	LOG_RAW("At %lld ms\n", time_us_64() / 1000);
	uevt_bc_e(UEVT_TIMER_4HZ);
	return true;
}

void led_blink_routine(void) {
	static uint8_t _tick = 0;
	_tick += 1;
	if(_tick & 0x1) {
		gpio_put(LED_PIN, 1);
	} else {
		gpio_put(LED_PIN, 0);
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

void main_handler(uevt_t* evt) {
	switch(evt->evt_id) {
		case UEVT_TIMER_4HZ:
			led_blink_routine();
			temperature_routine();
			break;
		case UEVT_ADC_TEMPERATURE_RESULT:
			LOG_RAW("Temperature is %0.1f\n", *((float*)(evt->content)));
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
		} else {
			serial_fifo[serial_wp++ & 0xF] = *buffer++;
		}
	}
}

extern void cdc_task(void);
int main() {
	CRITICAL_REGION_INIT();
	app_sched_init();
	user_event_init();
	user_event_handler_regist(main_handler);

	adc_init();
	adc_set_temp_sensor_enabled(true);
	adc_select_input(4);

	ws2812_setup();

	struct repeating_timer timer;
	add_repeating_timer_ms(250, timer_4hz_callback, NULL, &timer);
	tusb_init();
	while(true) {
		app_sched_execute();
		tud_task();
		cdc_task();
		__wfi();
	}
}
