#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"

#include "driver/uart_register.h"
#include "driver/uart.h"





void user_rf_pre_init(void)
{
}



//Init function 
void ICACHE_FLASH_ATTR
user_init() {
	uint8 data[] = "Hello World";
	uint8 hello[0x1000] = "1111111111";
	uint32_t i;
  extern void ets_wdt_disable(void);


  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  uart_tx_one_char(system_update_cpu_freq(SYS_CPU_160MHZ));
  os_install_putc1(uart_tx_one_char);
  spi_flash_erase_sector(0x12);
/*  if ( spi_flash_write( 0x12000, (uint32 *)data, 10 ) != 0 ){

	  ets_uart_printf("Error");

  } else {
	  ets_uart_printf("Write done");
	  if ( spi_flash_read( 0x12000, (uint32 *)hello, 10 ) != 0 ){

	 	  ets_uart_printf("Error");

	   } else {
		   ets_uart_printf("Read done");
	  uart_tx_one_char(hello[0]);
	  uart_tx_one_char(hello[1]);
	  uart_tx_one_char(hello[2]);
	  uart_tx_one_char(hello[3]);
	  uart_tx_one_char(hello[4]);
	  uart_tx_one_char(hello[5]);
	  uart_tx_one_char(hello[6]);
	  uart_tx_one_char(hello[7]);
	  uart_tx_one_char(hello[8]);
	  uart_tx_one_char(hello[9]);
	  ets_uart_printf("done");
	   }

  }

  */

  spi_flash_write( 0x12000 + 501, (uint32 *)data, 5 );
  spi_flash_read( 0x12000 , (uint32 *)hello, 0x1000 );
  for (  i = 0; i < 0x1000; i++ ){
	  uart_tx_one_char(hello[i]);
  }
/*  spi_flash_read( 0x13000, (uint32 *)hello, 0x1000 );
  for (  i = 0; i < 0x1000; i++ ){
	  uart_tx_one_char(hello[i]);
  }*/
}





