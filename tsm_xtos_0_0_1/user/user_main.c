/**
 ************************************************************************************
  * @file    user_main.c
  * @author  Denys
  * @version V0.0.1
  * @date    2-Sept-2015
  * @brief
 ************************************************************************************
 * @info:
 * 		esp8266 работает в режиме softAP и STA. Ќа ней запущен TCP сервер
 * который может обрабатывать определенный список команд ( который будет
 * определлен и реализован в следующей версии ), в данной же версии,
 * при получении любой информации она просто валитс€ в уарт. “акже, если есп
 * настроен на определенный роутер, то он пытаетс€ подключитьс€ к нему, в
 * случае успешного подключени€ есп udp бродкастом шлет через каждый
 * определенный промежуток времени необходимую информацию (свой ip, номер пората
 * , дл€ того чтобы данную есп можна было найти в сети и подключитьс€ с TCP
 * серверу с дальшей передачей необходимой информации. ќбратна€ св€зь реализована
 * через широковещительные запроси, через те же запросы, через которые передаетс€
 * информаци€ котора€ необходима, дл€ того чтобы можна было найти есп внутри
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



LOCAL uint8_t ICACHE_FLASH_ATTR StringToChar(uint8_t *data);
LOCAL uint8_t* ICACHE_FLASH_ATTR  ShortIntToString(uint16_t data, uint8_t *adressDestenation);
LOCAL void ICACHE_FLASH_ATTR senddata( void );
LOCAL void ICACHE_FLASH_ATTR initPeriph( void );
LOCAL uint8_t * ICACHE_FLASH_ATTR intToStringHEX(uint8_t data, uint8_t *adressDestenation);



LOCAL struct espconn broadcast;
LOCAL esp_udp udp;


LOCAL uint16_t server_timeover = 180;
LOCAL struct espconn server;
LOCAL esp_tcp tcp1;
struct station_config stationConf;


uint8_t hello[1000];
uint16_t i;
uint32_t adr;

struct ip_info inf;

uint8_t ledState = 0;


uint8_t nameSTA[100] = "esp8266 1";
uint8_t brodcastMessage[250] = { 0 };
uint8_t *count;
uint8_t macadr[10];
uint16_t servPort = 80;
sint8_t rssi;


LOCAL os_timer_t task_timer;

void user_rf_pre_init(void)
{

}


LOCAL void ICACHE_FLASH_ATTR
tcp_recvcb(void *arg, char *pdata, unsigned short len)
{
	int i = 0;
	 uint8_t mes[] = "HELLO MAN";
    struct espconn *pespconn = (struct espconn *) arg;
    //  if (*pdata == '+') {
    	GPIO_OUTPUT_SET(OUT_1_GPIO, ledState);
    			ledState ^=1;
//    }
    espconn_sent(arg, mes, strlen(mes));
    ets_uart_printf("Hello World!\r\n");
//    uart0_tx_buffer(pdata, len);
	for ( ; i++ < len; ){
		uart_tx_one_char(*pdata++);
	}
}


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
					ets_uart_printf("WiFi connected\r\n");
					os_printf( "rssi = %s", brodcastMessage);
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
user_init(void)
{
	initPeriph( );
	GPIO_OUTPUT_SET(OUT_1_GPIO, 1);

	for ( i = 0; i < sizeof(hello); i++ ) {
		hello[i] = i;
		uart_tx_one_char(hello[i]);
	}
	spi_flash_write( 169000, (uint32 *)hello, strlen(hello) );
	for ( adr = 70000; adr < 170000; adr += sizeof(hello) ) {
	 spi_flash_read( adr, (uint32 *)hello, sizeof(hello) );
	}
	GPIO_OUTPUT_SET(OUT_2_GPIO, 1);
	ets_uart_printf("TISO ver0.1");
	uart_tx_one_char(system_update_cpu_freq(SYS_CPU_160MHZ));
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_install_putc1(uart_tx_one_char);


	for ( i = 0; i < sizeof(hello); i++ ) {
//		uart_tx_one_char(hello[i]);
	}


#ifdef DEBUG
	os_printf( "SDK version: %s", system_get_sdk_version() );
#endif

//------------------------------------------------------------------
	wifi_station_disconnect();
	wifi_station_dhcpc_stop();

	wifi_set_opmode( STATION_MODE );
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



// broadcast message:
//          "name:" + " " + nameSTA +
//          + " " + mac: + " " + wifi_get_macaddr() +
//          + " " + ip: + " " + wifi_get_ip_info( ) +
//          + " " + server port: + " " servPort + " " +
//			+ " " + phy mode: + " " + wifi_get_phy_mode() +
//          + " " + rssi: + " " + wifi_station_get_rssi() +
//          + " " + statuses: + " " + doorClose: + " " + closed/open +
//          + " " + doorOpen: + " " + closed/open + "\r\n" + "\0"
LOCAL void ICACHE_FLASH_ATTR senddata( void )
{


	//выдел€ем бродкаст айпишку
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
	if ( 0 == GPIO_INPUT_GET(DOOR_CLOSE_SENSOR_PIN ) ) {
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
	if ( 0 == GPIO_INPUT_GET(DOOR_OPEN_SENSOR_PIN ) ) {
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

//	os_printf( "SDK version: %s", macadr );

	espconn_create(&broadcast);
	espconn_sent(&broadcast, brodcastMessage, strlen(brodcastMessage));
	espconn_delete(&broadcast);

}

LOCAL void ICACHE_FLASH_ATTR initPeriph( void ){

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

// ѕеревод дес€тичного числа в последовательность ASCII
uint8_t * ShortIntToString(uint16_t data, uint8_t *adressDestenation) {
	uint8_t *startAdressDestenation = adressDestenation;
	uint8_t *endAdressDestenation;
	uint8_t buff;

	do {// перевод входного значени€ в последовательность ASCII кодов
		// в обратном пор€дке
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

// ѕеревод последовательности ASCII в число
uint8_t StringToChar(uint8_t *data) {
	uint8_t returnedValue = 0;
	for (;*data >= '0' && *data <= '9'; data++)
	returnedValue = 10 * returnedValue + (*data - '0');
	return returnedValue;
}

// ѕеревод шестнадцетиричного числа в последовательность ASCII
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









