/**
 ************************************************************************************
  * @file    user_main.c
  * @author  Denys
  * @version V0.0.1
  * @date    2-Sept-2015
  * @brief
 ************************************************************************************
 * @info:
 * 		esp8266 �������� � ������ softAP � STA. �� ��� ������� TCP ������
 * ������� ����� ������������ ������������ ������ ������ ( ������� �����
 * ���������� � ���������� � ��������� ������ ), � ������ �� ������,
 * ��� ��������� ����� ���������� ��� ������ ������� � ����. �����, ���� ���
 * �������� �� ������������ ������, �� �� �������� ������������ � ����, �
 * ������ ��������� ����������� ��� udp ���������� ���� ����� ������
 * ������������ ���������� ������� ����������� ���������� (���� ip, ����� ������
 * , ��� ���� ����� ������ ��� ����� ���� ����� � ���� � ������������ � TCP
 * ������� � ������� ��������� ����������� ����������. �������� ����� �����������
 * ����� ����������������� �������, ����� �� �� �������, ����� ������� ����������
 * ���������� ������� ����������, ��� ���� ����� ����� ���� ����� ��� ������
 * ��������� ����.
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

extern void ets_wdt_enable (void);
extern void ets_wdt_disable (void);

LOCAL struct espconn broadcast;
LOCAL esp_udp udp;


LOCAL uint16_t server_timeover = 20;
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
user_init(void)
{

	initPeriph( );
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	{
	uint8_t testLine[13] = "380673173093";
	testLine[12] = 3;
	uint8_t secondLine[13] = "380673176890";
	uint8_t thirdLine[13] = "123456789012";



//=======================

		static uint8_t markerDisable[4] = { MARKER_DISABLE, 0xff, 0xff, 0xff };
		uint8_t firstLine[13] = "380673173093";
		uint8_t temp[0x1000];
		uint16_t i;
		ets_wdt_enable();
		ets_wdt_disable();
		ets_uart_printf("Begin");
		system_soft_wdt_stop();
	//	cleanAllSectors();
		ets_uart_printf("clean OK");
//		writeLine(firstLine);
//		writeLine(secondLine);
//		writeLine(thirdLine);


		spi_flash_write( 50*SPI_FLASH_SEC_SIZE, (uint32 *)markerDisable, 1 );
		spi_flash_write( (50*SPI_FLASH_SEC_SIZE + 1), (uint32 *)firstLine, 16 );
		spi_flash_read( 50*SPI_FLASH_SEC_SIZE , (uint32 *)temp, 0x1000 );
		  for (  i = 0; i < 0x1000; i++ ){
			  uart_tx_one_char(temp[i]);
		  }

/*		if ( OPERATION_OK == foundLine( firstLine ) ) {
			ets_uart_printf("string found");
		} else {
			ets_uart_printf("string not found");
		}*/

	}

	system_soft_wdt_restart();
//=======================
	ets_uart_printf("TISO ver0.1");
	uart_tx_one_char(system_update_cpu_freq(SYS_CPU_160MHZ));
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
	{ //tcp ������
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


    { //udp ������
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


LOCAL void ICACHE_FLASH_ATTR senddata( void )
{

	broadcastBuilder();
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

// ������� ����� � ������������������ ASCII
uint8_t * ShortIntToString(uint16_t data, uint8_t *adressDestenation) {
	uint8_t *startAdressDestenation = adressDestenation;
	uint8_t *endAdressDestenation;
	uint8_t buff;

	do {// ������� �������� �������� � ������������������ ASCII �����
		// � �������� �������
		*adressDestenation++ = data % 10 + '0';
	} while ((data /= 10) > 0);
	endAdressDestenation = adressDestenation ;

	// �������� ������������������
	for (adressDestenation--; startAdressDestenation < adressDestenation;startAdressDestenation++, adressDestenation--) {
		buff = *startAdressDestenation;
		*startAdressDestenation = *adressDestenation;
		*adressDestenation = buff;
	}
	return endAdressDestenation;
}

// ������� ������������������ ASCII � �����
uint8_t StringToChar(uint8_t *data) {
	uint8_t returnedValue = 0;
	for (;*data >= '0' && *data <= '9'; data++)
	returnedValue = 10 * returnedValue + (*data - '0');
	return returnedValue;
}

// ������� ������������������ ����� � ������������������ ASCII
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
	//�������� �������� �������
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
}




