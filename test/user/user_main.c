#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>

// see eagle_soc.h for these definitions
#define OUT_1_GPIO 4
#define OUT_1_MUX PERIPHS_IO_MUX_GPIO4_U
#define OUT_1_FUNC FUNC_GPIO4

#define OUT_1_TIME 15
#define OUT_2_TIME 35

#define OUT_2_GPIO 5
#define OUT_2_MUX PERIPHS_IO_MUX_GPIO5_U
#define OUT_2_FUNC FUNC_GPIO5

#define DELAY 10 /* milliseconds */

LOCAL os_timer_t blink_timer;
LOCAL uint8_t led_state1=0;
LOCAL uint8_t led_state2=0;
uint8_t out1 = 0;
uint8_t out2 = 0;

LOCAL void ICACHE_FLASH_ATTR blink_cb(void *arg)
{
	if (OUT_1_TIME == out1++) {
		GPIO_OUTPUT_SET(OUT_1_GPIO, led_state1);
		led_state1 ^=1;
		out1 = 0;
	} else if (OUT_2_TIME == out2++) {
		GPIO_OUTPUT_SET(OUT_2_GPIO, led_state2);
		led_state2 ^=1;
		out2 = 0;
	}
}

void user_rf_pre_init(void)
{
}

void user_init(void)
{
	// Configure pin as a GPIO
	PIN_FUNC_SELECT(OUT_1_MUX, OUT_1_FUNC);
	PIN_FUNC_SELECT(OUT_2_MUX, OUT_2_FUNC);
	// Set up a timer to blink the LED
	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&blink_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&blink_timer, (os_timer_func_t *)blink_cb, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&blink_timer, DELAY, 1);
}
