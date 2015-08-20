#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"

#include "driver/uart_register.h"
#include "driver/uart.h"


uint8 data[] = "Hello World";

uint8 hello[6];
uint8 world[6];



void user_rf_pre_init(void)
{
}



//Init function 
void ICACHE_FLASH_ATTR
user_init() {
  extern void ets_wdt_disable(void);


  uart_init(BIT_RATE_115200, BIT_RATE_115200);

  world[5] = '/0';
  hello[5] = '\0';

  if ( spi_flash_write( 70000, (uint32 *)data, strlen(data) ) != 0 ){

	  ets_uart_printf("Error");

  } else {
	  ets_uart_printf("Write done");
	  spi_flash_read( 70000, (uint32 *)hello, 5 );
	  spi_flash_read( 70005, (uint32 *)world, 5 );
//	  ets_uart_printf(hello);
	  uart_tx_one_char(hello[0]);
	  uart_tx_one_char(hello[1]);
	  uart_tx_one_char(hello[2]);
	  uart_tx_one_char(hello[3]);
	  uart_tx_one_char(hello[4]);
	  ets_uart_printf("done");
//	  ets_uart_printf(world);

  }

}
