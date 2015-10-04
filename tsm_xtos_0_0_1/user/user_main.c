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

LOCAL uint32_t ICACHE_FLASH_ATTR StringToInt(uint8_t *data);
LOCAL uint8_t* ICACHE_FLASH_ATTR  ShortIntToString(uint16_t data, uint8_t *adressDestenation);


LOCAL void ICACHE_FLASH_ATTR senddata( void );

LOCAL uint8_t * ICACHE_FLASH_ATTR intToStringHEX(uint8_t data, uint8_t *adressDestenation);
LOCAL void ICACHE_FLASH_ATTR broadcastBuilder( void );

LOCAL inline void ICACHE_FLASH_ATTR checkFlash( void );
LOCAL inline void ICACHE_FLASH_ATTR writeFlash( uint16_t where, uint8_t *what );

LOCAL inline void ICACHE_FLASH_ATTR initPeriph( void );
LOCAL inline void ICACHE_FLASH_ATTR initWIFI( void );

//*************************************************************************************************************


extern void ets_wdt_enable (void);
extern void ets_wdt_disable (void);


//**************************************************************************************************************

LOCAL uint8_t tmp[TMP_SIZE];

//**************************************************************************************************************

LOCAL uint32_t gpioOutImpulse1;
LOCAL uint32_t gpioOutImpulse2;

LOCAL uint8_t gpioModeOut1[ sizeof(DEF_GPIO_OUT_MODE) ];
LOCAL uint8_t gpioModeOut2[ sizeof(DEF_GPIO_OUT_MODE) ];

LOCAL stat gpioStatusOut1 = DISABLE;
LOCAL stat gpioStatusOut2 = DISABLE;

//**************************************************************************************************************


uint8_t alignString[ALIGN_STRING_SIZE];

LOCAL struct espconn broadcast;
LOCAL esp_udp udp;


LOCAL uint16_t server_timeover = 20;
LOCAL struct espconn server;
LOCAL esp_tcp tcp1;






struct ip_info inf;

uint8_t ledState = 0;


uint8_t nameSTA[100] = "esp8266 1";
uint8_t brodcastMessage[250] = { 0 };
uint8_t *count;
uint8_t macadr[10];
uint16_t servPort = 80;
sint8_t rssi;

uint8_t storage[1000];


LOCAL os_timer_t task_timer;

void user_rf_pre_init(void)
{

}

//*******************************************************Callbacks***********************************************************************
LOCAL void ICACHE_FLASH_ATTR
tcp_recvcb(void *arg, char *pdata, unsigned short len)
{
	int i = 0;
    struct espconn *pespconn = (struct espconn *) arg;
    //  if (*pdata == '+') {
    	GPIO_OUTPUT_SET(OUT_1_GPIO, ledState);
    			ledState ^=1;
//    }
 /*   memcpy( tmp, pdata, len );
    tmp[len++] = '\r';
    tmp[len] = '\0';
    espconn_sent(arg, tmp, strlen(tmp));
//    uart0_tx_buffer(pdata, len);
	for ( ; i++ < len; ){
		uart_tx_one_char(*pdata++);
	}*/
}

LOCAL void ICACHE_FLASH_ATTR
tcp_connectcb(void* arg){
	ets_uart_printf("Connect");
}

LOCAL void ICACHE_FLASH_ATTR
tcp_disnconcb(void* arg){
	ets_uart_printf("Disconnect");
//	os_printf( "disconnect result = %d", espconn_delete((struct espconn *) arg));
}

LOCAL void ICACHE_FLASH_ATTR
tcp_reconcb(void* arg){

}

LOCAL void ICACHE_FLASH_ATTR
tcp_sentcb(void* arg){

}

//************************************************************************************************************************




void ICACHE_FLASH_ATTR
mScheduler(char *datagram, uint16 size) {

	os_timer_disarm(&task_timer);

	struct station_info * station = wifi_softap_get_station_info();

	broadcastBuilder();

// Внутрення сеть
	while ( station ) {

		IP4_ADDR((ip_addr_t *)broadcast.proto.udp->remote_ip, (uint8_t)(station->ip.addr), (uint8_t)(station->ip.addr >> 8),\
															(uint8_t)(station->ip.addr >> 16), (uint8_t)(station->ip.addr >> 24) );

		os_printf( "bssid : %x:%x:%x:%x:%x:%x ip : %d.%d.%d.%d ", MAC2STR(station->bssid), IP2STR(&station->ip) );

		espconn_create(&broadcast);
		espconn_sent(&broadcast, brodcastMessage, strlen(brodcastMessage));
		espconn_delete(&broadcast);

		station = STAILQ_NEXT(station, next);
	}

	wifi_softap_free_station_info();

// Внешняя сеть
	switch(wifi_station_get_connect_status())
	{
		case STATION_GOT_IP:
			if ( ( rssi = wifi_station_get_rssi() ) < -90 ){
				count = brodcastMessage;
				*count++ = '-';
				count = ShortIntToString( ~rssi, count );
				*count = '\0';
				os_printf( "Bad signal, rssi = %s", brodcastMessage);
				GPIO_OUTPUT_SET(LED_GPIO, ledState);
				ledState ^=1;
			} else {
				count = brodcastMessage;
				*count++ = '-';
				count = ShortIntToString( ~rssi, count );
				*count = '\0';

				GPIO_OUTPUT_SET(LED_GPIO, 1);
				wifi_get_ip_info(STATION_IF, &inf);
				if(inf.ip.addr != 0) {
					#ifdef DEBUG
			//		ets_uart_printf("WiFi connected\r\n");
				//	os_printf( "rssi = %s", brodcastMessage);
					#endif
					senddata();
				}
			}
			break;
		case STATION_WRONG_PASSWORD:
			#ifdef DEBUG
			ets_uart_printf("WiFi connecting error, wrong password\r\n");
			#endif
			GPIO_OUTPUT_SET(LED_GPIO, ledState);
			ledState ^=1;
			break;
		case STATION_NO_AP_FOUND:
			#ifdef DEBUG
			ets_uart_printf("WiFi connecting error, ap not found\r\n");
			#endif
			GPIO_OUTPUT_SET(LED_GPIO, ledState);
			ledState ^=1;
			break;
		case STATION_CONNECT_FAIL:
			#ifdef DEBUG
			ets_uart_printf("WiFi connecting fail\r\n");
			#endif
			GPIO_OUTPUT_SET(LED_GPIO, ledState);
			ledState ^=1;
			break;
		default:
			#ifdef DEBUG
			ets_uart_printf("WiFi connecting...\r\n");
			#endif
			GPIO_OUTPUT_SET(LED_GPIO, ledState);
			ledState ^=1;
			break;

	}

	os_timer_setfn(&task_timer, (os_timer_func_t *)mScheduler, (void *)0);
	os_timer_arm(&task_timer, DELAY, 0);
}

void ICACHE_FLASH_ATTR
user_init(void) {

	initPeriph();

	checkFlash();

	initWIFI();

	if ( SPI_FLASH_RESULT_OK == spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
				                            (uint32 *)tmp, ALIGN_FLASH_READY_SIZE ) ) {

		if ( 0 == strcmp( &tmp[GPIO_OUT_1_MODE_OFSET], DEF_GPIO_OUT_MODE ) ) {

			gpioOutImpulse1 = StringToInt( &tmp[GPIO_OUT_1_DELEY_OFSET] );

			os_sprintf( gpioModeOut1, "%s", &tmp[ GPIO_OUT_1_MODE_OFSET ] );
		}
#ifdef DEBUG
	  os_printf( " gpioModeOut1  %s,  gpioOutImpulse1  %d", gpioModeOut1, gpioOutImpulse1 );
	  os_delay_us(500000);
#endif

		if ( 0 == strcmp( &tmp[GPIO_OUT_2_MODE_OFSET], DEF_GPIO_OUT_MODE ) ) {

			gpioOutImpulse2 = StringToInt( &tmp[GPIO_OUT_2_DELEY_OFSET] );

			os_sprintf( gpioModeOut2, "%s", &tmp[ GPIO_OUT_2_MODE_OFSET ] );
		}
#ifdef DEBUG
	  os_printf( " gpioModeOut2  %s,  gpioOutImpulse2  %d", gpioModeOut2, gpioOutImpulse2 );
	  os_delay_us(500000);
#endif

	}



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

	{ //tcp сервер
		server.type = ESPCONN_TCP;
		server.state = ESPCONN_NONE;
		server.proto.tcp = &tcp1;
		server.proto.tcp->local_port = servPort;

		espconn_accept(&server);
		espconn_regist_time(&server, server_timeover, 0);
		espconn_regist_recvcb(&server, tcp_recvcb);
		espconn_regist_connectcb(&server, tcp_connectcb);
		espconn_regist_disconcb(&server, tcp_disnconcb);
	}


    { //udp клиент
    	broadcast.type = ESPCONN_UDP;
    	broadcast.state = ESPCONN_NONE;
    	broadcast.proto.udp = &udp;
    	broadcast.proto.udp->remote_port = 9876;
    }

	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&task_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&task_timer, (os_timer_func_t *)mScheduler, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&task_timer, DELAY, 0);

}

void senddata( void ){

	broadcastBuilder();
	espconn_create(&broadcast);
	espconn_sent(&broadcast, brodcastMessage, strlen(brodcastMessage));
	espconn_delete(&broadcast);

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

	PIN_FUNC_SELECT(OUT_2_MUX, OUT_2_FUNC);

	PIN_FUNC_SELECT(LED_MUX, LED_FUNC);

	PIN_FUNC_SELECT(INP_1_MUX, INP_1_FUNC);
	gpio_output_set(0, 0, 0, INP_1);

	PIN_FUNC_SELECT(INP_2_MUX, INP_2_FUNC);
	gpio_output_set(0, 0, 0, INP_2);

	PIN_FUNC_SELECT(INP_3_MUX, INP_3_FUNC);
	gpio_output_set(0, 0, 0, INP_3);

	PIN_FUNC_SELECT(INP_3_MUX, INP_3_FUNC);
	gpio_output_set(0, 0, 0, INP_3);

}


void ICACHE_FLASH_ATTR
initWIFI( ) {

	struct station_config stationConf;
	struct softap_config softapConf;
	struct ip_info ipinfo;

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {

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

		IP4_ADDR(&ipinfo.ip, 10, 11, 1, 1);
		IP4_ADDR(&ipinfo.gw, 10, 11, 1, 1);
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


void ICACHE_FLASH_ATTR
writeFlash( uint16_t where, uint8_t *what ) {


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
//          + " " + statuses: + " " + doorClose: + " " + closed/open +
//          + " " + doorOpen: + " " + closed/open + "\r\n" + "\0"
void ICACHE_FLASH_ATTR
broadcastBuilder( void ){
	//выделяем бродкаст айпишку
	wifi_get_ip_info( STATION_IF, &inf );

	IP4_ADDR((ip_addr_t *)broadcast.proto.udp->remote_ip, (uint8_t)(inf.ip.addr), (uint8_t)(inf.ip.addr >> 8),\
															(uint8_t)(inf.ip.addr >> 16), 255);

	count = brodcastMessage;

	memcpy( count, NAME, ( sizeof( NAME ) - 1 ) );
	count += sizeof( NAME ) - 1;

	memcpy( count, nameSTA, strlen( nameSTA ) );
	count += strlen( nameSTA );
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
    if ( ( rssi = wifi_station_get_rssi() ) < 0 ) {
    	memcpy( count, RSSI, ( sizeof( RSSI ) - 1 ) );
	    	count += sizeof( RSSI ) - 1;

	    	*count++ = '-';
	//os_printf( "SDK version: %d", wifi_station_get_rssi() );
//		uart_tx_one_char( wifi_station_get_rssi() );
	    	count = ShortIntToString( ~rssi, count );
}
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

		count = ShortIntToString( servPort, count );
//================================================================
		memcpy( count, STATUSES, ( sizeof( STATUSES ) - 1 ) );
		count += sizeof( STATUSES ) - 1;
//================================================================
		memcpy( count, DOOR_CLOSE_SENSOR, ( sizeof( DOOR_CLOSE_SENSOR ) - 1 ) );
		count += sizeof( DOOR_CLOSE_SENSOR ) - 1;
		if ( 0 == GPIO_INPUT_GET(INP_1_PIN ) ) {
			memcpy( count, CLOSE, ( sizeof( CLOSE ) - 1 ) );
			GPIO_OUTPUT_SET(OUT_1_GPIO, 1);//
			count += sizeof( CLOSE ) - 1;
		} else {
			memcpy( count, OPEN, ( sizeof( OPEN ) - 1 ) );
			count += sizeof( OPEN ) - 1;
		}
//================================================================
		memcpy( count, DOOR_OPEN_SENSOR, ( sizeof( DOOR_OPEN_SENSOR ) - 1 ) );
		count += sizeof( DOOR_OPEN_SENSOR ) - 1;
		if ( 0 == GPIO_INPUT_GET(INP_1_PIN ) ) {
			memcpy( count, CLOSE, ( sizeof( CLOSE ) - 1 ) );
			GPIO_OUTPUT_SET(OUT_1_GPIO, 0);//
			count += sizeof( CLOSE ) - 1;
		} else {
			memcpy( count, OPEN, ( sizeof( OPEN ) - 1 ) );
			count += sizeof( OPEN ) - 1;
		}

		*count++ = '\r';
		*count++ = '\n';
		*count = '\0';
 }







