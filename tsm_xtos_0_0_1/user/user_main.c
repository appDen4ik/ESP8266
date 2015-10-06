/**
 ************************************************************************************
  * @file    user_main.c
  * @author  Denys
  * @version V0.0.1
  * @date    2-Sept-2015
  * @brief
 ************************************************************************************
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
 ************************************************************************************
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

#include "driver/myDB.h"

//**********************************************************************************************************************************
LOCAL inline void ICACHE_FLASH_ATTR comandParser( void )  __attribute__((always_inline));

LOCAL inline void ICACHE_FLASH_ATTR initPeriph( void )  __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR initWIFI( void )  __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR checkFlash( void ) __attribute__((always_inline));

LOCAL inline void ICACHE_FLASH_ATTR broadcastBuilder( void ) __attribute__((always_inline));

LOCAL void ICACHE_FLASH_ATTR buildQueryResponse( uint8_t *responseStatus );

LOCAL void ICACHE_FLASH_ATTR tcpRespounseBuilder( uint8_t *responseStatus );

LOCAL compStr ICACHE_FLASH_ATTR compareLenght( uint8_t *string, uint16_t maxLenght );

LOCAL uint32_t ICACHE_FLASH_ATTR StringToInt(uint8_t *data);
LOCAL uint8_t* ICACHE_FLASH_ATTR  ShortIntToString(uint16_t data, uint8_t *adressDestenation);

LOCAL uint8_t * ICACHE_FLASH_ATTR intToStringHEX(uint8_t data, uint8_t *adressDestenation);

LOCAL res ICACHE_FLASH_ATTR writeFlash( uint16_t where, uint8_t *what );
//**********************************************************************************************************************************

extern void ets_wdt_enable (void);
extern void ets_wdt_disable (void);

//**********************************************************************************************************************************

LOCAL uint8_t tmp[TMP_SIZE];

//**********************************************************************************************************************************

LOCAL uint8_t tmpFLASH[SPI_FLASH_SEC_SIZE];

//**********************************************************************************************************************************

LOCAL uint32_t gpioOutDeley1;
LOCAL uint32_t gpioOutDeley2;

LOCAL uint32_t gpioOutDeley1Counter;
LOCAL uint32_t gpioOutDeley2Counter;

LOCAL uint8_t gpioModeOut1[ sizeof(DEF_GPIO_OUT_MODE) ];
LOCAL uint8_t gpioModeOut2[ sizeof(DEF_GPIO_OUT_MODE) ];

LOCAL stat gpioStatusOut1 = DISABLE;
LOCAL stat gpioStatusOut2 = DISABLE;

//**********************************************************************************************************************************

LOCAL os_timer_t task_timer;

//**********************************************************************************************************************************

LOCAL struct espconn broadcast;

//**********************************************************************************************************************************

LOCAL uint8_t ledState = 0;
LOCAL uint32_t broadcastTmr;
LOCAL uint8_t *broadcastShift;
LOCAL uint8_t brodcastMessage[250] = { 0 };

//**********************************************************************************************************************************

LOCAL uint8_t rssiStr[5];
LOCAL sint8_t rssi;

//**********************************************************************************************************************************

LOCAL uint8_t *count;

//**********************************************************************************************************************************

LOCAL struct ip_info inf;

//**********************************************************************************************************************************

LOCAL tcp_stat tcpSt = TCP_FREE;
uint8_t storage[1000];
LOCAL uint16_t counterForTmp = 0;

//**********************************************************************************************************************************

struct espconn *pespconn;

//**********************************************************************************************************************************

uint32_t addressQuery;
uint16_t lenghtQuery;



uint8_t alignString[ ALIGN_STRING_SIZE ];

void user_rf_pre_init(void)
{

}

//**********************************************************Callbacks***************************************************************
LOCAL void ICACHE_FLASH_ATTR
tcp_recvcb( void *arg, char *pdata, unsigned short len ) { // data received

    if ( '\r' == pdata[ len - 2 ] && TCP_FREE == tcpSt ) {

    	pespconn = (struct espconn *) arg;

    	if ( TMP_SIZE < counterForTmp + len ) {
    		memcpy( &tmp[counterForTmp], pdata, len );
    		counterForTmp = 0;

    		comandParser();

    		pespconn = NULL;
    	} else {

    		memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
    		tmp[ sizeof(TCP_ERROR) ] = '\r';
    		tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';
    		espconn_sent( pespconn, tmp, ( sizeof( TCP_ERROR ) + 2 ) );
    	}

    } else if ( TCP_FREE == tcpSt ) {

    	if ( TMP_SIZE < counterForTmp + len ) {

    		pespconn = (struct espconn *) arg;

    		memcpy( &tmp[counterForTmp], pdata, len );
    		counterForTmp += len;
    	} else {

    		memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
    		tmp[ sizeof(TCP_ERROR) ] = '\r';
    		tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';
    		espconn_sent( pespconn, tmp, ( sizeof( TCP_ERROR ) + 2 ) );
    	}
    }
}

// espconn_sent(arg, tmp, strlen(tmp));

LOCAL void ICACHE_FLASH_ATTR
tcp_connectcb( void* arg ) { // TCP connected successfully

	struct espconn *pespconn = (struct espconn *) arg;

//	sint8 espconn_get_connection_info( pespconn, remot_info **pcon_info, uint8 typeflags );
// os_printf( "bssid : %x:%x:%x:%x:%x:%x ip : %d.%d.%d.%d Connected\r\n", MAC2STR(pespconn->bssid), IP2STR(&station->ip) );
}

LOCAL void ICACHE_FLASH_ATTR
tcp_disnconcb( void* arg ) { // TCP disconnected successfully
	ets_uart_printf("Disconnect");
//	os_printf( "disconnect result = %d", espconn_delete((struct espconn *) arg));
}

LOCAL void ICACHE_FLASH_ATTR
tcp_reconcb( void* arg, sint8 err ) { // error, or TCP disconnected

}

LOCAL void ICACHE_FLASH_ATTR
tcp_sentcb( void* arg ) { // data sent

}

//**********************************************************************************************************************************



res ICACHE_FLASH_ATTR
writeFlash( uint16_t where, uint8_t *what ) {

	uint16_t i;

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
				                                     (uint32 *)tmpFLASH, ALIGN_FLASH_READY_SIZE ) ) {

		return ERROR;
	}

	for ( i = where ; '\n' != tmpFLASH[ i ]; i++ ) {

		tmpFLASH[ i ] = 0xff;
	}

	tmpFLASH[ i ] = 0xff;

	for (  i = 0; '\0' != what[ i ]; i++, where++ ) {

		tmpFLASH[ where ] = what[ i ];
	}

	tmpFLASH[ where++ ] = '\0';
	tmpFLASH[ where ] = '\n';


	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM ) ) {

		return ERROR;
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_write( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                                                                            (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {

		return ERROR;
	}

	return DONE;
}



void ICACHE_FLASH_ATTR
mScheduler(char *datagram, uint16 size) {


//************************************************************************************
	 if ( 0 == GPIO_INPUT_GET( INP_3_PIN ) ) {

		 spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM );

			{

			uint16_t c, currentSector;


			for ( currentSector = USER_SECTOR_IN_FLASH_MEM; currentSector <= USER_SECTOR_IN_FLASH_MEM; currentSector++ ) {
					os_printf( " currentSector   %d", currentSector);
					spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE );
					for ( c = 0; SPI_FLASH_SEC_SIZE > c; c++ ) {
						uart_tx_one_char(tmpFLASH[c]);
					}
					system_soft_wdt_stop();
				}

			}
		 gpioStatusOut1 = ENABLE;
	 }

	 if ( 0 == GPIO_INPUT_GET( INP_1_PIN ) ) {

		    uint16_t a, i;
		    uint8_t *p, *str;
			uint8_t ascii[10];
			static uint8_t string[] = "HELLO";
			static uint8_t string1[] = "Тестирование";

			str = tmp;

			memcpy( str, TCP_REQUEST, sizeof(TCP_REQUEST) );

			str += sizeof(TCP_REQUEST);
			*str++ = ' ';

			for ( a = 0; a < STRING_SIZE - 1; a++ ) {
				str[a] = '0';
			}

			p = ShortIntToString( 10, ascii );

			memcpy( &str[ STRING_SIZE - 39 ], string, strlen(string) );

			str[ STRING_SIZE - 40 ] = START_OF_FIELD;
			str[ STRING_SIZE - 20 ]  = END_OF_FIELD;


			memcpy( &str[ STRING_SIZE - 1 - (p - ascii) - 1 - 40], ascii, (p - ascii) );

			str[ STRING_SIZE - 15 - 40 ] = START_OF_FIELD;
			str[ STRING_SIZE - 2 - 40 ]  = END_OF_FIELD;
			str[ STRING_SIZE - 1 ]  = END_OF_STRING;


			memcpy( &str[ STRING_SIZE - 40 - 15 - 20 ], string1, strlen( string1 ) );

			str[ STRING_SIZE - 40 - 20 - 15 - 1 ] = START_OF_FIELD;
			str[ STRING_SIZE - 41 - 15 ]  = END_OF_FIELD;

			str += ALIGN_STRING_SIZE;
			*str++ = '\r';
			*str++ = '\n';


			   for (i = 0; i < str - tmp; i++) {
			        uart_tx_one_char(tmp[i]);
			    }

			   comandParser();
			   os_printf( "tst");
			   for (i = 0; '\r' < tmp[ i ]; i++) {
			   		uart_tx_one_char(tmp[i]);
			   	}
			   uart_tx_one_char(tmp[i++]);
			   uart_tx_one_char(tmp[i]);
			while ( 0 == GPIO_INPUT_GET( INP_1_PIN ) );

	 }
//************************************************************************************
	if ( ENABLE == gpioStatusOut1 ) {
		if (  0 == strcmp( gpioModeOut1, GPIO_OUT_TRIGGER_MODE ) || 0 == strcmp( gpioModeOut1, DEF_GPIO_OUT_MODE ) ) {
			if ( gpioOutDeley1Counter < gpioOutDeley1 && GPIO_INPUT_GET(INP_2_PIN) ) {
				gpioOutDeley1Counter += 15;
				GPIO_OUTPUT_SET(OUT_1_GPIO, 1);
			} else {
				GPIO_OUTPUT_SET(OUT_1_GPIO, 0);
				gpioOutDeley1Counter = 0;
				gpioStatusOut1 = DISABLE;
			}
		}
	}

	if ( ENABLE == gpioStatusOut2 ) {
		if (  0 == strcmp( gpioModeOut2, GPIO_OUT_TRIGGER_MODE ) || 0 == strcmp( gpioModeOut2, DEF_GPIO_OUT_MODE ) ) {
			if ( gpioOutDeley2Counter < gpioOutDeley2 && GPIO_INPUT_GET(INP_4_PIN) ) {
				gpioOutDeley2Counter += 15;
				GPIO_OUTPUT_SET(OUT_2_GPIO, 1);
			} else {
				GPIO_OUTPUT_SET(OUT_2_GPIO, 0);
				gpioOutDeley2Counter = 0;
				gpioStatusOut2 = DISABLE;
			}
		}
	}

	if ( broadcastTmr >= BROADCAST_TIMER ) {

		struct station_info * station = wifi_softap_get_station_info();

		broadcastTmr = 0;

		broadcastBuilder();

		os_timer_disarm(&task_timer);

		// Внутрення сеть
		while ( station ) {

			IP4_ADDR( (ip_addr_t *)broadcast.proto.udp->remote_ip, (uint8_t)(station->ip.addr), (uint8_t)(station->ip.addr >> 8),\
															(uint8_t)(station->ip.addr >> 16), (uint8_t)(station->ip.addr >> 24) );
#ifdef DEBUG
		os_printf( "bssid : %x:%x:%x:%x:%x:%x ip : %d.%d.%d.%d ", MAC2STR(station->bssid), IP2STR(&station->ip) );
#endif

			espconn_create(&broadcast);
			espconn_sent(&broadcast, brodcastMessage, strlen(brodcastMessage));
			espconn_delete(&broadcast);

			station = STAILQ_NEXT(station, next);
		}

		wifi_softap_free_station_info();


		// Внешняя сеть
/*		switch( wifi_station_get_connect_status() ) {
			case STATION_GOT_IP:
				if ( ( rssi = wifi_station_get_rssi() ) < -90 ){
					count = rssiStr;
					*count++ = '-';
					count = ShortIntToString( ~rssi, count );
					*count = '\0';
					os_printf( "Bad signal, rssi = %s", rssiStr);
					os_delay_us(1000);
					os_printf( "%s", broadcastShift );
					GPIO_OUTPUT_SET(LED_GPIO, ledState);
					ledState ^=1;
				} else {

					GPIO_OUTPUT_SET(LED_GPIO, 1);

					wifi_get_ip_info(STATION_IF, &inf);

					if ( 0 != inf.ip.addr ) {

						ets_uart_printf("WiFi connected\r\n");
						os_delay_us(1000);
						os_printf( "%s", brodcastMessage );

						espconn_create(&broadcast);
						espconn_sent(&broadcast, brodcastMessage, strlen(brodcastMessage));
						espconn_delete(&broadcast);
					}
				}
				break;
			case STATION_WRONG_PASSWORD:
				ets_uart_printf("WiFi connecting error, wrong password\r\n");
				os_delay_us(1000);
				os_printf( "%s", broadcastShift );
				GPIO_OUTPUT_SET(LED_GPIO, ledState);
				ledState ^=1;
				break;
			case STATION_NO_AP_FOUND:
				ets_uart_printf("WiFi connecting error, ap not found\r\n");
				os_delay_us(1000);
				os_printf( "%s", broadcastShift );
				GPIO_OUTPUT_SET(LED_GPIO, ledState);
				ledState ^=1;
				break;
			case STATION_CONNECT_FAIL:
				ets_uart_printf("WiFi connecting fail\r\n");
				os_delay_us(1000);
				os_printf( "%s", broadcastShift );
				GPIO_OUTPUT_SET(LED_GPIO, ledState);
				ledState ^=1;
				break;
			default:
				ets_uart_printf("WiFi connecting...\r\n");
				os_delay_us(1000);
				os_printf( "%s", broadcastShift );
				GPIO_OUTPUT_SET(LED_GPIO, ledState);
				ledState ^=1;
				break;
		}*/
	}

	broadcastTmr += 10;

	os_timer_setfn(&task_timer, (os_timer_func_t *)mScheduler, (void *)0);
	os_timer_arm(&task_timer, DELAY, 0);
}


void ICACHE_FLASH_ATTR
user_init(void) {

	LOCAL struct espconn espconnServer;
	LOCAL esp_tcp tcpServer;
	LOCAL esp_udp udpClient;

	initPeriph();

	checkFlash();

	initWIFI();

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------

	{


		uint8_t ascii[10];
		static uint8_t string[] = "HELLO";
		static uint8_t string1[] = "Тестирование";


		uint16_t lenght;
		uint32_t adress;


			uint8_t alignStr[ALIGN_STRING_SIZE];

			uint8_t *p;

			uint16_t c, currentSector;
			uint32_t a, i;

			result res;

			clearSectorsDB();
			for ( i = STRING_SIZE; i < ALIGN_STRING_SIZE; i++ ) {
				alignString[i] = 0xff;
			}

			clearSectorsDB();

			os_delay_us(500000);
			ets_uart_printf(" Проверка query. Тест 1 ");
			os_delay_us(500000);


			ets_uart_printf(" П.1 Заполняем базу значениями (ASCII) выровняными по ALIGN_STRING_SIZE символов  ");

			for ( i = 1; i <= ( SPI_FLASH_SEC_SIZE / ALIGN_STRING_SIZE ); i++ ) {

				for ( a = 0; a < STRING_SIZE - 1; a++ ) {
						alignString[a] = '0';
				}

				p = ShortIntToString( i, ascii );

				memcpy( &alignString[ STRING_SIZE - 39 ], string, strlen(string) );

				alignString[ STRING_SIZE - 40 ] = START_OF_FIELD;
				alignString[ STRING_SIZE - 20 ]  = END_OF_FIELD;


				memcpy( &alignString[ STRING_SIZE - 1 - (p - ascii) - 1 - 40], ascii, (p - ascii) );

				alignString[ STRING_SIZE - 15 - 40 ] = START_OF_FIELD;
				alignString[ STRING_SIZE - 2 - 40 ]  = END_OF_FIELD;
				alignString[ STRING_SIZE - 1 ]  = END_OF_STRING;


				memcpy( &alignString[ STRING_SIZE - 40 - 15 - 20 ], string1, strlen( string1 ) );

				alignString[ STRING_SIZE - 40 - 20 - 15 - 1 ] = START_OF_FIELD;
				alignString[ STRING_SIZE - 41 - 15 ]  = END_OF_FIELD;

				os_printf( " \n %s \n String lenght %d", alignString, ( strlen( alignString ) + 1 ) );

				switch ( res = insert( alignString ) ) {
					case OPERATION_OK:
						ets_uart_printf("OPERATION_OK");
						system_soft_wdt_stop();
						break;
					case WRONG_LENGHT:
						ets_uart_printf("WRONG_LENGHT");
						break;
					case OPERATION_FAIL:
						ets_uart_printf("OPERATION_FAIL");
						break;
					case LINE_ALREADY_EXIST:
						ets_uart_printf("LINE_ALREADY_EXIST");
						break;
					case NOT_ENOUGH_MEMORY:
						ets_uart_printf("NOT_ENOUGH_MEMORY");
						break;

				}

			}

			spi_flash_read( SPI_FLASH_SEC_SIZE * START_SECTOR, (uint32 *)tmp, SPI_FLASH_SEC_SIZE );
			for ( i = 0; SPI_FLASH_SEC_SIZE > i; i++ ) {
				uart_tx_one_char(tmp[ i ]);
			}

	}




//-----------------------------------------------------------------------------------------------------------------------------------------------------------------


	if ( SPI_FLASH_RESULT_OK == spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
				                            (uint32 *)tmp, ALIGN_FLASH_READY_SIZE ) ) {

		if ( 0 == strcmp( &tmp[GPIO_OUT_1_MODE_OFSET], DEF_GPIO_OUT_MODE ) || \
				                                             0 == strcmp( &tmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) ) {


			gpioOutDeley1 = StringToInt( &tmp[GPIO_OUT_1_DELEY_OFSET] );

			os_sprintf( gpioModeOut1, "%s", &tmp[ GPIO_OUT_1_MODE_OFSET ] );
		}
#ifdef DEBUG
	  os_printf( " gpioModeOut1  %s,  gpioOutDeley1  %d", gpioModeOut1, gpioOutDeley1 );
	  os_delay_us(500000);
#endif

		if ( 0 == strcmp( &tmp[GPIO_OUT_2_MODE_OFSET], DEF_GPIO_OUT_MODE ) || \
				                                             0 == strcmp( &tmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) ) {

			gpioOutDeley2 = StringToInt( &tmp[GPIO_OUT_2_DELEY_OFSET] );

			os_sprintf( gpioModeOut2, "%s", &tmp[ GPIO_OUT_2_MODE_OFSET ] );
		}
#ifdef DEBUG
	  os_printf( " gpioModeOut2  %s,  gpioOutDeley2  %d", gpioModeOut2, gpioOutDeley2 );
	  os_delay_us(500000);
#endif

	}

#ifdef DEBUG
	{

	uint16_t c, currentSector;


	for ( currentSector = USER_SECTOR_IN_FLASH_MEM; currentSector <= USER_SECTOR_IN_FLASH_MEM; currentSector++ ) {
			os_printf( " currentSector   %d", currentSector);
			spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)tmp, SPI_FLASH_SEC_SIZE );
			for ( c = 0; SPI_FLASH_SEC_SIZE > c; c++ ) {
				uart_tx_one_char(tmp[c]);
			}
			system_soft_wdt_stop();
		}

	}
#endif
	{ //tcp сервер

		espconnServer.type = ESPCONN_TCP;
		espconnServer.state = ESPCONN_NONE;
		espconnServer.proto.tcp = &tcpServer;
		espconnServer.proto.tcp->local_port = TCP_PORT;

		espconn_accept(&espconnServer);
		espconn_regist_time(&espconnServer, TCP_SERVER_TIMEOUT, 0);
		espconn_regist_recvcb(&espconnServer, tcp_recvcb);          // data received
		espconn_regist_connectcb(&espconnServer, tcp_connectcb);    // TCP connected successfully
		espconn_regist_disconcb(&espconnServer, tcp_disnconcb);     // TCP disconnected successfully
		espconn_regist_sentcb(&espconnServer, tcp_sentcb);          // data sent
		espconn_regist_reconcb(&espconnServer, tcp_reconcb);        // error, or TCP disconnected

	}

    { //udp клиент

    	broadcast.type = ESPCONN_UDP;
    	broadcast.state = ESPCONN_NONE;
    	broadcast.proto.udp = &udpClient;
    	broadcast.proto.udp->remote_port = UDP_REMOTE_PORT;
    }

	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&task_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&task_timer, (os_timer_func_t *)mScheduler, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&task_timer, DELAY, 0);

}


void ICACHE_FLASH_ATTR
initPeriph(  ) {

	ets_wdt_enable();
	ets_wdt_disable();

	system_soft_wdt_stop();
//	system_soft_wdt_restart();

	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	uart_tx_one_char('\0');

	os_install_putc1(uart_tx_one_char);

	if ( SYS_CPU_160MHZ == system_get_cpu_freq() ) {
		system_update_cpu_freq(SYS_CPU_160MHZ);
	}

	PIN_FUNC_SELECT(OUT_1_MUX, OUT_1_FUNC);
	GPIO_OUTPUT_SET(OUT_1_GPIO, 0);

	PIN_FUNC_SELECT(OUT_2_MUX, OUT_2_FUNC);
	GPIO_OUTPUT_SET(OUT_2_GPIO, 0);

	PIN_FUNC_SELECT(LED_MUX, LED_FUNC);

	PIN_FUNC_SELECT(INP_1_MUX, INP_1_FUNC);
	gpio_output_set(0, 0, 0, INP_1);

	PIN_FUNC_SELECT(INP_2_MUX, INP_2_FUNC);
	gpio_output_set(0, 0, 0, INP_2);

	PIN_FUNC_SELECT(INP_3_MUX, INP_3_FUNC);
	gpio_output_set(0, 0, 0, INP_3);

	PIN_FUNC_SELECT(INP_4_MUX, INP_4_FUNC);
	gpio_output_set(0, 0, 0, INP_4);

}


void ICACHE_FLASH_ATTR
initWIFI( ) {

	struct station_config stationConf;
	struct softap_config softapConf;
	struct ip_info ipinfo;

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {

		ets_uart_printf("initWIFI Read fail");
		while(1);
	}

	if ( STATIONAP_MODE != wifi_get_opmode() ) {

		wifi_set_opmode( STATIONAP_MODE );
	}

	wifi_station_disconnect();
	wifi_station_dhcpc_stop();

	if( wifi_station_get_config( &stationConf ) ) {

	  if ( 0 != strcmp( stationConf.ssid, &tmp[ SSID_STA_OFSET ] ) ) {

		  os_memset( stationConf.ssid, 0, sizeof(&stationConf.ssid) );

		  os_sprintf( stationConf.ssid, "%s", &tmp[ SSID_STA_OFSET ] );
	  }
#ifdef DEBUG
	  os_printf( " stationConf.ssid  %s,  &tmp[ SSID_STA_OFSET ]  %s", stationConf.ssid, &tmp[ SSID_STA_OFSET ] );
#endif
	  if ( 0 != strcmp( stationConf.password, &tmp[ PWD_STA_OFSET ] ) ) {

		  os_memset( stationConf.password, 0, sizeof(&stationConf.password) );
		  os_sprintf( stationConf.password, "%s", &tmp[ PWD_STA_OFSET ] );
	  }
#ifdef DEBUG
	  os_printf( " stationConf.password  %s,  &tmp[ PWD_STA_OFSET ]  %s", stationConf.password, &tmp[ PWD_STA_OFSET ] );
#endif
	  if ( 0 !=  stationConf.bssid_set ) {

		  stationConf.bssid_set = 0;
	  }

	 if ( !wifi_station_set_config(&stationConf) ) {

		 ets_uart_printf("Module not set station config!\r\n");
	 }

	} else {

		ets_uart_printf("read station config fail\r\n");
	}

	wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);


	if ( wifi_softap_get_config( &softapConf ) ) {

		wifi_station_dhcpc_stop();

		if ( 0 != strcmp( softapConf.ssid, &tmp[ SSID_AP_OFSET ] ) ) {

			os_memset( softapConf.ssid, 0, sizeof(&softapConf.ssid) );
			softapConf.ssid_len = os_sprintf( softapConf.ssid, "%s", &tmp[ SSID_AP_OFSET ] );
		}
#ifdef DEBUG
		os_printf( " softapConf.ssid  %s,  &tmp[ SSID_AP_OFSET ]  %s, softapConf.ssid_len  %d", softapConf.ssid, &tmp[ SSID_AP_OFSET ], softapConf.ssid_len );
#endif

		if ( 0 != strcmp( softapConf.password, &tmp[ PWD_AP_OFSET ] ) ) {

			os_memset( softapConf.password, 0, sizeof(&softapConf.password) );
			os_sprintf( softapConf.password, "%s", &tmp[ PWD_AP_OFSET ] );
		}
#ifdef DEBUG
		os_printf( " softapConf.password  %s,  &tmp[ PWD_AP_OFSET ]  %s", softapConf.password, &tmp[ PWD_AP_OFSET ] );
#endif

		if ( softapConf.channel != DEF_CHANEL ) {

			softapConf.channel = DEF_CHANEL;
		}
#ifdef DEBUG
		os_printf( " softapConf.channel  %d", softapConf.channel );
#endif

		if ( softapConf.authmode != DEF_AUTH ) {

			softapConf.authmode = DEF_AUTH;
		}
#ifdef DEBUG
		os_printf( " softapConf.authmode  %d", softapConf.authmode );
#endif

		if ( softapConf.max_connection != MAX_CON ) {

		    softapConf.max_connection = MAX_CON;
		}
#ifdef DEBUG
		os_printf( " softapConf.max_connection  %d", softapConf.max_connection );
#endif

		if ( softapConf.ssid_hidden != NO_HIDDEN ) {

			softapConf.ssid_hidden = NO_HIDDEN;
		}
#ifdef DEBUG
		os_printf( " softapConf.ssid_hidden  %d", softapConf.ssid_hidden );
#endif

		if ( softapConf.beacon_interval != BEACON_INT ) {

			softapConf.beacon_interval = BEACON_INT;
		}
#ifdef DEBUG
		os_printf( " softapConf.beacon_interval  %d", softapConf.beacon_interval );
#endif

		if( !wifi_softap_set_config( &softapConf ) ) {
			#ifdef DEBUG
					ets_uart_printf("Module not set AP config!\r\n");
			#endif
		}

	} else {

		ets_uart_printf("read soft ap config fail\r\n");
	}


	if ( wifi_get_ip_info(SOFTAP_IF, &ipinfo ) ) {

		IP4_ADDR(&ipinfo.ip, 172, 16, 1, 1);
		IP4_ADDR(&ipinfo.gw, 172, 16, 1, 1);
		IP4_ADDR(&ipinfo.netmask, 255, 255, 255, 0);

		wifi_set_ip_info(SOFTAP_IF, &ipinfo);

	} else {

		ets_uart_printf("read ip ap fail\r\n");
	}
#ifdef DEBUG
		os_printf( " ipinfo.ip.addr  %d", ipinfo.ip.addr );
#endif

	wifi_station_dhcpc_start();

}


void ICACHE_FLASH_ATTR
checkFlash( void ) {

	uint16_t i;

	for ( i = 0; i < SPI_FLASH_SEC_SIZE; i++ ) {
		tmp[i] = 0xff;
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                            (uint32 *)tmp, ALIGN_FLASH_READY_SIZE ) ) {

		ets_uart_printf("Read fail");
		while(1);
	}

	if ( 0 != strcmp( tmp, FLASH_READY ) ) {

		if ( OPERATION_OK != clearSectorsDB() ) {

			ets_uart_printf("clearSectorsDB() fail");
			while(1);
		}

		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM )  ) {

			ets_uart_printf("Erase USER_SECTOR_IN_FLASH_MEM fail");
			while(1);
		}

		memcpy( &tmp[FLASH_READY_OFSET], FLASH_READY, sizeof(FLASH_READY) );
		tmp[ sizeof(FLASH_READY) ] = '\n';


		memcpy( &tmp[HEADER_STA_OFSET], HEADER_STA, sizeof(HEADER_STA) );
		tmp[ sizeof(HEADER_STA) + HEADER_STA_OFSET ] = '\n';

		memcpy( &tmp[SSID_STA_OFSET], DEF_SSID_STA, sizeof(DEF_SSID_STA) );
		tmp[ sizeof(DEF_SSID_STA) + SSID_STA_OFSET ] = '\n';

		memcpy( &tmp[PWD_STA_OFSET], DEF_PWD_STA, sizeof(DEF_PWD_STA) );
		tmp[ sizeof(DEF_PWD_STA) + PWD_STA_OFSET ] = '\n';


		memcpy( &tmp[SSID_AP_OFSET], DEF_SSID_AP, sizeof(DEF_SSID_AP) );
		tmp[ sizeof(DEF_SSID_AP) + SSID_AP_OFSET ] = '\n';

		memcpy( &tmp[PWD_AP_OFSET], DEF_PWD_AP, sizeof(DEF_PWD_AP) );
		tmp[ sizeof(DEF_PWD_AP) + PWD_AP_OFSET ] = '\n';

		memcpy( &tmp[HEADER_AP_OFSET], HEADER_AP, sizeof(HEADER_AP) );
		tmp[ sizeof(HEADER_AP) + HEADER_AP_OFSET ] = '\n';


		memcpy( &tmp[GPIO_OUT_1_HEADER_OFSET], GPIO_OUT_1_HEADER, sizeof(GPIO_OUT_1_HEADER) );
		tmp[ sizeof(GPIO_OUT_1_HEADER) + GPIO_OUT_1_HEADER_OFSET ] = '\n';

		memcpy( &tmp[GPIO_OUT_1_MODE_OFSET], DEF_GPIO_OUT_MODE, sizeof(DEF_GPIO_OUT_MODE) );
		tmp[ sizeof(DEF_GPIO_OUT_MODE) + GPIO_OUT_1_MODE_OFSET ] = '\n';

		memcpy( &tmp[GPIO_OUT_1_DELEY_OFSET], DEF_GPIO_OUT_DELEY, sizeof(DEF_GPIO_OUT_DELEY) );
		tmp[ sizeof(DEF_GPIO_OUT_DELEY) + GPIO_OUT_1_DELEY_OFSET ] = '\n';


		memcpy( &tmp[GPIO_OUT_2_HEADER_OFSET], GPIO_OUT_2_HEADER, sizeof(GPIO_OUT_2_HEADER) );
		tmp[ sizeof(GPIO_OUT_2_HEADER) + GPIO_OUT_2_HEADER_OFSET ] = '\n';

		memcpy( &tmp[GPIO_OUT_2_MODE_OFSET], DEF_GPIO_OUT_MODE, sizeof(DEF_GPIO_OUT_MODE) );
		tmp[ sizeof(DEF_GPIO_OUT_MODE) + GPIO_OUT_2_MODE_OFSET ] = '\n';

		memcpy( &tmp[GPIO_OUT_2_DELEY_OFSET], DEF_GPIO_OUT_DELEY, sizeof(DEF_GPIO_OUT_DELEY) );
		tmp[ sizeof(DEF_GPIO_OUT_DELEY) + GPIO_OUT_2_DELEY_OFSET ] = '\n';


		memcpy( &tmp[BROADCAST_NAME_HEADER_OFSET], BROADCAST_NAME_HEADER, sizeof(BROADCAST_NAME_HEADER) );
		tmp[ sizeof(BROADCAST_NAME_HEADER) + BROADCAST_NAME_HEADER_OFSET ] = '\n';

		memcpy( &tmp[BROADCAST_NAME_OFSET], BROADCAST_NAME, sizeof(BROADCAST_NAME) );
		tmp[ sizeof(BROADCAST_NAME) + BROADCAST_NAME_OFSET ] = '\n';



		if ( SPI_FLASH_RESULT_OK != spi_flash_write( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {

				ets_uart_printf("write default param fail");
				while(1);
		}

		system_restore();

	}

}


// Перевод числа в последовательность ASCII
uint8_t * ShortIntToString(uint16_t data, uint8_t *adressDestenation) {
	uint8_t *startAdressDestenation = adressDestenation;
	uint8_t *endAdressDestenation;
	uint8_t buff;

	do {// перевод входного значения в последовательность ASCII кодов
		// в обратном порядке
		*adressDestenation++ = data % 10 + '0';
	} while ((data /= 10) > 0);
	endAdressDestenation = adressDestenation ;

	// разворот последовательности
	for (adressDestenation--; startAdressDestenation < adressDestenation;startAdressDestenation++, adressDestenation--) {
		buff = *startAdressDestenation;
		*startAdressDestenation = *adressDestenation;
		*adressDestenation = buff;
	}
	return endAdressDestenation;
}

// Перевод последовательности ASCII в число
uint32_t StringToInt(uint8_t *data) {
	uint32_t returnedValue = 0;
	for (;*data >= '0' && *data <= '9'; data++) {

		returnedValue = 10 * returnedValue + (*data - '0');
	}
	return returnedValue;
}

// Перевод шестнадцетиричного числа в последовательность ASCII
uint8_t * intToStringHEX(uint8_t data, uint8_t *adressDestenation) {
	if ( ( data >> 4 ) < 10 ) {
		*adressDestenation++ = ( data >> 4 ) + '0';
	} else {
		*adressDestenation++ = ( data >> 4 ) % 10 + 'A';
	}
	if ( ( data & 0x0f ) < 10 ) {
		*adressDestenation++ = ( data & 0x0f ) + '0';
	} else {
		*adressDestenation++ = ( data & 0x0f ) % 10 + 'A';
	}
	return adressDestenation;
}


// broadcast message:
//          "name:" + " " + nameSTA +
//          + " " + mac: + " " + wifi_get_macaddr() +
//          + " " + ip: + " " + wifi_get_ip_info( ) +
//          + " " + server port: + " " servPort + " " +
//			+ " " + phy mode: + " " + wifi_get_phy_mode() +
//          + " " + rssi: + " " + wifi_station_get_rssi() +
//          + " " + outputs: +
//			+ " " + gpio_1: + " " + Trigger/Impulse + " " + delay+ms\n +
//		    + " " + gpio_2: + " " + Trigger/Impulse + " " + delay+ms\n +
//          + " " + inputs: +
//		    + " " + inp_1: + " " + high/low +
//          + " " + INP_2: + " " + high/low +
//          + " " + INP_3: + " " + high/low +
//			+ " " + INP_4: + " " + high/low + "\r\n" + "\0"
void ICACHE_FLASH_ATTR
broadcastBuilder( void ){

	uint8_t macadr[10];

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
				                            (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {

	}

	//выделяем бродкаст айпишку
	wifi_get_ip_info( STATION_IF, &inf );

	IP4_ADDR((ip_addr_t *)broadcast.proto.udp->remote_ip, (uint8_t)(inf.ip.addr), (uint8_t)(inf.ip.addr >> 8),\
															(uint8_t)(inf.ip.addr >> 16), 255);

	count = brodcastMessage;

	memcpy( count, NAME, ( sizeof( NAME ) - 1 ) );
	count += sizeof( NAME ) - 1;

	os_sprintf( count, "%s", &tmp[ BROADCAST_NAME_OFSET ] );
	count += strlen( &tmp[ BROADCAST_NAME_OFSET ] );

//================================================================
	memcpy( count, MAC, ( sizeof( MAC ) - 1 ) );
	count += sizeof( MAC ) - 1;

	wifi_get_macaddr( STATION_IF, macadr );

	count = intToStringHEX( macadr[0], count );
		*count++ = ':';
	count = intToStringHEX( macadr[1], count );
		*count++ = ':';
	count = intToStringHEX( macadr[2], count );
		*count++ = ':';
	count = intToStringHEX( macadr[3], count );
		*count++ = ':';
    count = intToStringHEX( macadr[4], count );
	   *count++ = ':';
    count = intToStringHEX( macadr[5], count );
//================================================================
    memcpy( count, IP, ( sizeof( IP ) - 1 ) );
    count += sizeof( IP ) - 1;

    count = ShortIntToString( (uint8_t)(inf.ip.addr), count );
    	*count++ = '.';
    count = ShortIntToString( (uint8_t)(inf.ip.addr >> 8), count );
    	*count++ = '.';
    count = ShortIntToString( (uint8_t)(inf.ip.addr >> 16) , count );
    	*count++ = '.';
    count = ShortIntToString( (uint8_t)(inf.ip.addr  >> 24), count);
//================================================================
    memcpy( count, SERVER_PORT, ( sizeof( SERVER_PORT ) - 1 ) );
    count += sizeof( SERVER_PORT ) - 1;

    count = ShortIntToString( TCP_PORT, count );
//================================================================
    memcpy( count, PHY_MODE, ( sizeof( PHY_MODE ) - 1 ) );
    count += sizeof( PHY_MODE ) - 1;
    {
    	uint8_t phyMode;
    	if ( PHY_MODE_11B == (phyMode = wifi_get_phy_mode() ) ) {
    		memcpy( count, PHY_MODE_B, ( sizeof( PHY_MODE_B ) - 1 ) );
    		count += sizeof( PHY_MODE_B ) - 1;
    	} else if ( PHY_MODE_11G == phyMode ) {
    		memcpy( count, PHY_MODE_G, ( sizeof( PHY_MODE_G ) - 1 ) );
    		count += sizeof( PHY_MODE_G ) - 1;
    	} else if ( PHY_MODE_11N == phyMode ) {
    		memcpy( count, PHY_MODE_N, ( sizeof( PHY_MODE_N ) - 1 ) );
    		count += sizeof( PHY_MODE_N ) - 1;
    	}
    }
//================================================================
    if ( ( rssi = wifi_station_get_rssi() ) < 0 ) {

    	memcpy( count, RSSI, ( sizeof( RSSI ) - 1 ) );
	    count += sizeof( RSSI ) - 1;

	    *count++ = '-';
	    count = ShortIntToString( (rssi * (-1)), count );
    }
//================================================================
	memcpy( count, OUTPUTS, ( sizeof( OUTPUTS ) - 1 ) );
	broadcastShift = count;
	count += sizeof( OUTPUTS ) - 1;
//================================================================
	memcpy( count, GPIO_1, ( sizeof( GPIO_1 ) - 1 ) );
	count += sizeof( GPIO_1 ) - 1;

	if ( 0 == strcmp( &tmp[GPIO_OUT_1_MODE_OFSET], DEF_GPIO_OUT_MODE ) ) {

		memcpy( count, DEF_GPIO_OUT_MODE, ( sizeof( DEF_GPIO_OUT_MODE ) - 1 ) );
		count += sizeof( DEF_GPIO_OUT_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmp[ GPIO_OUT_1_DELEY_OFSET ] );
		count += strlen( &tmp[ GPIO_OUT_1_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;

	} else if ( 0 == strcmp( &tmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) ) {

		memcpy( count, GPIO_OUT_TRIGGER_MODE, ( sizeof( GPIO_OUT_TRIGGER_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_TRIGGER_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmp[ GPIO_OUT_1_DELEY_OFSET ] );
		count += strlen( &tmp[ GPIO_OUT_1_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	}
//================================================================
	memcpy( count, GPIO_2, ( sizeof( GPIO_2 ) - 1 ) );
	count += sizeof( GPIO_2 ) - 1;

	if ( 0 == strcmp( &tmp[GPIO_OUT_2_MODE_OFSET], DEF_GPIO_OUT_MODE ) ) {

		memcpy( count, DEF_GPIO_OUT_MODE, ( sizeof( DEF_GPIO_OUT_MODE ) - 1 ) );
		count += sizeof( DEF_GPIO_OUT_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmp[ GPIO_OUT_2_DELEY_OFSET ] );
		count += strlen( &tmp[ GPIO_OUT_2_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	} else if ( 0 == strcmp( &tmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) ) {

		memcpy( count, GPIO_OUT_TRIGGER_MODE, ( sizeof( GPIO_OUT_TRIGGER_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_TRIGGER_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmp[ GPIO_OUT_2_DELEY_OFSET ] );
		count += strlen( &tmp[ GPIO_OUT_2_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	}
//================================================================
		memcpy( count, INPUTS, ( sizeof( INPUTS ) - 1 ) );
		count += sizeof( INPUTS ) - 1;
//================================================================
		memcpy( count, INPUT_1, ( sizeof( INPUT_1 ) - 1 ) );
		count += sizeof( INPUT_1 ) - 1;
		if ( 0 == GPIO_INPUT_GET(INP_1_PIN ) ) {

			memcpy( count, STATUS_LOW, ( sizeof( STATUS_LOW ) - 1 ) );
			count += sizeof( STATUS_LOW ) - 1;
		} else {
			memcpy( count, STATUS_HIGH, ( sizeof( STATUS_HIGH ) - 1 ) );
			count += sizeof( STATUS_HIGH ) - 1;
		}
//================================================================
		memcpy( count, INPUT_2, ( sizeof( INPUT_2 ) - 1 ) );
		count += sizeof( INPUT_2 ) - 1;
		if ( 0 == GPIO_INPUT_GET(INP_2_PIN ) ) {

			memcpy( count, STATUS_LOW, ( sizeof( STATUS_LOW ) - 1 ) );
			count += sizeof( STATUS_LOW ) - 1;
		} else {
			memcpy( count, STATUS_HIGH, ( sizeof( STATUS_HIGH ) - 1 ) );
			count += sizeof( STATUS_HIGH ) - 1;
		}
//================================================================
		memcpy( count, INPUT_3, ( sizeof( INPUT_3 ) - 1 ) );
		count += sizeof( INPUT_3 ) - 1;
		if ( 0 == GPIO_INPUT_GET(INP_3_PIN ) ) {

			memcpy( count, STATUS_LOW, ( sizeof( STATUS_LOW ) - 1 ) );
			count += sizeof( STATUS_LOW ) - 1;
		} else {
			memcpy( count, STATUS_HIGH, ( sizeof( STATUS_HIGH ) - 1 ) );
			count += sizeof( STATUS_HIGH ) - 1;
		}
//================================================================
		memcpy( count, INPUT_4, ( sizeof( INPUT_4 ) - 1 ) );
		count += sizeof( INPUT_4 ) - 1;
		if ( 0 == GPIO_INPUT_GET(INP_4_PIN ) ) {

			memcpy( count, STATUS_LOW, ( sizeof( STATUS_LOW ) - 1 ) );
			count += sizeof( STATUS_LOW ) - 1;
		} else {
			memcpy( count, STATUS_HIGH, ( sizeof( STATUS_HIGH ) - 1 ) );
			count += sizeof( STATUS_HIGH ) - 1;
		}
//================================================================


		*count++ = '\r';
		*count++ = '\n';
		*count = '\0';
 }


LOCAL void
ICACHE_FLASH_ATTR comandParser( void ) {

	int i = 0;

	if ( 0 == strcmp( tmp, TCP_REQUEST ) ) {

	    switch ( requestString( &tmp[ sizeof(TCP_REQUEST) + 1 ] ) ) {
	    	case WRONG_LENGHT:
	    		tcpRespounseBuilder( TCP_WRONG_LENGHT );
	    		break;
	    	case NOTHING_FOUND:
	    		tcpRespounseBuilder( TCP_NOTHING_FOUND );
	    		break;
	    	case OPERATION_OK:
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    		gpioStatusOut1 = ENABLE;
	    		break;
	    	case OPERATION_FAIL:
	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    		break;
	    	default:

	    		break;
	    }

	} else if ( 0 == strcmp( tmp, TCP_ENABLE_GPIO_1 ) ) {

	    	if ( ENABLE != gpioStatusOut1 ) {

	        	gpioStatusOut1 = ENABLE;
	        	tcpRespounseBuilder( TCP_OPERATION_OK );
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	} else if ( 0 == strcmp( tmp, TCP_ENABLE_GPIO_2 ) ) {

	    	if ( ENABLE != gpioStatusOut2 ) {

	        	gpioStatusOut2 = ENABLE;
	        	tcpRespounseBuilder( TCP_OPERATION_OK );
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	} else if ( 0 == strcmp( tmp, TCP_QUERY ) ) {

	    	lenghtQuery = StringToInt( &tmp[ sizeof( TCP_QUERY ) + 1 + sizeof( TCP_ADRESS ) + 1 + \
												  strlen( &tmp[ sizeof( TCP_QUERY ) + 1 + sizeof( TCP_ADRESS ) + 1 ] ) + 1 + \
												  1 + sizeof( TCP_LENGHT ) + 1 ] );

	    	addressQuery = StringToInt( &tmp[ sizeof( TCP_QUERY ) + 1 + sizeof( TCP_ADRESS ) + 1 ] );

	    	switch ( query( storage, &lenghtQuery, &addressQuery ) ) {

	    	    case OPERATION_OK:
	    	    	buildQueryResponse( TCP_OPERATION_OK );
	    	    	break;
	    	    case OPERATION_FAIL:
	    	    	memcpy( tmp, TCP_QUERY, sizeof(TCP_QUERY) );
	    	    	tmp[ sizeof(TCP_QUERY) ] = ' ';
	    	    	memcpy( &tmp[ sizeof(TCP_QUERY) + 1 ], TCP_OPERATION_FAIL, ( sizeof(TCP_OPERATION_FAIL) ) );

	    	    	tmp[ sizeof(TCP_QUERY) + 1 + sizeof(TCP_OPERATION_FAIL) ] = '\r';
	    	    	tmp[ sizeof(TCP_QUERY) + 1 + sizeof(TCP_OPERATION_FAIL) + 1 ] = '\n';

	    	    	if ( NULL != pespconn ) {

	    	    		espconn_sent( pespconn, tmp, ( sizeof(TCP_QUERY) + 1 + sizeof(TCP_OPERATION_FAIL) + 1 + 1 ) );
	    	    	}

	    	    	break;
	    	    case READ_DONE:

	    	    	buildQueryResponse( TCP_READ_DONE );
	    	    	break;
	    		default:

	    			break;
	    	 }

	} else if ( 0 == strcmp( tmp, TCP_INSERT ) ) {

	    	 switch ( insert( &tmp[ sizeof(TCP_INSERT) + 1 ] ) ) {
	    	    case WRONG_LENGHT:
	    	    	 tcpRespounseBuilder( TCP_WRONG_LENGHT );
	    	    	 break;
	    	    case NOT_ENOUGH_MEMORY:
	    	    	 tcpRespounseBuilder( TCP_NOT_ENOUGH_MEMORY );
	    	    	 break;
	    	    case OPERATION_OK:
	    	    	 tcpRespounseBuilder( TCP_OPERATION_OK );
	    	    	 break;
	    	    case OPERATION_FAIL:
	    	    	 tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	    	 break;
	    	    case LINE_ALREADY_EXIST:
	    	    	 tcpRespounseBuilder( TCP_LINE_ALREADY_EXIST );
	    	    	 break;
	    		default:

	    			break;
	    	 }

	    } else if ( 0 == strcmp( tmp, TCP_DELETE ) ) {

	    	 switch ( delete( &tmp[ sizeof(TCP_DELETE) + 1 ] ) ) {
	    	    case WRONG_LENGHT:
	    	    	 tcpRespounseBuilder( TCP_WRONG_LENGHT );
	    	    	 break;
	    	    case OPERATION_OK:
	    	    	 tcpRespounseBuilder( TCP_OPERATION_OK );
	    	    	 break;
	    	    case OPERATION_FAIL:
	    	    	 tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	    	 break;
	    	    case NOTHING_FOUND:
	    	    	 tcpRespounseBuilder( TCP_NOTHING_FOUND );
	    	    	 break;
	    		default:

	    			 break;
	    	 }

	    } else if ( 0 == strcmp( tmp, TCP_UPDATE ) ) {

	    	for ( i = sizeof(TCP_DELETE) + 1; '\0' != tmp[ i ]; i++ ) {

	    	}

	    	i += 2;

	    	switch ( update( &tmp[ sizeof(TCP_UPDATE) + 1 ], &tmp[i] ) ) {
	        	case WRONG_LENGHT:
	        	     tcpRespounseBuilder( TCP_WRONG_LENGHT );
	        	     break;
	        	case OPERATION_OK:
	        	     tcpRespounseBuilder( TCP_OPERATION_OK );
	        	     break;
	        	case OPERATION_FAIL:
	        	     tcpRespounseBuilder( TCP_OPERATION_FAIL );
	        	     break;
	        	case NOTHING_FOUND:
	        	     tcpRespounseBuilder( TCP_NOTHING_FOUND );
	        	     break;
	        	case LINE_ALREADY_EXIST:
	        	     tcpRespounseBuilder( TCP_LINE_ALREADY_EXIST );
	        	     break;
	        	default:

	        	break;
	        }

	    } else if ( 0 == strcmp( tmp, TCP_FIND ) ) {

	    	switch ( findString( &tmp[ sizeof(TCP_FIND) + 1 ] ) ) {
	    		 case WRONG_LENGHT:
	    		      tcpRespounseBuilder( TCP_WRONG_LENGHT );
	    		      break;
	    		 case OPERATION_FAIL:
	    		      tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    		      break;
	    		 case NOTHING_FOUND:
	    		      tcpRespounseBuilder( TCP_NOTHING_FOUND );
	    		      break;
	    		 default:
	    		       tcpRespounseBuilder( TCP_OPERATION_OK );
	    		       break;
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_RESET ) ) {

	    	system_restart();

	    } else if ( 0 == strcmp( tmp, TCP_CLEAR_HEAP ) ) {

	    	 switch ( clearSectorsDB( ) ) {
	    		 case OPERATION_OK:
	    		      tcpRespounseBuilder( TCP_OPERATION_OK );
	    		      break;
	    		  case OPERATION_FAIL:
	    		       tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    		       break;
	    		 default:

	    		     break;
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_SSID_STA ) ) { // запись во флешь, изменение после перезагрузки

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_SSID_STA) + 1 ],  SSID_MAX_LENGHT ) ) {

	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_PWD_STA ) ) { // запись во флешь, изменение после перезагрузки

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_PWD_STA) + 1 ],  PWD_MAX_LENGHT ) ) {

	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_SSID_AP ) ) {	// запись во флешь, изменение после перезагрузки

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_SSID_AP) + 1 ],  SSID_MAX_LENGHT ) ) {

	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_PWD_AP ) ) {	// запись во флешь, изменение после перезагрузки

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_PWD_AP) + 1 ],  PWD_MAX_LENGHT ) ) {

	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_BROADCAST_NAME ) ) {

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_BROADCAST_NAME) + 1 ],  BROADCAST_NAME_MAX_LENGHT ) ) {

	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_GPIO_MODE_1 ) ) {

	    	if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 ], DEF_GPIO_OUT_MODE ) ) {

	    		 writeFlash( GPIO_OUT_1_MODE_OFSET, DEF_GPIO_OUT_MODE );
	    		 gpioOutDeley1 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 + sizeof(DEF_GPIO_OUT_MODE) + 1 ] );
	    		 tcpRespounseBuilder( TCP_OPERATION_OK );

	    	} else if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 ], GPIO_OUT_TRIGGER_MODE ) ) {

	    		 writeFlash( GPIO_OUT_1_MODE_OFSET, GPIO_OUT_TRIGGER_MODE );
	    		 gpioOutDeley1 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 + sizeof(GPIO_OUT_TRIGGER_MODE) + 1 ] );
	    		 tcpRespounseBuilder( TCP_OPERATION_OK );

	    	} else { // error

		    	memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
		    	tmp[ sizeof(TCP_ERROR) ] = '\r';
		    	tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';
//debug
/*
		    	if ( NULL != pespconn ) {

		    		espconn_sent( pespconn, tmp, ( sizeof( TCP_ERROR ) + 2 ) );
		    	}
*/
//
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_GPIO_MODE_2 ) ) {

	    	if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 ], DEF_GPIO_OUT_MODE ) ) {

	    		 writeFlash( GPIO_OUT_2_MODE_OFSET, DEF_GPIO_OUT_MODE );
	    		 gpioOutDeley2 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 + sizeof(DEF_GPIO_OUT_MODE) + 1 ] );
	    		 tcpRespounseBuilder( TCP_OPERATION_OK );

	    	} else if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 ], GPIO_OUT_TRIGGER_MODE ) ) {

	    		 writeFlash( GPIO_OUT_2_MODE_OFSET, GPIO_OUT_TRIGGER_MODE );
	    		 gpioOutDeley2 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 + sizeof(GPIO_OUT_TRIGGER_MODE) + 1 ] );
	    		 tcpRespounseBuilder( TCP_OPERATION_OK );
	    	} else { // error
		    	memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
		    	tmp[ sizeof(TCP_ERROR) ] = '\r';
		    	tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';
//debug
/*
	       	    if ( NULL != pespconn ) {

	    			espconn_sent( pespconn, tmp, ( sizeof( TCP_ERROR ) + 2 ) );
	       	    }
*/
//
	    	}

	    } else { // ERROR

	    	memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
	    	tmp[ sizeof(TCP_ERROR) ] = '\r';
	    	tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';
//debug
/*
	    	if ( NULL != pespconn ) {

	    		espconn_sent( pespconn, tmp, ( sizeof( TCP_ERROR ) + 2 ) );
	    	}
*/
//
	    }
}



void ICACHE_FLASH_ATTR
tcpRespounseBuilder( uint8_t *responseCode ) {

	uint16_t i, lenght;

	for ( i = 0; '\r' != tmp[ i ] && i < TMP_SIZE; i++ ) {

	}

	for ( ; ' ' != tmp[ i ]; i-- ) {

	}

	i++;
	lenght = strlen( responseCode ) + 1 ;
	memcpy( &tmp[ i ], responseCode, lenght );
	i += lenght;
	tmp[ i++ ] = '\r';
	tmp[ i++ ] = '\n';

//debug
/*
	if ( NULL != pespconn ) {

		espconn_sent( pespconn, tmp, i );
	}
*/
//
}


/*
 * compStr ICACHE_FLASH_ATTR
 * compareLenght( uint8_t *string, uint16_t maxLenght )
 *
 * Возвращаемые значения:
 * LENGHT_ERROR
 * LENGHT_OK
 *
 * @Description
 * 		длина string сравнивается с maxLenght без учета нуль символа
 */
compStr ICACHE_FLASH_ATTR
compareLenght( uint8_t *string, uint16_t maxLenght ) {

	uint16_t i;

	for ( i = 0; '\0' != string[ i ]; i++ ) {

		if ( '\0' == string[ i ] || i > maxLenght ) {

			break;
		}
	}

	if ( i > maxLenght || i == 0  ) {

		return LENGHT_ERROR;
	} else  {

		return LENGHT_OK;
	}
}


void ICACHE_FLASH_ATTR
buildQueryResponse( uint8_t *responseStatus ) {

	uint16_t i;

	uint8_t *p = &tmp[ sizeof(TCP_QUERY) + 1 + sizeof(TCP_LENGHT) + 1 ];

	p = ShortIntToString( lenghtQuery, p );
	*p++ = '\0';
	*p++ = ' ';
	memcpy( p, TCP_ADRESS, sizeof( TCP_ADRESS ) );
	p += sizeof( TCP_ADRESS );
	*p++ = ' ';
	p = ShortIntToString( addressQuery, p );
	*p++ = '\0';
	*p++ = '\r';
	*p = '\n';

	i = p - tmp + 1;
//debug
/*
if ( NULL != pespconn ) {

	espconn_sent( pespconn, tmp, i );
}
*/
//

}













