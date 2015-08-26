#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include "driver/uart.h"
#include "user_interface.h"


#define OUT_1_GPIO 4
#define OUT_1_MUX PERIPHS_IO_MUX_GPIO4_U
#define OUT_1_FUNC FUNC_GPIO4

#define DELAY 10 /* milliseconds */

void callbackFunction(uint32 interruptMask, void *arg);
void callbackFunction1(uint32 interruptMask, void *arg);

LOCAL os_timer_t hello_timer;
extern int ets_uart_printf(const char *fmt, ...);

LOCAL void ICACHE_FLASH_ATTR hello_cb(void *arg)
{
/*	if( !GPIO_INPUT_GET(2) || !GPIO_INPUT_GET(12) || !GPIO_INPUT_GET(13) || !GPIO_INPUT_GET(14) ){
		GPIO_OUTPUT_SET(OUT_1_GPIO, 1);
	} else {
		GPIO_OUTPUT_SET(OUT_1_GPIO, 0);
	}*/
}

void user_rf_pre_init(void)
{
}

void user_init(void)
{
//	gpio_init();
	// Configure the UART
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	ets_wdt_enable();
	ets_wdt_disable();

	PIN_FUNC_SELECT(OUT_1_MUX, OUT_1_FUNC);


	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	gpio_output_set(0, 0, 0, BIT14);

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	gpio_output_set(0, 0, 0, BIT2);

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	gpio_output_set(0, 0, 0, BIT13);

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	gpio_output_set(0, 0, 0, BIT12);

//	GPIO_DIS_OUTPUT(2);
//	PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);


//	ETS_GPIO_INTR_ATTACH(callbackFunction, 12);



	   ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
	   gpio_intr_handler_register(callbackFunction, (void* )12); // GPIO12 interrupt handler
//	   ETS_GPIO_INTR_ATTACH(callbackFunction, 12);
//	   GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(12)); // Clear GPIO12 status
	   gpio_pin_intr_state_set(GPIO_ID_PIN(12), 1); // Interrupt on any GPIO12 edge

//	   ETS_GPIO_INTR_ATTACH(callbackFunction, 13); // GPIO12 interrupt handler
//	   	   GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(13)); // Clear GPIO12 status
//	   	   gpio_pin_intr_state_set(GPIO_ID_PIN(13), 1); // Interrupt on any GPIO12 edge


	  ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts






	// Set up a timer to send the message
	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&hello_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&hello_timer, (os_timer_func_t *)hello_cb, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&hello_timer, DELAY, 1);
}

void callbackFunction(uint32 interruptMask, void *arg){
//	if ( (interruptMask & 1<<12) ){
		GPIO_OUTPUT_SET(OUT_1_GPIO, 1);
//	} else {
//		GPIO_OUTPUT_SET(OUT_1_GPIO, 0);
//	}
}

