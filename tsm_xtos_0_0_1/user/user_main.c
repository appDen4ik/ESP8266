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
#include "driver/gpio16.h"
#include "driver/myDB.h"

//**********************************************************************************************************************************
LOCAL inline void ICACHE_FLASH_ATTR comandParser( void )  __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR initPeriph( void )  __attribute__((always_inline));
LOCAL void ICACHE_FLASH_ATTR initWIFI( void );
LOCAL inline void ICACHE_FLASH_ATTR loadDefParam( void ) __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR broadcastBuilder( void ) __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR buildQueryResponse( uint8_t *responseStatus ) __attribute__((always_inline));
LOCAL void ICACHE_FLASH_ATTR tcpRespounseBuilder( uint8_t *responseStatus );
LOCAL void ICACHE_FLASH_ATTR mScheduler(void);
LOCAL compStr ICACHE_FLASH_ATTR compareLenght( uint8_t *string, uint16_t maxLenght, uint16_t minLenght );
LOCAL uint32_t ICACHE_FLASH_ATTR StringToInt(uint8_t *data);
LOCAL uint8_t* ICACHE_FLASH_ATTR  ShortIntToString(uint32_t data, uint8_t *adressDestenation);
LOCAL uint8_t * ICACHE_FLASH_ATTR intToStringHEX(uint8_t data, uint8_t *adressDestenation);
LOCAL res ICACHE_FLASH_ATTR writeFlash( uint16_t where, uint8_t *what );
//**********************************************************************************************************************************
extern void ets_wdt_enable(void);
extern void ets_wdt_disable(void);
extern void ets_wdt_init(void);
extern void uart_tx_one_char();
//**********************************************************************************************************************************
//gpio
LOCAL uint32_t gpioOutDeley1;
LOCAL uint32_t gpioOutDeley2;

LOCAL uint32_t gpioOutDeley1Counter;
LOCAL uint32_t gpioOutDeley2Counter;

LOCAL uint8_t gpioModeOut1[ sizeof(GPIO_OUT_IMPULSE_MODE) ];
LOCAL uint8_t gpioModeOut2[ sizeof(GPIO_OUT_IMPULSE_MODE) ];

LOCAL stat gpioStatusOut1 = DISABLE;
LOCAL stat gpioStatusOut2 = DISABLE;
//**********************************************************************************************************************************
LOCAL os_timer_t task_timer;
//**********************************************************************************************************************************
LOCAL uint32_t resetDHCP = 0;
//**********************************************************************************************************************************
LOCAL uint8_t ledState = 0;
//**********************************************************************************************************************************
LOCAL uint32_t broadcastTmr;
LOCAL uint8_t *broadcastShift;
LOCAL uint8_t brodcastMessage[1000] = { 0 };
LOCAL uint8_t broadcastTmp[SPI_FLASH_SEC_SIZE]; //broadcast
//**********************************************************************************************************************************
LOCAL sint8_t rssi;
//**********************************************************************************************************************************
LOCAL uint8_t writeFlashTmp[ SPI_FLASH_SEC_SIZE ];
//**********************************************************************************************************************************
LOCAL struct ip_info inf;
//**********************************************************************************************************************************
uint32_t addressQuery;
uint16_t lenghtQuery;
//**********************************************************************************************************************************
//authorization tcp
LOCAL tcp_stat tcpSt = TCP_FREE;
LOCAL struct espconn *pespconn;
LOCAL uint32_t ipAdd;
//**********************************************************************************************************************************
LOCAL uint8_t routerSSID[32];
LOCAL uint8_t routerPWD[64];
//**********************************************************************************************************************************
LOCAL struct espconn espconnServer;
LOCAL esp_tcp tcpServer;
LOCAL uint8_t storage[1000];
LOCAL uint8_t tmp[1700]; // только для работы с сетью, приема запросов и формирования ответов
//**********************************************************************************************************************************
LOCAL struct  espconn espconnBroadcastAP;
LOCAL esp_udp espudpBroadcastAP;
//**********************************************************************************************************************************
LOCAL struct  espconn espconnBroadcastSTA;
LOCAL esp_udp espudpBroadcastSTA;
//**********************************************************************************************************************************
//tasks
LOCAL os_event_t *disconQueue;
#define DISCON_QUEUE_LENGHT  		1
#define DISCON_QUEUE_PRIO    		2
#define DISCON_ETS_SIGNAL_TOKEN	 	0xfa

LOCAL os_event_t *cmdPrsQueue;
#define CMD_PRS_QUEUE_LENGHT 				1
#define CMD_PRS_QUEUE_PRIO   				1
#define CMD_PRS_QUEUE_ETS_SIGNAL_TOKEN		0XF
#define CMD_PRS_QUEUE_ETS_PARAM_TOKEN		0XF
//**********************************************************************************************************************************



//**********************************************************************************************************************************
LOCAL void ICACHE_FLASH_ATTR
user_rf_pre_init(void) {

}

//*****************************************************************TASKS************************************************************

LOCAL void ICACHE_FLASH_ATTR
discon(os_event_t *e) {

#ifdef DEBUG
		os_printf( " |tcp discon cb\r\n| ");
#endif

	if ( NULL != ( struct espconn * )( e->par ) ) {

		espconn_disconnect( ( struct espconn * )( e->par ) );
	}

}


LOCAL void ICACHE_FLASH_ATTR
cmdPars(os_event_t *e) {

	os_printf(" cmdPars (char *)(e->sig)  %d,  (unsigned short)(e->par)  %d", (char *)(e->sig), (unsigned short)(e->par) );

#ifdef DEBUG
		{
			int i;
			for ( i = 0; i < (uint16_t)(e->par); i++) {

				uart_tx_one_char( tmp[i] );
			}
		}
#endif

	comandParser();

	ipAdd = 0;
	pespconn = NULL;
}

//**********************************************************Callbacks***************************************************************

LOCAL void ICACHE_FLASH_ATTR
gpioCallback(){

	uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
	if (!GPIO_INPUT_GET(2)) {
		os_printf( "load default options" );
		system_phy_set_max_tpw(82);
		loadDefParam( );
//		spi_flash_erase_sector( 59 );
/*		ets_wdt_disable();
	   	system_soft_wdt_stop();*/
		system_restart();
		while (1) {

		}
	} else {

		os_printf( "some not supporting gpio interrupt" );
	}
}


LOCAL void ICACHE_FLASH_ATTR
tcp_recvcb( void *arg, char *pdata, unsigned short len ) { // data received

	struct espconn *conn = (struct espconn *) arg;
	uint8_t *str;

#ifdef DEBUG
	os_printf( " |tcp_recvcb remote ip Connected : %d.%d.%d.%d \r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
	os_printf( " |tcp_recvcb local  ip Connected : %d.%d.%d.%d \r\n| ",  IP2STR( conn->proto.tcp->local_ip  ) );
#endif
	if ( TCP_FREE == tcpSt ) {

		pespconn = conn;
		ipAdd = *(uint32 *)( pespconn->proto.tcp->remote_ip );
		tcpSt = TCP_BUSY;
		os_printf(" tcp_recvcb *pdata  %d,  len  %d", pdata, len );
		memcpy( tmp, pdata, len );
		system_os_post( CMD_PRS_QUEUE_PRIO, (ETSSignal)pdata, (ETSParam)len );
	}
/*   if ( NULL != conn && NULL != pespconn ) {
   if ( '\r' == pdata[ len - 2 ] && (uint32)( conn->proto.tcp->remote_ip[0] ) == (uint32)( pespconn->proto.tcp->remote_ip[0]) ) {

    	if ( TMP_SIZE >= counterForTmp + len ) {

    		memcpy( &tmp[counterForTmp], pdata, len );
#ifdef DEBUG
    	{
    		int i;

			for (i = 0; i < len; i++) {
		  			   uart_tx_one_char(tmp[i]);
		 	 }
			uart_tx_one_char(tmp[i]);
    	}
#endif
    		counterForTmp = 0;
    		if ( TCP_FREE == comandPars ) {

    			comandPars = TCP_BUSY;
//    			system_os_post(1, '9', '9');
    		}

    	} else {

    		memcpy( tmp, TCP_NOT_ENOUGH_MEMORY, ( sizeof(TCP_NOT_ENOUGH_MEMORY) ) );
    		tmp[ sizeof(TCP_NOT_ENOUGH_MEMORY) ] = '\r';
    		tmp[ sizeof(TCP_NOT_ENOUGH_MEMORY) + 1 ] = '\n';
    		tcpSt = TCP_FREE;
    		comandPars = TCP_FREE;
    		espconn_send( pespconn, tmp, ( sizeof( TCP_NOT_ENOUGH_MEMORY ) + 2 ) );
    	}

    } else if ( (uint32)( conn->proto.tcp->remote_ip[0] ) == (uint32)( pespconn->proto.tcp->remote_ip[0]) ) {

    	if ( TMP_SIZE >= counterForTmp + len ) {

    		memcpy( &tmp[counterForTmp], pdata, len );
    		counterForTmp += len;

    	} else {

    		memcpy( tmp, TCP_NOT_ENOUGH_MEMORY, ( sizeof(TCP_NOT_ENOUGH_MEMORY) ) );
    		tmp[ sizeof(TCP_NOT_ENOUGH_MEMORY) ] = '\r';
    		tmp[ sizeof(TCP_NOT_ENOUGH_MEMORY) + 1 ] = '\n';
    		tcpSt = TCP_FREE;
    		comandPars = TCP_FREE;
    		espconn_send( pespconn, tmp, ( sizeof( TCP_NOT_ENOUGH_MEMORY ) + 2 ) );
    	}
    }
   }*/
}


// espconn_sent(arg, tmp, strlen(tmp));
LOCAL void ICACHE_FLASH_ATTR
tcp_connectcb( void *arg ) { // TCP connected successfully

	struct espconn *conn = (struct espconn *) arg;

	/*if ( TCP_FREE == tcpSt ) {

		ipAdd = *(uint32 *)( pespconn->proto.tcp->remote_ip );
		tcpSt = TCP_BUSY;*/
#ifdef DEBUG
		os_printf( " |tcp_connectcb ip : %d.%d.%d.%d Connected\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
#endif

	/*} else if ( TCP_BUSY == tcpSt && *(uint32 *)( conn->proto.tcp->remote_ip ) == ipAdd ) {

		uint8_t erB[] = { 'B', 'U', 'S', 'Y', '\0', '\r', '\n' };
		marker = mSET;
		espconn_send( conn, erB, sizeof(erB) );
#ifdef DEBUG
		os_printf( " |tcp_connectcb ip : %d.%d.%d.%d Connect busy...\r\n| ",  IP2STR( ( (struct espconn *) arg)->proto.tcp->remote_ip ) );
#endif
	} else*/ if ( TCP_BUSY == tcpSt && *(uint32 *)( conn->proto.tcp->remote_ip ) != ipAdd && 0 != ipAdd ) {

#ifdef DEBUG
		os_printf( " |tcp_connectcb ip: busy..\r\n| " );
#endif

		system_os_post( DISCON_QUEUE_PRIO, DISCON_ETS_SIGNAL_TOKEN, (ETSParam)conn );

//		uint8_t erB[] = { 'B', 'U', 'S', 'Y', '\0', '\r', '\n' };

//		espconn_send( conn, erB, sizeof(erB) );
#ifdef DEBUG
		os_printf( " |tcp_connectcb ip : %d.%d.%d.%d Connect busy...\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
#endif
	}

}


LOCAL void ICACHE_FLASH_ATTR
tcp_disnconcb( void *arg ) { // TCP disconnected successfully

	struct espconn *conn = (struct espconn *) arg;

#ifdef DEBUG
	os_printf( " |tcp_disnconcb TCP disconnected successfully : %d.%d.%d.%d\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
#endif

/*	if ( NULL != pespconn ) {

		if ( *(uint32 *)( conn->proto.tcp->remote_ip ) == ipAdd && marker == mCLEAR ) {
			pespconn = NULL;
			tcpSt = TCP_FREE;
#ifdef DEBUG
	os_printf( " |tcp_disnconcb tcp free| " );
#endif
		} else if ( *(uint32 *)( conn->proto.tcp->remote_ip ) == ipAdd && marker == mSET ) {
#ifdef DEBUG
	os_printf( " |tcp_disnconcb double connect| " );
#endif
			marker = mCLEAR;
		}
	}*/
}


LOCAL void ICACHE_FLASH_ATTR
tcp_reconcb( void *arg, sint8 err ) { // error, or TCP disconnected

	struct espconn *conn = (struct espconn *) arg;

#ifdef DEBUG
	os_printf( " |tcp_reconcb TCP RECON : %d.%d.%d.%d\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
#endif


/*	if ( NULL != pespconn ) {

		if ( *(uint32 *)( conn->proto.tcp->remote_ip ) == ipAdd && marker == mCLEAR ) {
			pespconn = NULL;
			tcpSt = TCP_FREE;
#ifdef DEBUG
	os_printf( " |tcp_reconcb tcp free| " );
#endif
		} else if ( *(uint32 *)( conn->proto.tcp->remote_ip ) == ipAdd && marker == mSET ) {

#ifdef DEBUG
	os_printf( " |tcp_reconcb double connect| " );
#endif
			marker = mCLEAR;
		}
	}*/
}


LOCAL void ICACHE_FLASH_ATTR
tcp_sentcb( void *arg ) { // data sent callback

	struct espconn *conn = (struct espconn *) arg;

#ifdef DEBUG
	os_printf( " |tcp_sentcb DATA sent for ip : %d.%d.%d.%d\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
#endif
/*	if ( NULL != pespconn && NULL != conn ) {
		os_printf( " |tcp_sentcb check point| " );
		if ( (uint32)( conn->proto.tcp->remote_ip[0] ) == (uint32)( pespconn->proto.tcp->remote_ip[0]) && marker == mCLEAR ) {
			pespconn = NULL;
			tcpSt = TCP_FREE;
#ifdef DEBUG
	os_printf( " |tcp_sentcb tcp free| " );
#endif
		} else if ( (uint32)( conn->proto.tcp->remote_ip[0] ) == (uint32)( pespconn->proto.tcp->remote_ip[0]) && marker == mSET ) {
#ifdef DEBUG
	os_printf( " |tcp_sentcb double connect| " );
#endif
		}
	}*/

	system_os_post( DISCON_QUEUE_PRIO, DISCON_ETS_SIGNAL_TOKEN, (ETSParam)conn );

//		espconn_disconnect(conn);

}

//**********************************************************************************************************************************

LOCAL res ICACHE_FLASH_ATTR
writeFlash( uint16_t where, uint8_t *what ) {

	uint16_t i;

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
				                                     (uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE ) ) {

		return ERROR;
	}

	for ( i = where ; '\n' != writeFlashTmp[ i ]; i++ ) {

		writeFlashTmp[ i ] = 0xff;
	}

	writeFlashTmp[ i ] = 0xff;

	for (  i = 0; '\0' != what[ i ]; i++, where++ ) {

		writeFlashTmp[ where ] = what[ i ];
	}

	writeFlashTmp[ where++ ] = '\0';
	writeFlashTmp[ where ] = '\n';

	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM ) ) {

		return ERROR;
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_write( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                                                                       (uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE ) ) {

		return ERROR;
	}

	return DONE;
}


LOCAL void ICACHE_FLASH_ATTR
mScheduler(void) {

	os_timer_disarm(&task_timer);

/*	if ( DHCP_STARTED == wifi_softap_dhcps_status() ) {

		os_printf("wifi_softap_reset_dhcps_lease_time() %d", wifi_softap_reset_dhcps_lease_time());
		os_printf("wifi_softap_set_dhcps_lease_time(1) %d", wifi_softap_set_dhcps_lease_time(1));
	}*/


	if ( DHCP_STOPPED == wifi_station_dhcpc_status() ) {

		if ( true == wifi_station_dhcpc_start() ) {

			os_printf("dhcp client stoped");
			os_printf("dhcp client reseted");
		} else {

			os_printf("dhcp client not reseted");
		}
	}

	if ( ENABLE == gpioStatusOut1 ) {
#ifdef DEBUG
//		os_printf(" |mScheduler gpioStatusOut1 = ENABLE| ");
#endif
		if (  0 == strcmp( gpioModeOut1, GPIO_OUT_COMBINE_MODE ) ) {
#ifdef DEBUG
//		os_printf(" |mScheduler gpioModeOut1 = GPIO_OUT_COMBINE_MODE, gpioOutDeley1 = %dms, gpioOutDeley1Counter = %dms| " \
				                              , gpioOutDeley1, gpioOutDeley1Counter);
#endif
			if ( gpioOutDeley1Counter < gpioOutDeley1 && GPIO_INPUT_GET(INP_2_PIN) ) {
				gpioOutDeley1Counter += 15;
				GPIO_OUTPUT_SET(OUT_1_GPIO, 1);
			} else {
#ifdef DEBUG
//		os_printf(" |mScheduler GPIO_OUT_COMBINE_MODE gpioStatusOut1 = DISABLE| ");
#endif
				GPIO_OUTPUT_SET(OUT_1_GPIO, 0);
				gpioOutDeley1Counter = 0;
				gpioStatusOut1 = DISABLE;
			}
		} else if ( 0 == strcmp( gpioModeOut1, GPIO_OUT_TRIGGER_MODE) ) {
#ifdef DEBUG
//		os_printf(" |mScheduler gpioModeOut1 = GPIO_OUT_TRIGGER_MODE| ");
#endif
			GPIO_OUTPUT_SET(OUT_1_GPIO, 1);
			if ( !GPIO_INPUT_GET(INP_2_PIN) ) {
#ifdef DEBUG
//		os_printf(" |mScheduler GPIO_OUT_TRIGGER_MODE gpioStatusOut1 = DISABLE| ");
#endif
					GPIO_OUTPUT_SET(OUT_1_GPIO, 0);
					gpioOutDeley1Counter = 0;
					gpioStatusOut1 = DISABLE;
			}
		} else if ( 0 == strcmp( gpioModeOut1, GPIO_OUT_IMPULSE_MODE) ) {
#ifdef DEBUG
//		os_printf(" |mScheduler gpioModeOut1 = GPIO_OUT_IMPULSE_MODE, gpioOutDeley1 = %dms, gpioOutDeley1Counter = %dms| "\
				                                                       , gpioOutDeley1, gpioOutDeley1Counter);
#endif
			if ( gpioOutDeley1Counter < gpioOutDeley1 ) {
				gpioOutDeley1Counter += 15;
				GPIO_OUTPUT_SET(OUT_1_GPIO, 1);

			} else {
#ifdef DEBUG
//		os_printf(" |mScheduler GPIO_OUT_IMPULSE_MODE gpioStatusOut1 = DISABLE| ");
#endif
				GPIO_OUTPUT_SET(OUT_1_GPIO, 0);
				gpioOutDeley1Counter = 0;
				gpioStatusOut1 = DISABLE;
			}
		}
	}

	if ( ENABLE == gpioStatusOut2 ) {
#ifdef DEBUG
//		os_printf(" |mScheduler gpioStatusOut2 = ENABLE| ");
#endif
		if ( 0 == strcmp( gpioModeOut2, GPIO_OUT_COMBINE_MODE) ) {
#ifdef DEBUG
//		os_printf(" |mScheduler gpioModeOut2 = GPIO_OUT_COMBINE_MODE, gpioOutDeley2 = %dms, gpioOutDeley2Counter = %d| " \
				                                                      , gpioOutDeley2, gpioOutDeley2);
#endif
			if ( gpioOutDeley2Counter < gpioOutDeley2 && gpio16_input_get() ) {
				gpioOutDeley2Counter += 15;
				GPIO_OUTPUT_SET(OUT_2_GPIO, 1);
			} else {
				GPIO_OUTPUT_SET(OUT_2_GPIO, 0);
				gpioOutDeley2Counter = 0;
				gpioStatusOut2 = DISABLE;
			}
		} else if ( 0 == strcmp( gpioModeOut2, GPIO_OUT_TRIGGER_MODE ) ) {
			GPIO_OUTPUT_SET(OUT_2_GPIO, 1);
#ifdef DEBUG
//		os_printf(" |mScheduler gpioModeOut2 = GPIO_OUT_TRIGGER_MODE| ");
#endif
			if ( !gpio16_input_get() ) {
#ifdef DEBUG
//		os_printf(" |mScheduler GPIO_OUT_TRIGGER_MODE gpioStatusOut2 = DISABLE| ");
#endif
				GPIO_OUTPUT_SET(OUT_2_GPIO, 0);
				gpioOutDeley2Counter = 0;
				gpioStatusOut2 = DISABLE;
			}

		} else if ( 0 == strcmp( gpioModeOut2, GPIO_OUT_IMPULSE_MODE) ) {
#ifdef DEBUG
//		os_printf(" |mScheduler gpioModeOut2 = GPIO_OUT_IMPULSE_MODE, gpioOutDeley2 = %dms, gpioOutDeley2Counter = %dms| "\
				                                                       , gpioOutDeley2, gpioOutDeley2Counter);
#endif
			if ( gpioOutDeley2Counter < gpioOutDeley2 ) {
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

	    struct station_info *station = wifi_softap_get_station_info();

		broadcastTmr = 0;

		broadcastBuilder();

		if ( DHCP_TIMEOUT <= resetDHCP ) {

			resetDHCP = 0;
			os_printf("wifi_station_dhcpc_start() %d", wifi_station_dhcpc_start());
		} else {

			resetDHCP++;
		}

		// Внутрення сеть
		while ( station ) {

#ifdef DEBUG
		os_printf( "bssid : %x:%x:%x:%x:%x:%x ip : %d.%d.%d.%d ", MAC2STR( station->bssid ), IP2STR( &station->ip ) );
#endif
    		espconnBroadcastAP.type = ESPCONN_UDP;
    		espconnBroadcastAP.state = ESPCONN_NONE;
    		espconnBroadcastAP.proto.udp = &espudpBroadcastAP;
		    espconnBroadcastAP.proto.udp->remote_port = StringToInt( &broadcastTmp[DEF_UDP_PORT_OFSET] );
		    IP4_ADDR( (ip_addr_t *)espconnBroadcastAP.proto.udp->remote_ip, (uint8_t)(station->ip.addr), (uint8_t)(station->ip.addr >> 8),\
		    															(uint8_t)(station->ip.addr >> 16), (uint8_t)(station->ip.addr >> 24) );
			espconn_create( &espconnBroadcastAP );
			switch ( espconn_send( &espconnBroadcastAP, brodcastMessage, strlen( brodcastMessage ) ) ) {
				case ESPCONN_OK:
					os_printf( " |esp soft AP  broadcast succeed| " );
					break;
				case ESPCONN_MEM:
					os_printf( " |esp soft AP  broadcast out of memory| " );
					system_restart();
					while (1) {

					}
					break;
				case ESPCONN_ARG:
					os_printf( " |esp soft AP  broadcast illegal argument| " );
					system_restart();
					while (1) {

					}
					break;
				case ESPCONN_IF:
					os_printf( " |esp soft AP  broadcast send UDP fail| " );
					system_restart();
					while (1) {

					}
					break;
				case ESPCONN_MAXNUM:
					os_printf( " |esp soft AP  broadcast buffer of sending data is full| " );
					system_restart();
					while (1) {

					}
					break;

			}
			espconn_delete(&espconnBroadcastAP);

			station = STAILQ_NEXT(station, next);
		}

		wifi_softap_free_station_info();
/*		{
			struct station_config stationConf;
			wifi_station_get_config( &stationConf );
			os_printf( " stationConf.ssid  %s ", stationConf.ssid, &stationConf.ssid );
			os_printf( " stationConf.password  %s ", stationConf.password, &stationConf.password );
	    	IP4_ADDR( (ip_addr_t *)espconnBroadcastSTA.proto.udp->remote_ip, (uint8_t)( ipaddr_addr( &broadcastTmp[ DEF_IP_SOFT_AP_OFSET ] ) ), \
				    								(uint8_t)( ipaddr_addr( &broadcastTmp[ DEF_IP_SOFT_AP_OFSET ] ) >> 8 ),\
													(uint8_t)( ipaddr_addr( &broadcastTmp[ DEF_IP_SOFT_AP_OFSET ] ) >> 16 ), 255 );
			espconnBroadcastSTA.proto.udp->remote_port = StringToInt( &broadcastTmp[DEF_UDP_PORT_OFSET] );
			espconn_create(&espconnBroadcastSTA);
			switch ( espconn_send(&espconnBroadcastSTA, brodcastMessage, strlen(brodcastMessage)) ) {
				case ESPCONN_OK:
					os_printf( " |esp soft AP  broadcast succeed| " );
					break;
				case ESPCONN_MEM:
					os_printf( " |esp soft AP  broadcast out of memory| " );
					break;
				case ESPCONN_ARG:
					os_printf( " |esp soft AP  broadcast illegal argument| " );
					break;
				case ESPCONN_IF:
					os_printf( " |esp soft AP  broadcast send UDP fail| " );
					break;
				case ESPCONN_MAXNUM:
					os_printf( " |esp soft AP  broadcast buffer of sending data is full| " );
					break;

			}
			espconn_delete(&espconnBroadcastSTA);
		}*/
		// Внешняя сеть
		switch( wifi_station_get_connect_status() ) {
			case STATION_GOT_IP:
				if ( ( rssi = wifi_station_get_rssi() ) < -90 ) {

					os_printf( "Bad signal, rssi = %d ", rssi );
					os_delay_us(1000);
					os_printf( "Broadcast port %s ", &broadcastTmp[DEF_UDP_PORT_OFSET] );
					os_printf( "%s ", broadcastShift );
					GPIO_OUTPUT_SET( LED_GPIO, ledState );
					ledState ^= 1;
				} else {

					GPIO_OUTPUT_SET( LED_GPIO, 1 );

					wifi_get_ip_info( STATION_IF, &inf );

					if ( 0 != inf.ip.addr ) {

						ets_uart_printf( "WiFi connected\r\n " );
						os_delay_us( 1000 );
	//					os_printf( "Broadcast port %s ", &broadcastTmp[DEF_UDP_PORT_OFSET] );
						espconnBroadcastSTA.type = ESPCONN_UDP;
						espconnBroadcastSTA.state = ESPCONN_NONE;
						espconnBroadcastSTA.proto.udp = &espudpBroadcastSTA;
						espconnBroadcastSTA.proto.udp->remote_port = StringToInt( &broadcastTmp[DEF_UDP_PORT_OFSET] );
						IP4_ADDR((ip_addr_t *)espconnBroadcastSTA.proto.udp->remote_ip, (uint8_t)(inf.ip.addr), (uint8_t)(inf.ip.addr >> 8),\
																			(uint8_t)(inf.ip.addr >> 16), 255);
						os_printf( "%s ", brodcastMessage );
						espconn_create( &espconnBroadcastSTA );
						switch ( espconn_send( &espconnBroadcastSTA, brodcastMessage, strlen( brodcastMessage ) ) ) {
							case ESPCONN_OK:
								os_printf( " |esp STA  broadcast succeed| " );
								break;
							case ESPCONN_MEM:
								os_printf( " |esp STA  broadcast out of memory| " );
								system_restart();
								while (1) {

								}
								break;
							case ESPCONN_ARG:
								os_printf( " |esp STA  broadcast illegal argument| " );
								system_restart();
								while (1) {

								}
								break;
							case ESPCONN_IF:
								os_printf( " |esp STA  broadcast send UDP fail| " );
								system_restart();
								while (1) {

								}
								break;
							case ESPCONN_MAXNUM:
								os_printf( " |esp STA  broadcast buffer of sending data is full| " );
								system_restart();
								while (1) {

								}
								break;

						}

						espconn_delete( &espconnBroadcastSTA );
					}
				}
				break;
			case STATION_WRONG_PASSWORD:
				ets_uart_printf( "WiFi connecting error, wrong password\r\n" );
				os_delay_us(1000);
		    	os_printf( "routerSSID %s\r\n", routerSSID );
		    	os_printf( "routerPWD %s\r\n", routerPWD );
	//	    	os_printf( "Broadcast port %s\r\n", &broadcastTmp[DEF_UDP_PORT_OFSET] );
				os_printf( "%s\r\n", broadcastShift );
				GPIO_OUTPUT_SET( LED_GPIO, ledState );
				ledState ^=1;
				break;
			case STATION_NO_AP_FOUND:
				ets_uart_printf("WiFi connecting error, ap not found\r\n" );
				os_delay_us(1000);
		    	os_printf( "routerSSID %s\r\n", routerSSID );
		    	os_printf( "routerPWD %s\r\n", routerPWD );
	//	    	os_printf( "Broadcast port %s\r\n", &broadcastTmp[DEF_UDP_PORT_OFSET] );
				os_printf( "%s\r\n", broadcastShift );
				GPIO_OUTPUT_SET( LED_GPIO, ledState );
				ledState ^=1;
				break;
			case STATION_CONNECT_FAIL:
				ets_uart_printf( "WiFi connecting fail\r\n" );
				os_delay_us(1000);
		    	os_printf( "routerSSID %s\r\n", routerSSID );
		    	os_printf( "routerPWD %s\r\n", routerPWD );
	//	    	os_printf( "Broadcast port %s\r\n", &broadcastTmp[DEF_UDP_PORT_OFSET] );
				os_printf( "%s\r\n", broadcastShift );
				system_restart();
				GPIO_OUTPUT_SET( LED_GPIO, ledState );
				ledState ^=1;
				break;
			default:
				ets_uart_printf( "WiFi connecting...\r\n " );
				os_delay_us(1000);
		    	os_printf( "routerSSID %s\r\n", routerSSID );
		    	os_printf( "routerPWD %s\r\n", routerPWD );
		 //   	os_printf( "Broadcast port %s\r\n", &broadcastTmp[DEF_UDP_PORT_OFSET] );
				os_printf( "%s\r\n", broadcastShift );
				GPIO_OUTPUT_SET( LED_GPIO, ledState );
				ledState ^=1;
				break;
		}
	} else {

		broadcastTmr += 10;
	}

	os_timer_setfn( &task_timer, (os_timer_func_t *)mScheduler, (void *)0 );
	os_timer_arm( &task_timer, DELAY, 0 );
}


LOCAL void ICACHE_FLASH_ATTR
init_done(void) {

   	ets_wdt_init();
    ets_wdt_disable();
   	ets_wdt_enable();
   	system_soft_wdt_stop();
    system_soft_wdt_restart();

    initWIFI(); // настройка sta ap

	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
	//gpio_intr_handler_register(callbackFunction, (void* )INP_4_PIN); // GPIO interrupt handler
	ETS_GPIO_INTR_ATTACH( gpioCallback, NULL );
	//GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(INP_2_PIN)); // Clear GPIO status
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, GPIO_REG_READ(GPIO_STATUS_ADDRESS) );
	gpio_pin_intr_state_set( GPIO_ID_PIN( 2 ), 4 ); // Interrupt on any GPIO edge
	ETS_GPIO_INTR_ENABLE();

	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm( &task_timer );
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn( &task_timer, (os_timer_func_t *)mScheduler, (void *)0 );
	 //void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm( &task_timer, DELAY, 0 );
}


void ICACHE_FLASH_ATTR
user_init(void) {

	ets_wdt_disable();
   	system_soft_wdt_stop();

	initPeriph();

	os_printf( " OS reset status: %d\r\n", system_get_rst_info()->reason );
	if ( system_get_rst_info()->reason != REASON_SOFT_RESTART \
			 && system_get_rst_info()->reason != REASON_DEFAULT_RST ) {

//		system_restart();
	}

    os_printf( " sdk version: %s \r\n", system_get_sdk_version() );
	os_printf( " module version 0.8\r\n" );

	//spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM );

	//первая загрузка
	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                                                                  (uint32 *)writeFlashTmp, ALIGN_FLASH_READY_SIZE ) ) {

		ets_uart_printf( " | user_init: Read USER_SECTOR_IN_FLASH_MEM fail! |\r\n" );
		system_restart();
		while(1);
	}

	if ( 0 != strcmp( writeFlashTmp, FLASH_READY ) ) {

		if ( OPERATION_OK != clearSectorsDB() ) {

			ets_uart_printf( " | user_init: clearSectorsDB() fail! |\r\n" );
			system_restart();
			while(1);
		}

		loadDefParam();
	}

	if ( 0 == GPIO_INPUT_GET( INP_3_PIN ) )  {

		uint16_t c, currentSector;
		uint32_t i;

		for ( currentSector = START_SECTOR; currentSector <= END_SECTOR; currentSector++ ) {
			os_printf( "currentSector: %d\r\n", currentSector );
			spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE );
			for ( i = 0; SPI_FLASH_SEC_SIZE > i; i++ ) {

				uart_tx_one_char( writeFlashTmp[ i ] );
			}

			system_soft_wdt_stop();
		}
	}

	// broadcast
	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                                                                          (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {

		ets_uart_printf(" | user_init: read Broadcast fail! |\r\n");
		system_restart();
		while(1);
	}

	if ( 0 == strcmp( &broadcastTmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_IMPULSE_MODE ) || \
				                                             0 == strcmp( &broadcastTmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) || \
															 0 == strcmp( &broadcastTmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_COMBINE_MODE ) ) {

		gpioOutDeley1 = StringToInt( &broadcastTmp[GPIO_OUT_1_DELEY_OFSET] );

		os_sprintf( gpioModeOut1, "%s", &broadcastTmp[ GPIO_OUT_1_MODE_OFSET ] );

#ifdef DEBUG
	  os_printf( " gpioModeOut1  %s,  gpioOutDeley1  %d\r\n", gpioModeOut1, gpioOutDeley1 );
	  os_delay_us(50000);
#endif

	}

	if ( 0 == strcmp( &broadcastTmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_IMPULSE_MODE ) || \
            												0 == strcmp( &broadcastTmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) || \
															0 == strcmp( &broadcastTmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_COMBINE_MODE ) ) {

		gpioOutDeley2 = StringToInt( &broadcastTmp[GPIO_OUT_2_DELEY_OFSET] );

		os_sprintf( gpioModeOut2, "%s", &broadcastTmp[ GPIO_OUT_2_MODE_OFSET ] );

#ifdef DEBUG
	  os_printf( " gpioModeOut2  %s,  gpioOutDeley2  %d\r\n", gpioModeOut2, gpioOutDeley2 );
	  os_delay_us(50000);
#endif

	}

#ifdef DEBUG
	{

	uint16_t c, currentSector;

	for ( currentSector = USER_SECTOR_IN_FLASH_MEM; currentSector <= USER_SECTOR_IN_FLASH_MEM; currentSector++ ) {
			os_printf( " currentSector %d\r\n", currentSector);
			spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE );
			for ( c = 0; SPI_FLASH_SEC_SIZE > c; c++ ) {

				uart_tx_one_char(writeFlashTmp[c]);
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


		espconn_regist_recvcb(&espconnServer, tcp_recvcb);          // data received
		espconn_regist_connectcb(&espconnServer, tcp_connectcb);    // TCP connected successfully
		espconn_regist_disconcb(&espconnServer, tcp_disnconcb);     // TCP disconnected successfully
		espconn_regist_sentcb(&espconnServer, tcp_sentcb);          // data sent
		espconn_regist_reconcb(&espconnServer, tcp_reconcb);        // error, or TCP disconnected
		espconn_accept(&espconnServer);
		espconn_regist_time(&espconnServer, TCP_SERVER_TIMEOUT, 0);
//		espconn_tcp_set_max_con(255);

#ifdef DEBUG
    	os_printf( " espconn_tcp_get_max_con() %d\r\n", espconn_tcp_get_max_con() );
#endif

   	if ( 5 != espconn_tcp_get_max_con() ) {

    		espconn_tcp_set_max_con(5);
    	}
  /*  	if ( 0 == espconn_tcp_set_max_con_allow( &espconnServer, 2 ) ) {

    		os_printf( "espconn_tcp_set_max_con_allow( espconnServer, 2 ) fail " );
    	} else {

    		os_printf( "espconn_tcp_get_max_con_allow( espconnServer, 2 ) %d ", espconn_tcp_get_max_con_allow(&espconnServer) );
    	}*/

	}


  /*  { //udp клиент Sta

    	espconnBroadcastAP.type = ESPCONN_UDP;
    	espconnBroadcastAP.state = ESPCONN_NONE;
    	espconnBroadcastAP.proto.udp = &espudpBroadcastAP;
    	espconnBroadcastAP.proto.udp->remote_port = StringToInt( &writeFlashTmp[DEF_UDP_PORT_OFSET] );
#ifdef DEBUG
    	os_printf( " broadcast.proto.udp->remote_port %d\r\n", espconnBroadcastAP.proto.udp->remote_port );
#endif

     //udp клиент AP

   	espconnBroadcastSTA.type = ESPCONN_UDP;
    	espconnBroadcastSTA.state = ESPCONN_NONE;
    	espconnBroadcastSTA.proto.udp = &espudpBroadcastSTA;
    	espconnBroadcastSTA.proto.udp->remote_port = StringToInt( &writeFlashTmp[DEF_UDP_PORT_OFSET] );
    	IP4_ADDR( (ip_addr_t *)espconnBroadcastSTA.proto.udp->remote_ip, (uint8_t)( ipaddr_addr( &writeFlashTmp[ DEF_IP_SOFT_AP_OFSET ] ) ), \
    												(uint8_t)( ipaddr_addr( &writeFlashTmp[ DEF_IP_SOFT_AP_OFSET ] ) >> 8 ),\
													(uint8_t)( ipaddr_addr( &writeFlashTmp[ DEF_IP_SOFT_AP_OFSET ] ) >> 16 ), 255 );
#ifdef DEBUG
    	os_printf( " brodcastSTA.proto.udp->remote_ip %d.%d.%d.%d\r\n", IP2STR(espconnBroadcastSTA.proto.udp->remote_ip) );
#endif
    }*/

	if( wifi_station_get_auto_connect() == 0 ) {

			wifi_station_set_auto_connect(1);
	}

	if ( STATIONAP_MODE == wifi_get_opmode() ) {

		wifi_station_set_reconnect_policy(true);
	}

    memcpy( routerSSID, &broadcastTmp[SSID_STA_OFSET], strlen(&broadcastTmp[SSID_STA_OFSET]) + 1 );
    memcpy( routerPWD, &broadcastTmp[PWD_STA_OFSET], strlen(&broadcastTmp[PWD_STA_OFSET]) + 1 );

#ifdef DEBUG
    	os_printf( " routerSSID %s\r\n", routerSSID );
    	os_printf( " routerPWD %s\r\n", routerPWD );
#endif

    disconQueue = (os_event_t *)os_malloc( sizeof(os_event_t)*DISCON_QUEUE_LENGHT );
    cmdPrsQueue = (os_event_t *)os_malloc( sizeof(os_event_t)*CMD_PRS_QUEUE_LENGHT );

    system_os_task( discon, DISCON_QUEUE_PRIO, disconQueue, DISCON_QUEUE_LENGHT );
    system_os_task( cmdPars, CMD_PRS_QUEUE_PRIO, cmdPrsQueue, CMD_PRS_QUEUE_PRIO );

    system_init_done_cb(init_done);

	// os_timer_disarm(ETSTimer *ptimer)
    //os_timer_disarm(&task_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	//os_timer_setfn(&task_timer, (os_timer_func_t *)config, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	//os_timer_arm(&task_timer, DELAY, 0);
}


LOCAL void ICACHE_FLASH_ATTR
initPeriph( ) {

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

	gpio16_input_conf();

}


LOCAL void ICACHE_FLASH_ATTR
initWIFI( ) {

	struct station_config *stationConf = (struct station_config *)os_zalloc(sizeof(struct station_config));
	struct softap_config *softapConf = (struct softap_config *)os_zalloc(sizeof(struct softap_config));
	struct ip_info ipinfo;


	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
																		(uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE ) ) {

		ets_uart_printf( " | initWIFI Read fail! |\r\n" );
		system_restart();
		while(1);
	}

/* 	if ( 0 != strcmp( &tmp[BROADCAST_NAME_OFSET],  wifi_station_get_hostname( ) ) ) {

		wifi_station_set_hostname( &tmp[BROADCAST_NAME_OFSET] );
	}
*/
	if ( STATIONAP_MODE != wifi_get_opmode() ) {

		wifi_set_opmode( STATIONAP_MODE );
	}

	wifi_station_disconnect();
	wifi_station_dhcpc_stop();
	wifi_softap_dhcps_stop();

	os_sprintf( stationConf->ssid, "%s", &writeFlashTmp[ SSID_STA_OFSET ] );
#ifdef DEBUG
	  os_printf( " stationConf->ssid  %s,  &tmp[ SSID_STA_OFSET ]  %s\r\n", stationConf->ssid, &writeFlashTmp[ SSID_STA_OFSET ] );
#endif

	os_sprintf( stationConf->password, "%s", &writeFlashTmp[ PWD_STA_OFSET ] );
#ifdef DEBUG
	  os_printf( " stationConf->password  %s,  &tmp[ PWD_STA_OFSET ]  %s\r\n", stationConf->password, &writeFlashTmp[ PWD_STA_OFSET ] );
#endif
/*	  if ( 0 !=  stationConf.bssid_set ) {

		  stationConf.bssid_set = 0;
	  }
*/
	  if ( !wifi_station_set_config( stationConf ) ) {

	  	ets_uart_printf(" | initWIFI: module not set station config! |\r\n");
	 }

	os_free( stationConf );

	wifi_station_connect();
	wifi_station_dhcpc_start();


	softapConf->ssid_len = os_sprintf( softapConf->ssid, "%s", &writeFlashTmp[ SSID_AP_OFSET ] );
#ifdef DEBUG
		os_printf( " softapConf->ssid  %s,  &tmp[ SSID_AP_OFSET ]  %s, softapConf->ssid_len  %d\r\n", \
														softapConf->ssid, &writeFlashTmp[ SSID_AP_OFSET ], softapConf->ssid_len );
#endif

	os_sprintf( softapConf->password, "%s", &writeFlashTmp[ PWD_AP_OFSET ] );
#ifdef DEBUG
		os_printf( " softapConf->password  %s,  &tmp[ PWD_AP_OFSET ]  %s\r\n", softapConf->password, &writeFlashTmp[ PWD_AP_OFSET ] );
#endif

	softapConf->channel = DEF_CHANEL;
#ifdef DEBUG
		os_printf( " softapConf->channel  %d\r\n", softapConf->channel );
#endif

	softapConf->authmode = DEF_AUTH;
#ifdef DEBUG
		os_printf( " softapConf->authmode  %d\r\n", softapConf->authmode );
#endif

	softapConf->max_connection = MAX_CON;
#ifdef DEBUG
		os_printf( " softapConf->max_connection  %d\r\n", softapConf->max_connection );
#endif

	softapConf->ssid_hidden = NO_HIDDEN;
#ifdef DEBUG
		os_printf( " softapConf->ssid_hidden  %d\r\n", softapConf->ssid_hidden );
#endif

	softapConf->beacon_interval = BEACON_INT;
#ifdef DEBUG
		os_printf( " softapConf->beacon_interval  %d\r\n", softapConf->beacon_interval );
#endif

	if( !wifi_softap_set_config( softapConf ) ) {

		ets_uart_printf(" | initWIFI: module not set AP config! |\r\n");
	}

	os_free( softapConf );



	if ( wifi_get_ip_info(SOFTAP_IF, &ipinfo ) ) {

		ipinfo.ip.addr = ipaddr_addr( &writeFlashTmp[ DEF_IP_SOFT_AP_OFSET ] );
		ipinfo.gw.addr = ipinfo.ip.addr;  //шлюз
		IP4_ADDR( &ipinfo.netmask, 255, 255, 255, 0 );

		wifi_set_ip_info( SOFTAP_IF, &ipinfo );

	} else {

		ets_uart_printf( " | initWIFI: read ip ap fail! |\r\n" );
	}
#ifdef DEBUG
		os_printf( " ipinfo.ip.addr  %d ", ipinfo.ip.addr );
#endif

		wifi_softap_dhcps_start();



	if( wifi_get_phy_mode() != PHY_MODE_11N ) {

		wifi_set_phy_mode( PHY_MODE_11N );
	}
}


LOCAL void ICACHE_FLASH_ATTR
loadDefParam( void ) {

	uint16_t i;

	for ( i = 0; i < SPI_FLASH_SEC_SIZE; i++ ) {
		writeFlashTmp[i] = 0xff;
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM )  ) {

		ets_uart_printf(" | loadDefParam: Erase USER_SECTOR_IN_FLASH_MEM fail! |\r\n");
		while(1);
	}

	memcpy( &writeFlashTmp[FLASH_READY_OFSET], FLASH_READY, sizeof(FLASH_READY) );
	writeFlashTmp[ sizeof(FLASH_READY) ] = '\n';


	memcpy( &writeFlashTmp[HEADER_STA_OFSET], HEADER_STA, sizeof(HEADER_STA) );
	writeFlashTmp[ sizeof(HEADER_STA) + HEADER_STA_OFSET ] = '\n';

	memcpy( &writeFlashTmp[SSID_STA_OFSET], DEF_SSID_STA, sizeof(DEF_SSID_STA) );
	writeFlashTmp[ sizeof(DEF_SSID_STA) + SSID_STA_OFSET ] = '\n';

	memcpy( &writeFlashTmp[PWD_STA_OFSET], DEF_PWD_STA, sizeof(DEF_PWD_STA) );
	writeFlashTmp[ sizeof(DEF_PWD_STA) + PWD_STA_OFSET ] = '\n';


	memcpy( &writeFlashTmp[SSID_AP_OFSET], DEF_SSID_AP, sizeof(DEF_SSID_AP) );
	writeFlashTmp[ sizeof(DEF_SSID_AP) + SSID_AP_OFSET ] = '\n';

	memcpy( &writeFlashTmp[PWD_AP_OFSET], DEF_PWD_AP, sizeof(DEF_PWD_AP) );
	writeFlashTmp[ sizeof(DEF_PWD_AP) + PWD_AP_OFSET ] = '\n';

	memcpy( &writeFlashTmp[HEADER_AP_OFSET], HEADER_AP, sizeof(HEADER_AP) );
	writeFlashTmp[ sizeof(HEADER_AP) + HEADER_AP_OFSET ] = '\n';


	memcpy( &writeFlashTmp[GPIO_OUT_1_HEADER_OFSET], GPIO_OUT_1_HEADER, sizeof(GPIO_OUT_1_HEADER) );
	writeFlashTmp[ sizeof(GPIO_OUT_1_HEADER) + GPIO_OUT_1_HEADER_OFSET ] = '\n';

	memcpy( &writeFlashTmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_IMPULSE_MODE, sizeof(GPIO_OUT_IMPULSE_MODE) );
	writeFlashTmp[ sizeof(GPIO_OUT_IMPULSE_MODE) + GPIO_OUT_1_MODE_OFSET ] = '\n';

	memcpy( &writeFlashTmp[GPIO_OUT_1_DELEY_OFSET], DEF_GPIO_OUT_DELEY, sizeof(DEF_GPIO_OUT_DELEY) );
	writeFlashTmp[ sizeof(DEF_GPIO_OUT_DELEY) + GPIO_OUT_1_DELEY_OFSET ] = '\n';


	memcpy( &writeFlashTmp[GPIO_OUT_2_HEADER_OFSET], GPIO_OUT_2_HEADER, sizeof(GPIO_OUT_2_HEADER) );
	writeFlashTmp[ sizeof(GPIO_OUT_2_HEADER) + GPIO_OUT_2_HEADER_OFSET ] = '\n';

	memcpy( &writeFlashTmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_IMPULSE_MODE, sizeof(GPIO_OUT_IMPULSE_MODE) );
	writeFlashTmp[ sizeof(GPIO_OUT_IMPULSE_MODE) + GPIO_OUT_2_MODE_OFSET ] = '\n';

	memcpy( &writeFlashTmp[GPIO_OUT_2_DELEY_OFSET], DEF_GPIO_OUT_DELEY, sizeof(DEF_GPIO_OUT_DELEY) );
	writeFlashTmp[ sizeof(DEF_GPIO_OUT_DELEY) + GPIO_OUT_2_DELEY_OFSET ] = '\n';


	memcpy( &writeFlashTmp[BROADCAST_NAME_HEADER_OFSET], BROADCAST_NAME_HEADER, sizeof(BROADCAST_NAME_HEADER) );
	writeFlashTmp[ sizeof(BROADCAST_NAME_HEADER) + BROADCAST_NAME_HEADER_OFSET ] = '\n';

	memcpy( &writeFlashTmp[BROADCAST_NAME_OFSET], BROADCAST_NAME, sizeof(BROADCAST_NAME) );
	writeFlashTmp[ sizeof(BROADCAST_NAME) + BROADCAST_NAME_OFSET ] = '\n';


	memcpy( &writeFlashTmp[DEF_IP_SOFT_AP_HEADER_OFSET], DEF_IP_SOFT_AP_HEADER, sizeof(DEF_IP_SOFT_AP_HEADER) );
	writeFlashTmp[ sizeof(DEF_IP_SOFT_AP_HEADER) + DEF_IP_SOFT_AP_HEADER_OFSET ] = '\n';

	memcpy( &writeFlashTmp[DEF_IP_SOFT_AP_OFSET], DEF_IP_SOFT_AP, sizeof(DEF_IP_SOFT_AP) );
	writeFlashTmp[ sizeof(DEF_IP_SOFT_AP) + DEF_IP_SOFT_AP_OFSET ] = '\n';


	memcpy( &writeFlashTmp[DEF_UDP_PORT_HEADER_OFSET], DEF_UDP_PORT_HEADER, sizeof(DEF_UDP_PORT_HEADER) );
	writeFlashTmp[ sizeof(DEF_UDP_PORT_HEADER) + DEF_UDP_PORT_HEADER_OFSET ] = '\n';

	memcpy( &writeFlashTmp[DEF_UDP_PORT_OFSET], DEF_UDP_PORT, sizeof(DEF_UDP_PORT) );
	writeFlashTmp[ sizeof(DEF_UDP_PORT) + DEF_UDP_PORT_OFSET ] = '\n';


	memcpy( &writeFlashTmp[MAX_TPW_HEADER_OFSET], MAX_TPW_HEADER, sizeof(MAX_TPW_HEADER) );
	writeFlashTmp[ sizeof(MAX_TPW_HEADER) + MAX_TPW_HEADER_OFSET ] = '\n';

	memcpy( &writeFlashTmp[MAX_TPW_VALUE_OFSET], MAX_TPW_DEFAULT_VALUE, sizeof(MAX_TPW_DEFAULT_VALUE) );
		writeFlashTmp[ sizeof(MAX_TPW_DEFAULT_VALUE) + MAX_TPW_VALUE_OFSET ] = '\n';

	if ( SPI_FLASH_RESULT_OK != spi_flash_write( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
																				(uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE ) ) {

		ets_uart_printf(" | loadDefParam: write default param fail! |\r\n");
		while(1);
	}

		system_restore();
}


// Перевод числа в последовательность ASCII
LOCAL uint8_t * ICACHE_FLASH_ATTR
ShortIntToString(uint32_t data, uint8_t *adressDestenation) {

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
LOCAL uint32_t ICACHE_FLASH_ATTR
StringToInt(uint8_t *data) {

	uint32_t returnedValue = 0;

	for (;*data >= '0' && *data <= '9'; data++) {

		returnedValue = 10 * returnedValue + (*data - '0');
	}

	return returnedValue;
}


// Перевод шестнадцетиричного числа в последовательность ASCII
LOCAL uint8_t * ICACHE_FLASH_ATTR
intToStringHEX(uint8_t data, uint8_t *adressDestenation) {

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
//          + " " + ipSTA: + " " + wifi_get_ip_info( ) +
//          + " " + ipAP: + " " + wifi_get_ip_info( ) +
//          + " " + server port: + " " servPort + " " +
//			+ " " + phy mode: + " " + wifi_get_phy_mode() +
//          + " " + rssi: + " " + wifi_station_get_rssi() +
//          + " " + ssidSTA: + " " + name\n +
//          + " " + pwdSTA: + " " + pwd\n +
//          + " " + ssidAP: + " " + name\n +
//          + " " + pwdAP: + " " + pwd\n +
//          + " " + maxTpw: + " " + value\n +
//          + " " + broadcastPort + " " + port\n +
//          + " " + outputs: +
//			+ " " + gpio_1: + " " + Trigger/Impulse/Combine + " " + delay+ms\n +
//		    + " " + gpio_2: + " " + Trigger/Impulse/Combine + " " + delay+ms\n +
//          + " " + inputs: +
//		    + " " + inp_1: + " " + high/low +
//          + " " + INP_2: + " " + high/low +
//          + " " + INP_3: + " " + high/low +
//			+ " " + INP_4: + " " + high/low + "\r\n" + "\0"
LOCAL void ICACHE_FLASH_ATTR
broadcastBuilder( void ) {

	uint8_t *count;
	uint8_t macadr[10];

	//выделяем бродкаст айпишку

	if (STATION_GOT_IP == wifi_station_get_connect_status() ) {

		wifi_get_ip_info( STATION_IF, &inf );
	} else {

		inf.ip.addr = 0;
	}

	count = brodcastMessage;

	memcpy( count, NAME, ( sizeof( NAME ) - 1 ) );
	count += sizeof( NAME ) - 1;

	os_sprintf( count, "%s", &broadcastTmp[ BROADCAST_NAME_OFSET ] );
	count += strlen( &broadcastTmp[ BROADCAST_NAME_OFSET ] );

//=================================================================================================
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
//=================================================================================================
    memcpy( count, IP, ( sizeof( IP ) - 1 ) );
    count += sizeof( IP ) - 1;

    count = ShortIntToString( (uint8_t)(inf.ip.addr), count );
    	*count++ = '.';
    count = ShortIntToString( (uint8_t)(inf.ip.addr >> 8), count );
    	*count++ = '.';
    count = ShortIntToString( (uint8_t)(inf.ip.addr >> 16) , count );
    	*count++ = '.';
    count = ShortIntToString( (uint8_t)(inf.ip.addr  >> 24), count);
//=================================================================================================
    memcpy( count, IP_AP, ( sizeof( IP_AP ) - 1 ) );
    count += sizeof( IP_AP ) - 1;

    os_sprintf( count, "%s", &broadcastTmp[ DEF_IP_SOFT_AP_OFSET ] );
    count += strlen( &broadcastTmp[ DEF_IP_SOFT_AP_OFSET ] );
//=================================================================================================
 /*   memcpy( count, SERVER_PORT, ( sizeof( SERVER_PORT ) - 1 ) );
    count += sizeof( SERVER_PORT ) - 1;

    count = ShortIntToString( TCP_PORT, count ); */
//=================================================================================================
/*    memcpy( count, PHY_MODE, ( sizeof( PHY_MODE ) - 1 ) );
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
    }*/
//=================================================================================================
    if ( ( rssi = wifi_station_get_rssi() ) < 0 ) {

    	memcpy( count, RSSI, ( sizeof( RSSI ) - 1 ) );
	    count += sizeof( RSSI ) - 1;

	    *count++ = '-';
	    count = ShortIntToString( (rssi * (-1)), count );
    }
//=================================================================================================
	memcpy( count, SSID_STA, ( sizeof( SSID_STA ) - 1 ) );
	count += sizeof( SSID_STA ) - 1;

	os_sprintf( count, "%s", &broadcastTmp[ SSID_STA_OFSET ] );
	count += strlen( &broadcastTmp[ SSID_STA_OFSET ] );
//=================================================================================================
	memcpy( count, PWD_STA, ( sizeof( PWD_STA ) - 1 ) );
	count += sizeof( PWD_STA ) - 1;

	os_sprintf( count, "%s", &broadcastTmp[ PWD_STA_OFSET ] );
	count += strlen( &broadcastTmp[ PWD_STA_OFSET ] );
//=================================================================================================
	memcpy( count, SSID_AP, ( sizeof( SSID_AP ) - 1 ) );
	count += sizeof( SSID_AP ) - 1;

	os_sprintf( count, "%s", &broadcastTmp[ SSID_AP_OFSET ] );
	count += strlen( &broadcastTmp[ SSID_AP_OFSET ] );
//=================================================================================================
	memcpy( count, PWD_AP, ( sizeof( PWD_AP ) - 1 ) );
	count += sizeof( PWD_AP ) - 1;

	os_sprintf( count, "%s", &broadcastTmp[ PWD_AP_OFSET ] );
	count += strlen( &broadcastTmp[ PWD_AP_OFSET ] );
//=================================================================================================
	memcpy( count, MAX_TPW, ( sizeof( MAX_TPW ) - 1 ) );
	count += sizeof( MAX_TPW ) - 1;

	os_sprintf( count, "%s", &broadcastTmp[ MAX_TPW_VALUE_OFSET ] );
	count += strlen( &broadcastTmp[ MAX_TPW_VALUE_OFSET ] );
//=================================================================================================
	memcpy( count, BOADCAST_PORT, ( sizeof( BOADCAST_PORT ) - 1 ) );
	count += sizeof( BOADCAST_PORT ) - 1;

	os_sprintf( count, "%s", &broadcastTmp[ DEF_UDP_PORT_OFSET ] );
	count += strlen( &broadcastTmp[ DEF_UDP_PORT_OFSET ] );
//=================================================================================================
	memcpy( count, OUTPUTS, ( sizeof( OUTPUTS ) - 1 ) );
	broadcastShift = count;
	count += sizeof( OUTPUTS ) - 1;
//=================================================================================================
	memcpy( count, GPIO_1, ( sizeof( GPIO_1 ) - 1 ) );
	count += sizeof( GPIO_1 ) - 1;

	if ( 0 == strcmp( &broadcastTmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_IMPULSE_MODE ) ) {

		memcpy( count, GPIO_OUT_IMPULSE_MODE, ( sizeof( GPIO_OUT_IMPULSE_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_IMPULSE_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &broadcastTmp[ GPIO_OUT_1_DELEY_OFSET ] );
		count += strlen( &broadcastTmp[ GPIO_OUT_1_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;

	} else if ( 0 == strcmp( &broadcastTmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) ) {

		memcpy( count, GPIO_OUT_TRIGGER_MODE, ( sizeof( GPIO_OUT_TRIGGER_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_TRIGGER_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &broadcastTmp[ GPIO_OUT_1_DELEY_OFSET ] );
		count += strlen( &broadcastTmp[ GPIO_OUT_1_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	} else if ( 0 == strcmp( &broadcastTmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_COMBINE_MODE ) ) {

		memcpy( count, GPIO_OUT_COMBINE_MODE, ( sizeof( GPIO_OUT_COMBINE_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_COMBINE_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &broadcastTmp[ GPIO_OUT_1_DELEY_OFSET ] );
		count += strlen( &broadcastTmp[ GPIO_OUT_1_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	}
//=================================================================================================
	memcpy( count, GPIO_2, ( sizeof( GPIO_2 ) - 1 ) );
	count += sizeof( GPIO_2 ) - 1;

	if ( 0 == strcmp( &broadcastTmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_IMPULSE_MODE ) ) {

		memcpy( count, GPIO_OUT_IMPULSE_MODE, ( sizeof( GPIO_OUT_IMPULSE_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_IMPULSE_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &broadcastTmp[ GPIO_OUT_2_DELEY_OFSET ] );
		count += strlen( &broadcastTmp[ GPIO_OUT_2_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	} else if ( 0 == strcmp( &broadcastTmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) ) {

		memcpy( count, GPIO_OUT_TRIGGER_MODE, ( sizeof( GPIO_OUT_TRIGGER_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_TRIGGER_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &broadcastTmp[ GPIO_OUT_2_DELEY_OFSET ] );
		count += strlen( &broadcastTmp[ GPIO_OUT_2_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	} else if ( 0 == strcmp( &broadcastTmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_COMBINE_MODE ) ) {

		memcpy( count, GPIO_OUT_COMBINE_MODE, ( sizeof( GPIO_OUT_COMBINE_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_COMBINE_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &broadcastTmp[ GPIO_OUT_2_DELEY_OFSET ] );
		count += strlen( &broadcastTmp[ GPIO_OUT_2_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	}
//=================================================================================================
		memcpy( count, INPUTS, ( sizeof( INPUTS ) - 1 ) );
		count += sizeof( INPUTS ) - 1;
//=================================================================================================
		memcpy( count, INPUT_1, ( sizeof( INPUT_1 ) - 1 ) );
		count += sizeof( INPUT_1 ) - 1;
		if ( 0 == GPIO_INPUT_GET(INP_1_PIN ) ) {

			memcpy( count, STATUS_LOW, ( sizeof( STATUS_LOW ) - 1 ) );
			count += sizeof( STATUS_LOW ) - 1;
		} else {
			memcpy( count, STATUS_HIGH, ( sizeof( STATUS_HIGH ) - 1 ) );
			count += sizeof( STATUS_HIGH ) - 1;
		}
//=================================================================================================
		memcpy( count, INPUT_2, ( sizeof( INPUT_2 ) - 1 ) );
		count += sizeof( INPUT_2 ) - 1;
		if ( 0 == GPIO_INPUT_GET(INP_2_PIN ) ) {

			memcpy( count, STATUS_LOW, ( sizeof( STATUS_LOW ) - 1 ) );
			count += sizeof( STATUS_LOW ) - 1;
		} else {
			memcpy( count, STATUS_HIGH, ( sizeof( STATUS_HIGH ) - 1 ) );
			count += sizeof( STATUS_HIGH ) - 1;
		}
//=================================================================================================
		memcpy( count, INPUT_3, ( sizeof( INPUT_3 ) - 1 ) );
		count += sizeof( INPUT_3 ) - 1;
		if ( 0 == GPIO_INPUT_GET(INP_3_PIN ) ) {

			memcpy( count, STATUS_LOW, ( sizeof( STATUS_LOW ) - 1 ) );
			count += sizeof( STATUS_LOW ) - 1;
		} else {
			memcpy( count, STATUS_HIGH, ( sizeof( STATUS_HIGH ) - 1 ) );
			count += sizeof( STATUS_HIGH ) - 1;
		}
//=================================================================================================
		memcpy( count, INPUT_4, ( sizeof( INPUT_4 ) - 1 ) );
		count += sizeof( INPUT_4 ) - 1;
		if ( 0 == gpio16_input_get() ) {

			memcpy( count, STATUS_LOW, ( sizeof( STATUS_LOW ) - 1 ) );
			count += sizeof( STATUS_LOW ) - 1;
		} else {
			memcpy( count, STATUS_HIGH, ( sizeof( STATUS_HIGH ) - 1 ) );
			count += sizeof( STATUS_HIGH ) - 1;
		}
//=================================================================================================

		*count++ = '\r';
		*count++ = '\n';
		*count = '\0';
 }


LOCAL void ICACHE_FLASH_ATTR
comandParser( void ) {

	int i = 0;

	if ( 0 == strcmp( tmp, TCP_REQUEST ) ) { 																	//+

		if ( 0 == strcmp( tmp, "system request" ) ) {

			gpioStatusOut1 = ENABLE;
			tcpRespounseBuilder( TCP_OPERATION_OK );
		} else {

			switch ( requestString( &tmp[ sizeof(TCP_REQUEST) + 1 ] ) ) {

				  case WRONG_LENGHT:
					  tcpRespounseBuilder( TCP_WRONG_LENGHT );
					  break;

				  case NOTHING_FOUND:
					  tcpRespounseBuilder( TCP_NOTHING_FOUND );
					  break;
				  case OPERATION_OK:
				    gpioStatusOut1 = ENABLE;
				    tcpRespounseBuilder( TCP_OPERATION_OK );
				    break;

				   case OPERATION_FAIL:
					   tcpRespounseBuilder( TCP_OPERATION_FAIL );
					   break;
				   default:

					   break;
			}
		}

	} else if ( 0 == strcmp( tmp, TCP_ENABLE_GPIO_1 ) ) {  														//+

	    		GPIO_OUTPUT_SET(OUT_1_GPIO, 1);
	        	tcpRespounseBuilder( TCP_OPERATION_OK );

	} else if ( 0 == strcmp( tmp, TCP_ENABLE_GPIO_2 ) ) {  														//+

	    		GPIO_OUTPUT_SET(OUT_2_GPIO, 1);
	        	tcpRespounseBuilder( TCP_OPERATION_OK );

	} else if ( 0 == strcmp( tmp, TCP_DISABLE_GPIO_1 ) ) {  													//+

	    		GPIO_OUTPUT_SET(OUT_1_GPIO, 0);
	        	tcpRespounseBuilder( TCP_OPERATION_OK );

	} else if ( 0 == strcmp( tmp, TCP_DISABLE_GPIO_2 ) ) {  													//+

	    		GPIO_OUTPUT_SET(OUT_2_GPIO, 0);
	        	tcpRespounseBuilder( TCP_OPERATION_OK );

	} else if ( 0 == strcmp( tmp, TCP_QUERY ) ) {                                               				//+

	    	addressQuery = StringToInt( &tmp[ sizeof( TCP_QUERY ) + 1 + sizeof( TCP_ADRESS ) + 1 ] );
#ifdef DEBUG
	    	os_printf("addressQuery %d ", addressQuery);
#endif
	    	switch ( query( storage, &lenghtQuery, &addressQuery ) ) {

	    	    case OPERATION_OK:
#ifdef DEBUG
	    	    	os_printf("addressQuery %d ", addressQuery);
#endif
	    	    	buildQueryResponse( TCP_OPERATION_OK );
	    	    	break;
	    	    case OPERATION_FAIL:
	    	    	tcpRespounseBuilder( TCP_OPERATION_FAIL );

	    	    	break;
	    	    case READ_DONE:
#ifdef DEBUG
	    	    	os_printf("addressQuery %d ", addressQuery);
#endif
	    	    	buildQueryResponse( TCP_READ_DONE );
	    	    	break;
	    		default:

	    			break;
	    	 }

	} else if ( 0 == strcmp( tmp, TCP_INSERT ) ) {																//+

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

	    } else if ( 0 == strcmp( tmp, TCP_DELETE ) ) { 															//+

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

	    } else if ( 0 == strcmp( tmp, TCP_UPDATE ) ) { 															//+

	    	for ( i = sizeof(TCP_UPDATE); '\0' != tmp[ i ]; i++ ) {

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

	    } else if ( 0 == strcmp( tmp, TCP_FIND ) ) { 															//+

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

	    } else if ( 0 == strcmp( tmp, TCP_RESET ) ) { 															//+

	    	tcpRespounseBuilder( TCP_OPERATION_OK );
	    	system_restart();

	    } else if ( 0 == strcmp( tmp, TCP_RESTORE ) ) { 														//+

	    	loadDefParam();
	    	tcpRespounseBuilder( TCP_OPERATION_OK );
			system_restart();
    	/*	while (1) {
    		    ets_wdt_disable();
    		   	system_soft_wdt_stop();
    		}*/

	    } else if ( 0 == strcmp( tmp, TCP_SET_UDP_PORT ) ) {													//+

	    	writeFlash( DEF_UDP_PORT_OFSET, &tmp[ sizeof(TCP_SET_UDP_PORT) + 1 ] );
	    	//espconnBroadcastAP.proto.udp->remote_port = StringToInt( &tmp[ sizeof(TCP_SET_UDP_PORT) + 1 ] );
	    	//espconnBroadcastSTA.proto.udp->remote_port = espconnBroadcastAP.proto.udp->remote_port;
	    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    			    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
	    	}
	    	tcpRespounseBuilder( TCP_OPERATION_OK );

	    } else if ( 0 == strcmp( tmp, TCP_SET_IP ) ) {															//+

	    	writeFlash( DEF_IP_SOFT_AP_OFSET, &tmp[ sizeof(TCP_SET_IP) + 1 ] );
	    	tcpRespounseBuilder( TCP_OPERATION_OK );
	    	system_restart();

	    } else if ( 0 == strcmp( tmp, TCP_CLEAR_DB ) ) {														//+

//	    	writeFlash( CLEAR_DB_STATUS_OFSET, CLEAR_DB_STATUS );
	    	clearSectorsDB();
	    	tcpRespounseBuilder( TCP_OPERATION_OK );
//			system_restart();

	    } else if ( 0 == strcmp( tmp, TCP_SSID_STA ) ) { 														//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_SSID_STA) + 1 ],  SSID_MAX_LENGHT, SSID_MIN_LENGHT ) ) {

	    		writeFlash( SSID_STA_OFSET, &tmp[ sizeof(TCP_SSID_STA) + 1 ] );

	    		os_sprintf( routerSSID, "%s", &tmp[ sizeof(TCP_SSID_STA) + 1 ] );
#ifdef DEBUG
	    os_printf( "routerSSID %s ", routerSSID );
#endif
		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
		}
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    		//system_restart();
	    		/*while (1) {
	   		    ets_wdt_disable();
	    		   	system_soft_wdt_stop();
	    		}*/
	//    		initWIFI();
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_PWD_STA ) ) {															//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_PWD_STA) + 1 ],  PWD_MAX_LENGHT, PWD_MIN_LENGHT ) ) {

	    		writeFlash( PWD_STA_OFSET, &tmp[ sizeof(TCP_PWD_STA) + 1 ] );

	    		os_sprintf( routerPWD, "%s", &tmp[ sizeof(TCP_PWD_STA) + 1 ] );
#ifdef DEBUG
	    		os_printf( "routerPWD %s ", routerPWD );
#endif
	    		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
	    		}
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    		//system_restart();
	    		/*while (1) {
	    		    ets_wdt_disable();
	    		   	system_soft_wdt_stop();
	    		}*/
	 //   		initWIFI();
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_SSID_AP ) ) {												   			//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_SSID_AP) + 1 ],  SSID_MAX_LENGHT, SSID_MIN_LENGHT ) ) {

	    		writeFlash( SSID_AP_OFSET, &tmp[ sizeof(TCP_SSID_AP) + 1 ] );

	    		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
	    		}
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	   // 		initWIFI();
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_PWD_AP ) ) {															//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_PWD_AP) + 1 ],  PWD_MAX_LENGHT, PWD_MIN_LENGHT ) ) {

	    		writeFlash( PWD_AP_OFSET, &tmp[ sizeof(TCP_PWD_AP) + 1 ] );

	    		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
	    		}
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	//    		initWIFI();
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_BROADCAST_NAME ) ) {													//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_BROADCAST_NAME) + 1 ],  BROADCAST_NAME_MAX_LENGHT, \
	    			                                                              BROADCAST_NAME_MIN_LENGHT ) ) {

	    		writeFlash( BROADCAST_NAME_OFSET, &tmp[ sizeof(TCP_BROADCAST_NAME) + 1 ] );
	    		if ( 0 != strcmp( &tmp[ sizeof(TCP_BROADCAST_NAME) + 1 ],  wifi_station_get_hostname( ) ) ) {

	    			//wifi_station_set_hostname( &tmp[ sizeof(TCP_BROADCAST_NAME) + 1 ] );
	    		}

	    		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
	    		}
	    		tcpRespounseBuilder( TCP_OPERATION_OK );

	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_MAX_TPW ) ) { //-------------------------------------------------------------------------
	    	uint32_t userTpw = StringToInt( &tmp[ sizeof(TCP_MAX_TPW) + 1 ] );

	    	if (userTpw > MAX_TPW_MAX_VALUE) {

	    		userTpw = MAX_TPW_MAX_VALUE;
	    	} else if (userTpw < MAX_TPW_MIN_VALUE) {

	    		userTpw = MAX_TPW_MIN_VALUE;
	    	}
#ifdef DEBUG
	   os_printf(" userTpw %d ",userTpw);
#endif
	    	system_phy_set_max_tpw(userTpw);

	    	writeFlash( MAX_TPW_VALUE_OFSET, &tmp[ sizeof(TCP_MAX_TPW) + 1 ] );

	    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
	    	}

	    	tcpRespounseBuilder( TCP_OPERATION_OK );

	    } else if ( 0 == strcmp( tmp, TCP_GPIO_MODE_1 ) ) {                                                     //+

	    	if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 ], GPIO_OUT_IMPULSE_MODE ) ) {

	    		writeFlash( GPIO_OUT_1_MODE_OFSET, GPIO_OUT_IMPULSE_MODE );
	    		memcpy(gpioModeOut1, GPIO_OUT_IMPULSE_MODE, sizeof(GPIO_OUT_IMPULSE_MODE) );
	    		gpioOutDeley1 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 + sizeof(GPIO_OUT_IMPULSE_MODE) + 1 ] );
#ifdef DEBUG
	   os_printf("gpioModeOut1 %s, gpioOutDeley1 %d ",gpioModeOut1, gpioOutDeley1);
#endif
	    		writeFlash( GPIO_OUT_1_DELEY_OFSET ,&tmp[ sizeof(TCP_GPIO_MODE_1) + 1 + sizeof(GPIO_OUT_IMPULSE_MODE) + 1 ] );
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
		    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
		    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
		    	}

	    	} else if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 ], GPIO_OUT_TRIGGER_MODE ) ) {

	    		writeFlash( GPIO_OUT_1_MODE_OFSET, GPIO_OUT_TRIGGER_MODE );
	    		memcpy(gpioModeOut1, GPIO_OUT_TRIGGER_MODE, sizeof(GPIO_OUT_TRIGGER_MODE) );
	    		gpioOutDeley1 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 + sizeof(GPIO_OUT_TRIGGER_MODE) + 1 ] );
#ifdef DEBUG
	   os_printf("gpioModeOut1 %s, gpioOutDeley1 %d ",gpioModeOut1, gpioOutDeley1);
#endif
	    		writeFlash( GPIO_OUT_1_DELEY_OFSET ,&tmp[ sizeof(TCP_GPIO_MODE_1) + 1 + sizeof(GPIO_OUT_TRIGGER_MODE) + 1 ] );
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
		    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
		    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
		    	}
	    	} else if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 ], GPIO_OUT_COMBINE_MODE ) ) {

		    		writeFlash( GPIO_OUT_1_MODE_OFSET, GPIO_OUT_COMBINE_MODE );
		    		memcpy(gpioModeOut1, GPIO_OUT_COMBINE_MODE, sizeof(GPIO_OUT_COMBINE_MODE) );
		    		gpioOutDeley1 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_1) + 1 + sizeof(GPIO_OUT_COMBINE_MODE) + 1 ] );
#ifdef DEBUG
	   os_printf("gpioModeOut1 %s, gpioOutDeley1 %d ",gpioModeOut1, gpioOutDeley1);
#endif
		    		writeFlash( GPIO_OUT_1_DELEY_OFSET ,&tmp[ sizeof(TCP_GPIO_MODE_1) + 1 + sizeof(GPIO_OUT_COMBINE_MODE) + 1 ] );
		    		tcpRespounseBuilder( TCP_OPERATION_OK );
			    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
			    	}


	    	} else { 																								      // error

		    	memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
		    	tmp[ sizeof(TCP_ERROR) ] = '\r';
		    	tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';

		    	tcpSt = TCP_FREE;
		    	if ( NULL != pespconn ) {

		    		espconn_send( pespconn, tmp, ( sizeof( TCP_ERROR ) + 2 ) );
		    	}

	    	}

	    } else if ( 0 == strcmp( tmp, TCP_GPIO_MODE_2 ) ) {

	    	if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 ], GPIO_OUT_IMPULSE_MODE ) ) {

	    		writeFlash( GPIO_OUT_2_MODE_OFSET, GPIO_OUT_IMPULSE_MODE );
	    		memcpy(gpioModeOut2, GPIO_OUT_IMPULSE_MODE, sizeof(GPIO_OUT_IMPULSE_MODE) );
	    		gpioOutDeley2 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 + sizeof(GPIO_OUT_IMPULSE_MODE) + 1 ] );
#ifdef DEBUG
	   os_printf("gpioModeOut2 %s, gpioOutDeley2 %d ",gpioModeOut2, gpioOutDeley2);
#endif
	    		writeFlash( GPIO_OUT_2_DELEY_OFSET ,&tmp[ sizeof(TCP_GPIO_MODE_2) + 1 + sizeof(GPIO_OUT_IMPULSE_MODE) + 1 ] );
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
		    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
		    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
		    	}

	    	} else if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 ], GPIO_OUT_TRIGGER_MODE ) ) {

	    		writeFlash( GPIO_OUT_2_MODE_OFSET, GPIO_OUT_TRIGGER_MODE );
	    		memcpy(gpioModeOut2, GPIO_OUT_TRIGGER_MODE, sizeof(GPIO_OUT_TRIGGER_MODE) );
	    		gpioOutDeley2 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 + sizeof(GPIO_OUT_TRIGGER_MODE) + 1 ] );
#ifdef DEBUG
	    os_printf("gpioModeOut2 %s, gpioOutDeley2 %d ",gpioModeOut2, gpioOutDeley2);
#endif
	    		writeFlash( GPIO_OUT_2_DELEY_OFSET ,&tmp[ sizeof(TCP_GPIO_MODE_2) + 1 + sizeof(GPIO_OUT_TRIGGER_MODE) + 1 ] );
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
		    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
		    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
		    	}

	    	} else if ( 0 == strcmp( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 ], GPIO_OUT_COMBINE_MODE ) ) {

	    		writeFlash( GPIO_OUT_2_MODE_OFSET, GPIO_OUT_COMBINE_MODE );
	    		memcpy(gpioModeOut2, GPIO_OUT_COMBINE_MODE, sizeof(GPIO_OUT_COMBINE_MODE) );
	    		gpioOutDeley2 = StringToInt( &tmp[ sizeof(TCP_GPIO_MODE_2) + 1 + sizeof(GPIO_OUT_COMBINE_MODE) + 1 ] );
#ifdef DEBUG
	    os_printf("gpioModeOut2 %s, gpioOutDeley2 %d ",gpioModeOut2, gpioOutDeley2);
#endif
	    		writeFlash( GPIO_OUT_2_DELEY_OFSET ,&tmp[ sizeof(TCP_GPIO_MODE_2) + 1 + sizeof(GPIO_OUT_COMBINE_MODE) + 1 ] );
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
		    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
		    					                                                  (uint32 *)broadcastTmp, SPI_FLASH_SEC_SIZE ) ) {
		    	}

	    	} else { // error
		    	memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
		    	tmp[ sizeof(TCP_ERROR) ] = '\r';
		    	tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';

		    	tcpSt = TCP_FREE;
	       	    if ( NULL != pespconn ) {

	    			espconn_send( pespconn, tmp, ( sizeof( TCP_ERROR ) + 2 ) );
	       	    }

	    	}

	  } else { // ERROR

		  	 memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
	    	 tmp[ sizeof(TCP_ERROR) ] = '\r';
	    	 tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';
#ifdef DEBUG
	    os_printf("error check ");
#endif
	    	tcpSt = TCP_FREE;
	    	if ( NULL != pespconn ) {

	    		espconn_send( pespconn, tmp, ( sizeof( TCP_ERROR ) + 2 ) );
	    	}

	 }
}


LOCAL void ICACHE_FLASH_ATTR
tcpRespounseBuilder( uint8_t *responseCode ) {

	uint16_t i, lenght;

	for ( i = 0; '\r' != tmp[ i ] && i < TMP_SIZE; i++ ) {

	}

	tmp[i++] = ' ';
	lenght = strlen( responseCode ) + 1 ;
	memcpy( &tmp[ i ], responseCode, lenght );
	i += lenght;
	tmp[ i++ ] = '\r';
	tmp[ i++ ] = '\n';

#ifdef DEBUG
	{
			uint16_t a;
			os_printf(" tcp answer ");
			for ( a = 0; a < i; a++) {
				uart_tx_one_char(tmp[a]);
			}
		}
#endif
	tcpSt = TCP_FREE;
	if ( NULL != pespconn ) {

		espconn_send( pespconn, tmp, i );
	}
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
LOCAL compStr ICACHE_FLASH_ATTR
compareLenght( uint8_t *string, uint16_t maxLenght, uint16_t minLenght ) {

	uint16_t i;

	for ( i = 0; '\0' != string[ i ]; i++ ) {

		if ( '\0' == string[ i ] || i > maxLenght ) {

			break;
		}
	}

	if ( i > maxLenght || i < minLenght  ) {

		return LENGHT_ERROR;
	} else  {

		return LENGHT_OK;
	}
}


LOCAL void ICACHE_FLASH_ATTR
buildQueryResponse( uint8_t *responseStatus ) {

	uint16_t i;
	uint8_t *p = &tmp[ sizeof(TCP_QUERY) + 1 + sizeof(TCP_ADRESS) + 1 ];

	p = ShortIntToString( addressQuery, p );
	*p++ = '\0';
	*p++ = ' ';
	memcpy( p, TCP_LENGHT, sizeof(TCP_LENGHT) );
	p += sizeof( TCP_LENGHT );
	*p++ = ' ';
	p = ShortIntToString( lenghtQuery, p );
	*p++ = '\0';
	*p++ = ' ';
	memcpy( p, storage, lenghtQuery );
	p += lenghtQuery;
	*p++ = ' ';
	memcpy( p, responseStatus, strlen(responseStatus) + 1 );
	p += strlen(responseStatus) + 1;
	*p++ = '\r';
	*p++ = '\n';

	i = p - tmp;

#ifdef DEBUG
	{
		uint16_t a;
		for ( a = 0; a < i; a++) {
			uart_tx_one_char(tmp[a]);
		}
	}
#endif

	tcpSt = TCP_FREE;
	if ( NULL != pespconn ) {

		espconn_send( pespconn, tmp, i );
	}
}













