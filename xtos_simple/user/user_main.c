#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"

#include "espconn.h"
#include "mem.h"
#include "gpio.h"
#include "user_config.h"
#include "driver/uart.h"


#define PRIORITY_TASK_2  1
#define PRIORITY_TASK_1  0
#define TEST_QUEUE_LEN  4

os_event_t *testQueue;
os_event_t *testQueue1;

void user_rf_pre_init(void)
{
}



void ICACHE_FLASH_ATTR
Task1(os_event_t *e) {

	os_delay_us(100000);
	ets_uart_printf("Hello World!\r\n");
	system_os_post(PRIORITY_TASK_2, 0, 0);

}

void ICACHE_FLASH_ATTR
Task2(os_event_t *e) {

	os_delay_us(100000);
	ets_uart_printf("BYE ALL!\r\n");
	system_os_post(PRIORITY_TASK_1, 0, 0);

}

void ICACHE_FLASH_ATTR
user_init(void)
{
	testQueue=(os_event_t *)os_malloc(sizeof(os_event_t)*TEST_QUEUE_LEN);
	testQueue1=(os_event_t *)os_malloc(sizeof(os_event_t)*TEST_QUEUE_LEN);


	uart_init(BIT_RATE_115200, BIT_RATE_115200);


	system_os_task(Task1, PRIORITY_TASK_1, testQueue, TEST_QUEUE_LEN );
	system_os_task(Task2, PRIORITY_TASK_2, testQueue1, TEST_QUEUE_LEN );
	system_os_post(PRIORITY_TASK_1, 0, 0);
	system_os_post(PRIORITY_TASK_2, 0, 0);
}




