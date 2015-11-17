/**
 **********************************************************************************************************************************
  * @file    user_main.c
  * @author  Denys
  * @version V0.0.1
  * @date    2-Sept-2015
  * @brief
 **********************************************************************************************************************************
 * @info:
 * 		esp8266 работает в режиме softAP и STA. На ней запущен TCP сервер
 * который может обрабатывать определенный список команд ( который будет
 * определлен и реализован в следующей версии ), в данной же версии,
 * при получении любой информации она просто валится в уарт. Также, если есп
 * настроен на определенный роутер, то он пытается подключиться к нему, в
 * случае успешного подключения есп udp бродкастом шлет через каждый
 * определенный промежуток времени необходимую информацию (свой ip, номер пората
 * , для того чтобы данную есп можна было найти в сети и подключиться с TCP
 * серверу с дальшей передачей необходимой информации. Обратная связь реализована
 * через широковещительные запроси, через те же запросы, через которые передается
 * информация которая необходима, для того чтобы можна было найти есп внутри
 * локальной сети.
 * 		Длинна запросов ограничена ~1400 байт из-за ограничения буффера( если
 * длинна запроса более указаной длинны, то оно передается вызовом receive callback
 * несколько раз, а в даном проэкте механизм приема таких сообщений не реализован ).
 **********************************************************************************************************************************
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "gpio.h"
#include "user_config.h"
#include "driver/uart.h"

LOCAL void ICACHE_FLASH_ATTR initPeriph();
//**********************************************************************************************************************************
extern void ets_wdt_enable(void);
extern void ets_wdt_disable(void);
extern void ets_wdt_init(void);
extern void uart_tx_one_char();
//**********************************************************************************************************************************

LOCAL void ICACHE_FLASH_ATTR
init_done(void) {

   	ets_wdt_init();
    ets_wdt_disable();
   	ets_wdt_enable();
   	system_soft_wdt_stop();
    system_soft_wdt_restart();

}


void ICACHE_FLASH_ATTR
user_init(void) {

	ets_wdt_disable();
   	system_soft_wdt_stop();

	initPeriph();

    system_init_done_cb(init_done);

}


LOCAL void ICACHE_FLASH_ATTR
initPeriph() {

	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	os_install_putc1( uart_tx_one_char );

	if ( SYS_CPU_160MHZ != system_get_cpu_freq() ) {

		system_update_cpu_freq( SYS_CPU_160MHZ );
	}

	PIN_FUNC_SELECT( OUT_1_MUX, OUT_1_FUNC );
	GPIO_OUTPUT_SET( OUT_1_GPIO, 0 );

	PIN_FUNC_SELECT( OUT_2_MUX, OUT_2_FUNC );
	GPIO_OUTPUT_SET( OUT_2_GPIO, 0 );

	PIN_FUNC_SELECT( LED_MUX, LED_FUNC );
	GPIO_OUTPUT_SET( LED_GPIO, 0 );

	PIN_FUNC_SELECT( INP_1_MUX, INP_1_FUNC );
	gpio_output_set( 0, 0, 0, INP_1 );

	PIN_FUNC_SELECT( INP_2_MUX, INP_2_FUNC );
	gpio_output_set( 0, 0, 0, INP_2 );

	PIN_FUNC_SELECT( INP_3_MUX, INP_3_FUNC );
	gpio_output_set( 0, 0, 0, INP_3 );

	PIN_FUNC_SELECT( PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2 );
	gpio_output_set( 0, 0, 0, BIT2 );

}


