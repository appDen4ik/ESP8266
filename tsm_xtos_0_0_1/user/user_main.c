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

LOCAL uint8_t ICACHE_FLASH_ATTR StringToChar(uint8_t *data);
LOCAL uint8_t* ICACHE_FLASH_ATTR  ShortIntToString(uint16_t data, uint8_t *adressDestenation);
LOCAL void ICACHE_FLASH_ATTR senddata( void );
LOCAL void ICACHE_FLASH_ATTR initPeriph( void );
LOCAL uint8_t * ICACHE_FLASH_ATTR intToStringHEX(uint8_t data, uint8_t *adressDestenation);
LOCAL void ICACHE_FLASH_ATTR broadcastBuilder( void );

//****************************************************************************************************************************
uint8_t tmpTest[SPI_FLASH_SEC_SIZE];
//****************************************************************************************************************************


extern void ets_wdt_enable (void);
extern void ets_wdt_disable (void);



LOCAL struct espconn broadcast;
LOCAL esp_udp udp;


LOCAL uint16_t server_timeover = 20;
LOCAL struct espconn server;
LOCAL esp_tcp tcp1;
struct station_config stationConf;
struct softap_config softapConf;


uint8_t hello[1000];
uint16_t i;
uint32_t adr;

struct ip_info inf;

uint8_t ledState = 0;


uint8_t nameSTA[100] = "esp8266 1";
uint8_t brodcastMessage[250] = { 0 };
uint8_t tmp[250] = { 0 };
uint8_t *count;
uint8_t macadr[10];
uint16_t servPort = 80;
sint8_t rssi;


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
    memcpy( tmp, pdata, len );
    tmp[len++] = '\r';
    tmp[len] = '\0';
    espconn_sent(arg, tmp, strlen(tmp));
//    uart0_tx_buffer(pdata, len);
	for ( ; i++ < len; ){
		uart_tx_one_char(*pdata++);
	}
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
sendDatagram(char *datagram, uint16 size) {
	os_timer_disarm(&task_timer);
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

	os_timer_setfn(&task_timer, (os_timer_func_t *)sendDatagram, (void *)0);
	os_timer_arm(&task_timer, DELAY, 0);
}

void ICACHE_FLASH_ATTR
user_init(void) {

	initPeriph();

//*********************************************************************************************************************

	uint32_t data0 = 0;
	uint32_t data3 = 0;
	uint8_t ascii[10];
	static uint8_t string[STRING_SIZE];

	uint8_t alignString[ALIGN_STRING_SIZE];
	uint8_t alignStr[ALIGN_STRING_SIZE];

	uint8_t *p;

	uint16_t c, currentSector;
	uint32_t a, i;

	result res;


	for ( i = STRING_SIZE; i < ALIGN_STRING_SIZE; i++ ) {
		alignString[i] = 0xff;
	}

	clearSectorsDB();

	os_delay_us(500000);

	ets_uart_printf(" Проверка updateLine Тест 1 ");

	os_delay_us(500000);

	ets_uart_printf(" П.1 Заполняем память значениями (ASCII) выровнеными по 101 символов  ");

	for ( ; res != NOT_ENOUGH_MEMORY; ) {


		for ( a = 0; a < STRING_SIZE - 1; a++ ) {
				alignString[a] = '0';
		}
		data0++;
		p = ShortIntToString(data0, ascii);
		memcpy( &alignString[ STRING_SIZE - 1 - (p - ascii) ], ascii, (p - ascii) );
		alignString[STRING_SIZE - 1] = '\0';

//		os_printf( " \n %s \n String lenght %d", alignString, (strlen(alignString) + 1) );

		switch ( res = insert( alignString ) ) {
			case OPERATION_OK:
				//ets_uart_printf("OPERATION_OK");
				system_soft_wdt_stop();
				break;
			case WRONG_LENGHT:
				ets_uart_printf("WRONG_LENGHT");
				goto m;
			case OPERATION_FAIL:
				ets_uart_printf("OPERATION_FAIL");
				goto m;
			case LINE_ALREADY_EXIST:
				ets_uart_printf("LINE_ALREADY_EXIST");
				goto m;
			case NOT_ENOUGH_MEMORY:
				ets_uart_printf("NOT_ENOUGH_MEMORY");
				system_soft_wdt_stop();
				break;

		}

	}

	os_delay_us(500000);
	ets_uart_printf(" Запрос db ");
	os_delay_us(500000);


/*	for ( currentSector = START_SECTOR; currentSector <= END_SECTOR; currentSector++ ) {
		os_printf( " currentSector   %d", currentSector);
		spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)tmpTest, SPI_FLASH_SEC_SIZE );
		for ( c = 0; SPI_FLASH_SEC_SIZE > c; c++ ) {
			uart_tx_one_char(tmpTest[c]);
		}
		system_soft_wdt_stop();
	}
*/
		os_delay_us(500000);
		ets_uart_printf(" Проверка updateLine Тест 1 . Увеличения значения в каждой записи");
		os_delay_us(500000);

		data0--;

		for ( data3 = 1; data3 <= data0; data3++ ) {


			for ( a = 0; a < STRING_SIZE - 1; a++ ) {
					alignString[a] = '0';
			}


			for ( a = 0; a < STRING_SIZE - 1; a++ ) {
					alignStr[a] = '0';
			}

			p = ShortIntToString(data3, ascii);
			memcpy( &alignString[ STRING_SIZE - 1 - (p - ascii) ], ascii, (p - ascii) );
			alignString[STRING_SIZE - 1] = '\0';

			a = data0 + data3;

			p = ShortIntToString(a, ascii);
			memcpy( &alignStr[ STRING_SIZE - 1 - (p - ascii) ], ascii, (p - ascii) );
			alignStr[STRING_SIZE - 1] = '\0';

			os_printf( " \n %s \n String 1 lenght %d", alignString, (strlen(alignString) + 1) );
			os_printf( " \n %s \n String 2 lenght %d", alignStr, (strlen(alignStr) + 1) );

			switch ( res = update( alignString, alignStr ) ) {
				case OPERATION_OK:
					ets_uart_printf("OPERATION_OK");
					system_soft_wdt_stop();
					break;
				case WRONG_LENGHT:
					ets_uart_printf("WRONG_LENGHT");
					goto m;
				case OPERATION_FAIL:
					ets_uart_printf("OPERATION_FAIL");
					goto m;
				case LINE_ALREADY_EXIST:
					ets_uart_printf("LINE_ALREADY_EXIST");
					goto m;
				case NOTHING_FOUND:
					ets_uart_printf("NOTHING_FOUND");
					goto m;
			}

		}


		for ( currentSector = START_SECTOR; currentSector <= END_SECTOR; currentSector++ ) {
			os_printf( " currentSector   %d", currentSector);
			spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)tmpTest, SPI_FLASH_SEC_SIZE );
			for ( c = 0; SPI_FLASH_SEC_SIZE > c; c++ ) {
				uart_tx_one_char(tmpTest[c]);
			}
			system_soft_wdt_stop();
		}



		ets_uart_printf(" Тестирование успешно завершено ");

m:

//*********************************************************************************************************************

	os_printf( "SDK version: %s", system_get_sdk_version() );

//------------------------------------------------------------------
	wifi_station_disconnect();
	wifi_station_dhcpc_stop();

	wifi_set_opmode( STATIONAP_MODE );
	memcpy(&stationConf.ssid, SSID_STA, sizeof(SSID_STA));
	memcpy(&stationConf.password, PWD_STA, sizeof(PWD_STA));
	wifi_station_set_config(&stationConf);

	wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);

//-----------------------------------------------------------------
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
	os_timer_setfn(&task_timer, (os_timer_func_t *)sendDatagram, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&task_timer, DELAY, 0);

}

void senddata( void ){

	broadcastBuilder();
	espconn_create(&broadcast);
	espconn_sent(&broadcast, brodcastMessage, strlen(brodcastMessage));
	espconn_delete(&broadcast);

}

void ICACHE_FLASH_ATTR initPeriph( void ) {

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
uint8_t StringToChar(uint8_t *data) {
	uint8_t returnedValue = 0;
	for (;*data >= '0' && *data <= '9'; data++)
	returnedValue = 10 * returnedValue + (*data - '0');
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
			if ( PHY_MODE_11B == (phyMode = wifi_get_phy_mode()) ){
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







