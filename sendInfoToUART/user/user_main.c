/*
	The hello world demo
*/

#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include "driver/uart.h"
#include "user_interface.h"

#define DELAY 10000 /* milliseconds */

LOCAL os_timer_t hello_timer;
extern int ets_uart_printf(const char *fmt, ...);

LOCAL void ICACHE_FLASH_ATTR hello_cb(void *arg)
{
	ets_uart_printf("Hello World!\r\n");
}

void user_rf_pre_init(void)
{
}

void user_init(void)
{
	// Configure the UART
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	ets_wdt_enable();
	ets_wdt_disable();

	os_delay_us(100000);
	ets_uart_printf("Update system freq:");
	uart_tx_one_char(system_update_cpu_freq(SYS_CPU_160MHZ));
	os_delay_us(100000);

	ets_uart_printf("System freq:");
	uart_tx_one_char(system_get_cpu_freq());
	os_delay_us(100000);

	ets_uart_printf("Flash size map:");
	uart_tx_one_char(system_get_flash_size_map());
	os_delay_us(100000);

	os_install_putc1(uart_tx_one_char);
	os_printf( "SDK version: %s", system_get_sdk_version() );
	os_delay_us(100000);

	ets_uart_printf("free heap size:");
	uart_tx_one_char(system_get_free_heap_size());
	uart_tx_one_char(system_get_free_heap_size() >> 8);
	uart_tx_one_char(system_get_free_heap_size() >> 16);
	uart_tx_one_char(system_get_free_heap_size() >> 24);
	os_delay_us(100000);



//	ets_uart_printf(system_get_sdk_version());

	// Set up a timer to send the message
	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&hello_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&hello_timer, (os_timer_func_t *)hello_cb, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&hello_timer, DELAY, 1);
}
