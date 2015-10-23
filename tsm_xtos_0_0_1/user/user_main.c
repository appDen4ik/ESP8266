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
LOCAL inline void ICACHE_FLASH_ATTR initWIFI( void ) __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR loadDefParam( void ) __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR broadcastBuilder( void ) __attribute__((always_inline));
LOCAL void ICACHE_FLASH_ATTR buildQueryResponse( uint8_t *responseStatus );
LOCAL void ICACHE_FLASH_ATTR tcpRespounseBuilder( uint8_t *responseStatus );
LOCAL compStr ICACHE_FLASH_ATTR compareLenght( uint8_t *string, uint16_t maxLenght );
LOCAL uint32_t ICACHE_FLASH_ATTR StringToInt(uint8_t *data);
LOCAL uint8_t* ICACHE_FLASH_ATTR  ShortIntToString(uint32_t data, uint8_t *adressDestenation);
LOCAL uint8_t * ICACHE_FLASH_ATTR intToStringHEX(uint8_t data, uint8_t *adressDestenation);
LOCAL res ICACHE_FLASH_ATTR writeFlash( uint16_t where, uint8_t *what );
//**********************************************************************************************************************************

extern void ets_wdt_enable (void);
extern void ets_wdt_disable (void);
extern void ets_wdt_init(void);

//**********************************************************************************************************************************
LOCAL uint8_t tmp[TMP_SIZE]; // только для работы с сетью, приема запросов и формирования ответов
//**********************************************************************************************************************************
LOCAL uint8_t tmpFLASH[SPI_FLASH_SEC_SIZE]; //broadcast
//**********************************************************************************************************************************
LOCAL uint8_t writeFlashTmp[ SPI_FLASH_SEC_SIZE ];
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
//**********************************************************************************************************************************
LOCAL struct espconn broadcast;
//**********************************************************************************************************************************
LOCAL uint8_t ledState = 0;
LOCAL uint32_t broadcastTmr;
LOCAL uint8_t *broadcastShift;
LOCAL uint8_t brodcastMessage[500] = { 0 };
//**********************************************************************************************************************************
LOCAL uint8_t rssiStr[5];
LOCAL sint8_t rssi;
//**********************************************************************************************************************************
LOCAL uint8_t *count;
//**********************************************************************************************************************************
LOCAL struct ip_info inf;
//*********************************************************************************************************************************
LOCAL uint8_t storage[1000];
//**********************************************************************************************************************************
uint32_t addressQuery;
uint16_t lenghtQuery;
//**********************************************************************************************************************************
//authorization tcp
LOCAL tcp_stat tcpSt = TCP_FREE;
LOCAL uint16_t counterForTmp = 0;
LOCAL struct espconn *pespconn;
LOCAL uint32_t ipAdd;
LOCAL mark marker = mCLEAR;
//**********************************************************************************************************************************
LOCAL struct espconn brodcastSSA;
LOCAL esp_udp udpSTA;
//**********************************************************************************************************************************
LOCAL uint8_t routerSSID[32];
LOCAL uint8_t routerPWD[64];
//**********************************************************************************************************************************
struct espconn espconnServer;
esp_tcp tcpServer;
esp_udp udpClient;


uint8_t alignString[ALIGN_STRING_SIZE];

void user_rf_pre_init(void)
{

}

//**********************************************************Callbacks***************************************************************
LOCAL void ICACHE_FLASH_ATTR
tcp_recvcb( void *arg, char *pdata, unsigned short len ) { // data received

	struct espconn *conn = (struct espconn *) arg;
	uint8_t *str;
#ifdef DEBUG
	os_printf( " |tcp_recvcb remote ip Connected : %d.%d.%d.%d \r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
	os_printf( " |tcp_recvcb local ip Connected : %d.%d.%d.%d \r\n| ",   IP2STR( conn->proto.tcp->local_ip ) );
#endif

#ifdef DEBUG
	{
		int i;
		for ( i = 0; i < len; i++) {
			uart_tx_one_char( pdata[i] );
		}
	}
#endif
   if ( NULL != conn && NULL != pespconn ) {
   if ( '\r' == pdata[ len - 2 ] && (uint32)( conn->proto.tcp->remote_ip[0] ) == (uint32)( pespconn->proto.tcp->remote_ip[0]) ) {

    	if ( TMP_SIZE >= counterForTmp + len ) {

    		os_printf(" |tcp_recvcb check point| ");
    		memcpy( &tmp[counterForTmp], pdata, len );
#ifdef DEBUG
    	{
    		int i;
    		os_printf( " |tcp_recvcb check point 1| ");
			for (i = 0; '\r' != tmp[i]; i++) {
		  			   uart_tx_one_char(tmp[i]);
		 	 }
			uart_tx_one_char(tmp[i]);
    	}
#endif

    		counterForTmp = 0;
    		comandParser();

    	} else {

    		memcpy( tmp, TCP_NOT_ENOUGH_MEMORY, ( sizeof(TCP_NOT_ENOUGH_MEMORY) ) );
    		tmp[ sizeof(TCP_NOT_ENOUGH_MEMORY) ] = '\r';
    		tmp[ sizeof(TCP_NOT_ENOUGH_MEMORY) + 1 ] = '\n';
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
    		espconn_send( pespconn, tmp, ( sizeof( TCP_NOT_ENOUGH_MEMORY ) + 2 ) );
    	}
    }
   }
}


// espconn_sent(arg, tmp, strlen(tmp));
LOCAL void ICACHE_FLASH_ATTR
tcp_connectcb( void *arg ) { // TCP connected successfully

	struct espconn *conn = (struct espconn *) arg;

	if ( TCP_FREE == tcpSt ) {

		pespconn = (struct espconn *) arg;
		ipAdd = *(uint32 *)( pespconn->proto.tcp->remote_ip );
#ifdef DEBUG
		os_printf( " |tcp_connectcb ip : %d Connected\r\n| ",  ipAdd );
#endif
		tcpSt = TCP_BUSY;

#ifdef DEBUG
		os_printf( " |tcp_connectcb ip : %d.%d.%d.%d Connected\r\n| ",  IP2STR( pespconn->proto.tcp->remote_ip ) );
#endif

	} else if ( TCP_BUSY == tcpSt && *(uint32 *)( conn->proto.tcp->remote_ip ) == ipAdd ) {

		uint8_t erB[] = { 'B', 'U', 'S', 'Y', '\0', '\r', '\n' };
		marker = mSET;
		espconn_send( conn, erB, sizeof(erB) );
#ifdef DEBUG
		os_printf( " |tcp_connectcb ip : %d.%d.%d.%d Connect busy...\r\n| ",  IP2STR( ( (struct espconn *) arg)->proto.tcp->remote_ip ) );
#endif
	} else if ( TCP_BUSY == tcpSt && *(uint32 *)( conn->proto.tcp->remote_ip ) != ipAdd ) {

		uint8_t erB[] = { 'B', 'U', 'S', 'Y', '\0', '\r', '\n' };

		espconn_send( conn, erB, sizeof(erB) );
#ifdef DEBUG
		os_printf( " |tcp_connectcb ip : %d.%d.%d.%d Connect busy...\r\n| ",  IP2STR( ( (struct espconn *) arg)->proto.tcp->remote_ip ) );
#endif
	}

}


LOCAL void ICACHE_FLASH_ATTR
tcp_disnconcb( void *arg ) { // TCP disconnected successfully

	struct espconn *conn = (struct espconn *) arg;

#ifdef DEBUG
	os_printf( " |tcp_disnconcb TCP disconnected successfully : %d.%d.%d.%d\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
#endif

	if ( NULL != pespconn ) {
#ifdef DEBUG
	os_printf( " |tcp_disnconcb check point| " );
#endif
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
	}
}


LOCAL void ICACHE_FLASH_ATTR
tcp_reconcb( void *arg, sint8 err ) { // error, or TCP disconnected

	struct espconn *conn = (struct espconn *) arg;

#ifdef DEBUG
	os_printf( " |tcp_reconcb TCP RECON : %d.%d.%d.%d\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
#endif

	if ( NULL != pespconn ) {
#ifdef DEBUG
	os_printf( " |tcp_reconcb check point| " );
#endif
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
	}
}


LOCAL void ICACHE_FLASH_ATTR
tcp_sentcb( void *arg ) { // data sent
	struct espconn *conn = (struct espconn *) arg;

#ifdef DEBUG
	os_printf( " |tcp_sentcb DATA sent for ip : %d.%d.%d.%d\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
#endif
	if ( NULL != pespconn && NULL != conn ) {
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
	}
	if ( NULL != conn ) {
		os_printf( " |tcp_sentcb check point 1| " );
		espconn_disconnect(conn);
	}
}

//**********************************************************************************************************************************

res ICACHE_FLASH_ATTR
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


void ICACHE_FLASH_ATTR
mScheduler(char *datagram, uint16 size) {

//*************************************************************************************
/*	 if ( 0 == GPIO_INPUT_GET( INP_3_PIN ) ) {

			 uint8_t *str;
			 str = tmp;
				 memcpy( str, TCP_RESTORE, sizeof(TCP_RESTORE) );
				 str += sizeof(TCP_RESTORE);
				*str++ = '\r';
				*str++ = '\n';

				comandParser();
	 }

	 if ( 0 == GPIO_INPUT_GET( INP_2_PIN ) ) {

			 uint8_t *str;
			 str = tmp;
				 memcpy( str, TCP_ENABLE_GPIO_2, sizeof(TCP_ENABLE_GPIO_2) );
				 str += sizeof(TCP_ENABLE_GPIO_2);
				*str++ = '\r';
				*str++ = '\n';

				comandParser();
	 }

	 if ( 0 == GPIO_INPUT_GET( INP_4_PIN ) ) {

			 uint8_t *str;
			 str = tmp;
				 memcpy( str, TCP_DISABLE_GPIO_2, sizeof(TCP_DISABLE_GPIO_2) );
				 str += sizeof(TCP_DISABLE_GPIO_2);
				*str++ = '\r';
				*str++ = '\n';

				comandParser();
	 }

	 if ( 0 == GPIO_INPUT_GET( INP_1_PIN ) ) {

/*		    uint16_t a, i;
		    uint8_t *p, *str;
			uint8_t ascii[10];
			static uint8_t string[] = "HELLO";
			static uint8_t string1[] = "Тестирование";

			str = tmp;

			memcpy( str, TCP_UPDATE, sizeof(TCP_UPDATE) );

			str += sizeof(TCP_UPDATE);
			*str++ = ' ';

			for ( a = 0; a < STRING_SIZE - 1; a++ ) {
				str[a] = '0';
			}

			p = ShortIntToString( digit++, ascii );

			memcpy( &str[ STRING_SIZE - 1 - (p - ascii) - 1 - 40], ascii, (p - ascii) );

			str[ STRING_SIZE - 15 - 40 ] = START_OF_FIELD;
			str[ STRING_SIZE - 2 - 40 ]  = END_OF_FIELD;
			str[ STRING_SIZE - 1 ]  = END_OF_STRING;


			str += ALIGN_STRING_SIZE;

			*str++ = ' ';

			for ( a = 0; a < STRING_SIZE - 1; a++ ) {
							str[a] = '0';
			}
			p = ShortIntToString( digit + 5, ascii );

			memcpy( &str[ STRING_SIZE - 1 - (p - ascii) - 1 - 40], ascii, (p - ascii) );

			str[ STRING_SIZE - 15 - 40 ] = START_OF_FIELD;
			str[ STRING_SIZE - 2 - 40 ]  = END_OF_FIELD;
			str[ STRING_SIZE - 1 ]  = END_OF_STRING;

			str += ALIGN_STRING_SIZE;

			*str++ = '\r';
			*str++ = '\n';


			   for (i = 0; i < str - tmp; i++) {
			        uart_tx_one_char(tmp[i]);
			    }

			   comandParser();
			   os_printf( "tst");
			   for (i = 0; '\r' != tmp[ i ]; i++) {
			   		uart_tx_one_char(tmp[i]);
			   	}
			   uart_tx_one_char(tmp[i++]);
			   uart_tx_one_char(tmp[i]);
			   os_printf( "Check flash");
			   spi_flash_read( SPI_FLASH_SEC_SIZE * START_SECTOR, (uint32 *)tmp, 1000 );
			   		for ( i = 0; 1000 > i; i++ ) {
			   			uart_tx_one_char(tmp[i]);
			   			system_soft_wdt_stop();
			   	}*/

/*		 uint8_t *p, *str;
		 uint32_t i, currentSector;
		const uint8_t data[] = "25621";
		 uint8_t data2[] ="test";

		 str = tmp;
		 memcpy( str, TCP_SSID_STA, sizeof(TCP_SSID_STA) );
		 str += sizeof(TCP_SSID_STA);
		 *str++ = ' ';
		*str++ = '7';
		*str++ = '8';
		*str++ = '8';
		*str++ = '2';
		*str++ = '4';
		*str++ = '\0';
		*str++ = '\r';
		*str++ = '\n';

			   os_printf( " Request ");
			for (i = 0; i < str - tmp; i++) {
			  			        uart_tx_one_char(tmp[i]);
			 }

			   comandParser();


/*
				for ( currentSector = USER_SECTOR_IN_FLASH_MEM; currentSector <= USER_SECTOR_IN_FLASH_MEM; currentSector++ ) {
						os_printf( " currentSector   %d", currentSector);
						spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)tmp, SPI_FLASH_SEC_SIZE );
						for ( i = 0; SPI_FLASH_SEC_SIZE > i; i++ ) {
							uart_tx_one_char(tmp[i]);
						}
						system_soft_wdt_stop();
					}
*/
/*			while ( 0 == GPIO_INPUT_GET( INP_1_PIN ) ) {

				system_soft_wdt_stop();
			}
			os_printf( "check point ");

*/
//	 }

//************************************************************************************
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
			if ( gpioOutDeley2Counter < gpioOutDeley2 && GPIO_INPUT_GET(INP_4_PIN) ) {
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
			if ( !GPIO_INPUT_GET(INP_4_PIN) ) {
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

	    struct station_info * station = wifi_softap_get_station_info();

		broadcastTmr = 0;

		broadcastBuilder();

		os_timer_disarm(&task_timer);

		// Внутрення сеть
		while ( station ) {

			IP4_ADDR( (ip_addr_t *)brodcastSSA.proto.udp->remote_ip, (uint8_t)(station->ip.addr), (uint8_t)(station->ip.addr >> 8),\
															(uint8_t)(station->ip.addr >> 16), (uint8_t)(station->ip.addr >> 24) );
#ifdef DEBUG
		os_printf( "bssid : %x:%x:%x:%x:%x:%x ip : %d.%d.%d.%d ", MAC2STR(station->bssid), IP2STR(&station->ip) );
#endif

		    brodcastSSA.proto.udp->remote_port = StringToInt( &tmpFLASH[DEF_UDP_PORT_OFSET] );
/*	    	IP4_ADDR( (ip_addr_t *)brodcastSSA.proto.udp->remote_ip, (uint8_t)( ipaddr_addr( &tmpFLASH[ DEF_IP_SOFT_AP_OFSET ] ) ), \
	    												(uint8_t)( ipaddr_addr( &tmpFLASH[ DEF_IP_SOFT_AP_OFSET ] ) >> 8 ),\
														(uint8_t)( ipaddr_addr( &tmpFLASH[ DEF_IP_SOFT_AP_OFSET ] ) >> 16 ), 255 );*/
			espconn_create(&brodcastSSA);
			switch ( espconn_send(&brodcastSSA, brodcastMessage, strlen(brodcastMessage)) ) {
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
			espconn_delete(&brodcastSSA);

			station = STAILQ_NEXT(station, next);
		}

		wifi_softap_free_station_info();

			struct station_config stationConf;
			 wifi_station_get_config( &stationConf );
			 os_printf( " stationConf.ssid  %s ", stationConf.ssid, &stationConf.ssid );
			 os_printf( " stationConf.password  %s ", stationConf.password, &stationConf.password );

		// Внешняя сеть
		switch( wifi_station_get_connect_status() ) {
			case STATION_GOT_IP:
				if ( ( rssi = wifi_station_get_rssi() ) < -90 ){
					count = rssiStr;
					*count++ = '-';
					count = ShortIntToString( ~rssi, count );
					*count = '\0';
					os_printf( "Bad signal, rssi = %s ", rssiStr);
					os_delay_us(1000);
					os_printf( "Broadcast port %s ", tmpFLASH[DEF_UDP_PORT_OFSET] );
					os_printf( "%s ", broadcastShift );
					GPIO_OUTPUT_SET(LED_GPIO, ledState);
					ledState ^=1;
				} else {

					GPIO_OUTPUT_SET(LED_GPIO, 1);

					wifi_get_ip_info(STATION_IF, &inf);

					if ( 0 != inf.ip.addr ) {

						ets_uart_printf("WiFi connected\r\n ");
						os_delay_us(1000);
						os_printf( "Broadcast port %s ", &tmpFLASH[DEF_UDP_PORT_OFSET] );
						os_printf( "%s ", brodcastMessage );


						espconn_create(&broadcast);
						switch ( espconn_send(&broadcast, brodcastMessage, strlen(brodcastMessage)) ) {
							case ESPCONN_OK:
								os_printf( " |esp STA  broadcast succeed| " );
								break;
							case ESPCONN_MEM:
								os_printf( " |esp STA  broadcast out of memory| " );
								break;
							case ESPCONN_ARG:
								os_printf( " |esp STA  broadcast illegal argument| " );
								break;
							case ESPCONN_IF:
								os_printf( " |esp STA  broadcast send UDP fail| " );
								break;
							case ESPCONN_MAXNUM:
								os_printf( " |esp STA  broadcast buffer of sending data is full| " );
								break;

						}
						espconn_delete(&broadcast);
					}
				}
				break;
			case STATION_WRONG_PASSWORD:
				ets_uart_printf("WiFi connecting error, wrong password\r\n ");
				os_delay_us(1000);
		    	os_printf( "routerSSID %s ", routerSSID );
		    	os_printf( "routerPWD %s ", routerPWD );
		    	os_printf( "Broadcast port %s ", &tmpFLASH[DEF_UDP_PORT_OFSET] );
				os_printf( "%s ", broadcastShift );
				GPIO_OUTPUT_SET(LED_GPIO, ledState);
				ledState ^=1;
				break;
			case STATION_NO_AP_FOUND:
				ets_uart_printf("WiFi connecting error, ap not found\r\n" );
				os_delay_us(1000);
		    	os_printf( "routerSSID %s ", routerSSID );
		    	os_printf( "routerPWD %s ", routerPWD );
		    	os_printf( "Broadcast port %s ", &tmpFLASH[DEF_UDP_PORT_OFSET] );
				os_printf( "%s ", broadcastShift );
				GPIO_OUTPUT_SET(LED_GPIO, ledState);
				ledState ^=1;
				break;
			case STATION_CONNECT_FAIL:
				ets_uart_printf("WiFi connecting fail\r\n ");
				os_delay_us(1000);
		    	os_printf( "routerSSID %s ", routerSSID );
		    	os_printf( "routerPWD %s ", routerPWD );
		    	os_printf( "Broadcast port %s ", &tmpFLASH[DEF_UDP_PORT_OFSET] );
				os_printf( "%s ", broadcastShift );
				system_restart();
				os_delay_us(100000);
				GPIO_OUTPUT_SET(LED_GPIO, ledState);
				ledState ^=1;
				break;
			default:
				ets_uart_printf("WiFi connecting...\r\n ");

				os_delay_us(1000);
		    	os_printf( "routerSSID %s ", routerSSID );
		    	os_printf( "routerPWD %s ", routerPWD );
		    	os_printf( "Broadcast port %s ", &tmpFLASH[DEF_UDP_PORT_OFSET] );
				os_printf( "%s ", broadcastShift );
				GPIO_OUTPUT_SET(LED_GPIO, ledState);
				ledState ^=1;
				break;
		}
	} else {

		broadcastTmr += 10;
	}

	os_timer_setfn(&task_timer, (os_timer_func_t *)mScheduler, (void *)0);
	os_timer_arm(&task_timer, DELAY, 0);
}


void ICACHE_FLASH_ATTR
config(void) {

	initWIFI(); // настройка sta ap

   	ets_wdt_init();
    ets_wdt_disable();
   	ets_wdt_enable();
   	system_soft_wdt_stop();
    system_soft_wdt_restart();

	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&task_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
		os_timer_setfn(&task_timer, (os_timer_func_t *)mScheduler, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&task_timer, DELAY, 0);
}


void ICACHE_FLASH_ATTR
user_init(void) {

	uint8_t clearStatus[ sizeof(CLEAR_DB_STATUS) ];

	initPeriph();

 	ets_wdt_disable();
   	system_soft_wdt_stop();

	os_printf("OS reset status: %d", system_get_rst_info()->reason ); //после падения сервера через soft AP
	 if ( system_get_rst_info()->reason != REASON_SOFT_RESTART \
			 && system_get_rst_info()->reason !=  REASON_DEFAULT_RST) {	// необходима еще одна перезагрузка
		// system_soft_wdt_restart();
//		 ets_wdt_enable();
		// 	 system_restart();
		 	//os_delay_us(3000000);
//		 	 while(1);
	 }

    os_printf( " sdk version: %s \n", system_get_sdk_version() );
	os_printf( " module version 0.1 ");

//	spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM );
	//первая загрузка
	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                                                                  (uint32 *)writeFlashTmp, ALIGN_FLASH_READY_SIZE ) ) {

		ets_uart_printf("Read fail ");
		while(1);
	}

	if ( 0 != strcmp( writeFlashTmp, FLASH_READY ) ) {

		if ( OPERATION_OK != clearSectorsDB() ) {

			ets_uart_printf("clearSectorsDB() fail ");
			while(1);
		}

		loadDefParam();
	}

	 // запрос очистки дб
	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                            (uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE ) ) {

		ets_uart_printf("Clear db fail ");
		while(1);
	}

	if ( 0 == strcmp( CLEAR_DB_STATUS, &writeFlashTmp[ CLEAR_DB_STATUS_OFSET ] ) ) {
#ifdef DEBUG
		ets_uart_printf("clearSectorsDB() check point ");
#endif
		writeFlash( CLEAR_DB_STATUS_OFSET, CLEAR_DB_STATUS_EMPTY );
		if ( OPERATION_OK != clearSectorsDB() ) {

			ets_uart_printf("clearSectorsDB() fail ");
			while(1);
		}
	}

//	else {

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------

/*	clearSectorsDB();

	{
		uint8_t fsdlf[50];
		uint8_t ascii[10];
		static uint8_t string[] = "HELLO";
		static uint8_t string1[] = "Тестирование";


		uint16_t lenght;
		uint32_t adress;

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

			for ( i = 1; i <=  ( SPI_FLASH_SEC_SIZE / ALIGN_STRING_SIZE ) * (END_SECTOR - START_SECTOR + 1) - 1; i++ ) {

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

		//		os_printf( " \n %s \n String lenght %d", alignString, ( strlen( alignString ) + 1 ) );

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

			for ( currentSector = END_SECTOR; currentSector <= END_SECTOR; currentSector++ ) {
				spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)tmp, SPI_FLASH_SEC_SIZE );
				for ( i = 0; SPI_FLASH_SEC_SIZE > i; i++ ) {
					uart_tx_one_char(tmp[ i ]);

				}

	        }

	}*/

 if ( 0 == GPIO_INPUT_GET(INP_3_PIN) )  {

	uint16_t c, currentSector;
	uint32_t i;

	for ( currentSector = START_SECTOR; currentSector <= END_SECTOR; currentSector++ ) {
		os_printf("currentSector: %d ", currentSector);
		spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)tmp, SPI_FLASH_SEC_SIZE );
		for ( i = 0; SPI_FLASH_SEC_SIZE > i; i++ ) {
			uart_tx_one_char(tmp[ i ]);

		}

		system_soft_wdt_stop();
	}
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------

 //wifi_set_opmode_current (NULL_MODE);
	// broadcast
	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                                                                          (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {

		ets_uart_printf("Read fail");
		while(1);
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                                                                          (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
		ets_uart_printf("Read for gpioMode fail!!");
		while(1);
	}

	if ( 0 == strcmp( &tmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_IMPULSE_MODE ) || \
				                                             0 == strcmp( &tmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) || \
															 0 == strcmp( &tmp[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_COMBINE_MODE ) ) {

		gpioOutDeley1 = StringToInt( &tmp[GPIO_OUT_1_DELEY_OFSET] );

		os_sprintf( gpioModeOut1, "%s", &tmp[ GPIO_OUT_1_MODE_OFSET ] );

#ifdef DEBUG
	  os_printf( " gpioModeOut1  %s,  gpioOutDeley1  %d ", gpioModeOut1, gpioOutDeley1 );
	  os_delay_us(50000);
#endif

	}

	if ( 0 == strcmp( &tmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_IMPULSE_MODE ) || \
            												0 == strcmp( &tmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) || \
															0 == strcmp( &tmp[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_COMBINE_MODE ) ) {

		gpioOutDeley2 = StringToInt( &tmp[GPIO_OUT_2_DELEY_OFSET] );

		os_sprintf( gpioModeOut2, "%s", &tmp[ GPIO_OUT_2_MODE_OFSET ] );

#ifdef DEBUG
	  os_printf( " gpioModeOut2  %s,  gpioOutDeley2  %d ", gpioModeOut2, gpioOutDeley2 );
	  os_delay_us(50000);
#endif

	}

#ifdef DEBUG
	{

	uint16_t c, currentSector;

	for ( currentSector = USER_SECTOR_IN_FLASH_MEM; currentSector <= USER_SECTOR_IN_FLASH_MEM; currentSector++ ) {
			os_printf( " currentSector   %d ", currentSector);
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

		espconn_tcp_set_max_con(2);

#ifdef DEBUG
    	os_printf( "espconn_tcp_get_max_con(); %d ", espconn_tcp_get_max_con() );
#endif

  /*  	if ( 0 == espconn_tcp_set_max_con_allow( &espconnServer, 2 ) ) {

    		os_printf( "espconn_tcp_set_max_con_allow( espconnServer, 2 ) fail " );
    	} else {

    		os_printf( "espconn_tcp_get_max_con_allow( espconnServer, 2 ) %d ", espconn_tcp_get_max_con_allow(&espconnServer) );
    	}*/

	}

    { //udp клиент Sta

    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
    			                                                                  (uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE ) ) {

    	}

    	broadcast.type = ESPCONN_UDP;
    	broadcast.state = ESPCONN_NONE;
    	broadcast.proto.udp = &udpClient;
    	broadcast.proto.udp->remote_port = StringToInt( &writeFlashTmp[DEF_UDP_PORT_OFSET] );
#ifdef DEBUG
    	os_printf( "broadcast.proto.udp->remote_port %d ", broadcast.proto.udp->remote_port );
#endif

     //udp клиент AP

    	brodcastSSA.type = ESPCONN_UDP;
    	brodcastSSA.state = ESPCONN_NONE;
    	brodcastSSA.proto.udp = &udpSTA;
    	brodcastSSA.proto.udp->remote_port = StringToInt( &writeFlashTmp[DEF_UDP_PORT_OFSET] );
    	IP4_ADDR( (ip_addr_t *)brodcastSSA.proto.udp->remote_ip, (uint8_t)( ipaddr_addr( &writeFlashTmp[ DEF_IP_SOFT_AP_OFSET ] ) ), \
    												(uint8_t)( ipaddr_addr( &writeFlashTmp[ DEF_IP_SOFT_AP_OFSET ] ) >> 8 ),\
													(uint8_t)( ipaddr_addr( &writeFlashTmp[ DEF_IP_SOFT_AP_OFSET ] ) >> 16 ), 255 );
#ifdef DEBUG
    	os_printf( "brodcastSSA.proto.udp->remote_ip %d.%d.%d.%d ", IP2STR(brodcastSSA.proto.udp->remote_ip) );
#endif
    }

/*	initWIFI();

   	ets_wdt_init();
    ets_wdt_disable();
   	system_soft_wdt_stop();
   	ets_wdt_enable();
    system_soft_wdt_restart();*/

	if(wifi_station_get_auto_connect() == 0) {

			wifi_station_set_auto_connect(1);
	}

    wifi_station_set_reconnect_policy(true);

    memcpy( routerSSID, &writeFlashTmp[SSID_STA_OFSET], strlen(&writeFlashTmp[SSID_STA_OFSET]) + 1 );
    memcpy( routerPWD, &writeFlashTmp[PWD_STA_OFSET], strlen(&writeFlashTmp[PWD_STA_OFSET]) + 1 );

#ifdef DEBUG
    	os_printf( "routerSSID %s", routerSSID );
    	os_printf( "routerPWD %s", routerPWD );
#endif


	// os_timer_disarm(ETSTimer *ptimer)
    os_timer_disarm(&task_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&task_timer, (os_timer_func_t *)config, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&task_timer, DELAY, 0);

}


void ICACHE_FLASH_ATTR
initPeriph( ) {

//	ets_wdt_enable();
	ets_wdt_disable();

	system_soft_wdt_stop();
//	system_soft_wdt_restart();

	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	uart_tx_one_char('\0');

	os_install_putc1(uart_tx_one_char);

	if ( SYS_CPU_160MHZ != system_get_cpu_freq() ) {
		system_update_cpu_freq(SYS_CPU_160MHZ);
	}

	PIN_FUNC_SELECT(OUT_1_MUX, OUT_1_FUNC);
	GPIO_OUTPUT_SET(OUT_1_GPIO, 0);

	PIN_FUNC_SELECT(OUT_2_MUX, OUT_2_FUNC);
	GPIO_OUTPUT_SET(OUT_2_GPIO, 0);

	PIN_FUNC_SELECT(LED_MUX, LED_FUNC);
	GPIO_OUTPUT_SET(LED_GPIO, 0);

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

	 // wifi_set_opmode( STATION_MODE );


	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
																		(uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE ) ) {

		ets_uart_printf("initWIFI Read fail ");
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

	if( wifi_station_get_config( &stationConf ) ) {

	  if ( 0 != strcmp( stationConf.ssid, &writeFlashTmp[ SSID_STA_OFSET ] ) ) {

		  os_memset( stationConf.ssid, 0, sizeof(&stationConf.ssid) );

		  os_sprintf( stationConf.ssid, "%s", &writeFlashTmp[ SSID_STA_OFSET ] );
	  }
#ifdef DEBUG
	  os_printf( " stationConf.ssid  %s,  &tmp[ SSID_STA_OFSET ]  %s ", stationConf.ssid, &writeFlashTmp[ SSID_STA_OFSET ] );
#endif
	  if ( 0 != strcmp( stationConf.password, &writeFlashTmp[ PWD_STA_OFSET ] ) ) {

		  os_memset( stationConf.password, 0, sizeof(&stationConf.password) );
		  os_sprintf( stationConf.password, "%s", &writeFlashTmp[ PWD_STA_OFSET ] );
	  }
#ifdef DEBUG
	  os_printf( " stationConf.password  %s,  &tmp[ PWD_STA_OFSET ]  %s ", stationConf.password, &writeFlashTmp[ PWD_STA_OFSET ] );
#endif
	  if ( 0 !=  stationConf.bssid_set ) {

		  stationConf.bssid_set = 0;
	  }

	 if ( !wifi_station_set_config( &stationConf ) || !wifi_station_set_config_current( &stationConf ) ) {

		 ets_uart_printf("Module not set station config!\r\n ");
	 }

	} else {

		ets_uart_printf("read station config fail\r\n ");
	}

	wifi_station_connect();
	wifi_station_dhcpc_start();


	if ( wifi_softap_get_config( &softapConf ) ) {

		wifi_softap_dhcps_stop();
/*	    ets_wdt_disable();
	   	system_soft_wdt_stop();
		os_delay_us(2000000);
	   	ets_wdt_init();
	   	ets_wdt_enable();
	    system_soft_wdt_restart();*/

		if ( 0 != strcmp( softapConf.ssid, &writeFlashTmp[ SSID_AP_OFSET ] ) ) {

			os_memset( softapConf.ssid, 0, sizeof(&softapConf.ssid) );
			softapConf.ssid_len = os_sprintf( softapConf.ssid, "%s", &writeFlashTmp[ SSID_AP_OFSET ] );
		}
#ifdef DEBUG
		os_printf( " softapConf.ssid  %s,  &tmp[ SSID_AP_OFSET ]  %s, softapConf.ssid_len  %d", \
														softapConf.ssid, &writeFlashTmp[ SSID_AP_OFSET ], softapConf.ssid_len );
#endif

		if ( 0 != strcmp( softapConf.password, &writeFlashTmp[ PWD_AP_OFSET ] ) ) {

			os_memset( softapConf.password, 0, sizeof(&softapConf.password) );
			os_sprintf( softapConf.password, "%s", &writeFlashTmp[ PWD_AP_OFSET ] );
		}
#ifdef DEBUG
		os_printf( " softapConf.password  %s,  &tmp[ PWD_AP_OFSET ]  %s", softapConf.password, &writeFlashTmp[ PWD_AP_OFSET ] );
#endif

		if ( softapConf.channel != DEF_CHANEL ) {

			softapConf.channel = DEF_CHANEL;
		}
#ifdef DEBUG
		os_printf( " softapConf.channel  %d ", softapConf.channel );
#endif

		if ( softapConf.authmode != DEF_AUTH ) {

			softapConf.authmode = DEF_AUTH;
		}
#ifdef DEBUG
		os_printf( " softapConf.authmode  %d ", softapConf.authmode );
#endif

		if ( softapConf.max_connection != MAX_CON ) {

		    softapConf.max_connection = MAX_CON;
		}
#ifdef DEBUG
		os_printf( " softapConf.max_connection  %d ", softapConf.max_connection );
#endif

		if ( softapConf.ssid_hidden != NO_HIDDEN ) {

			softapConf.ssid_hidden = NO_HIDDEN;
		}
#ifdef DEBUG
		os_printf( " softapConf.ssid_hidden  %d ", softapConf.ssid_hidden );
#endif

		if ( softapConf.beacon_interval != BEACON_INT ) {

			softapConf.beacon_interval = BEACON_INT;
		}
#ifdef DEBUG
		os_printf( " softapConf.beacon_interval  %d ", softapConf.beacon_interval );
#endif

		if( !wifi_softap_set_config( &softapConf ) ) {
			#ifdef DEBUG
					ets_uart_printf("Module not set AP config!\r\n ");
			#endif
		}

	} else {

		ets_uart_printf("read soft ap config fail\r\n ");
	}


	if ( wifi_get_ip_info(SOFTAP_IF, &ipinfo ) ) {

		ipinfo.ip.addr = ipaddr_addr( &writeFlashTmp[ DEF_IP_SOFT_AP_OFSET ] );
//		ipinfo.gw.addr = ipinfo.ip.addr;  //шлюз
		IP4_ADDR(&ipinfo.netmask, 255, 255, 255, 0);

		wifi_set_ip_info(SOFTAP_IF, &ipinfo);

	} else {

		ets_uart_printf("read ip ap fail\r\n ");
	}
#ifdef DEBUG
		os_printf( " ipinfo.ip.addr  %d ", ipinfo.ip.addr );
#endif

		wifi_softap_dhcps_start();

		if(wifi_get_phy_mode() != PHY_MODE_11N) {

				wifi_set_phy_mode(PHY_MODE_11N);
		}
}


void ICACHE_FLASH_ATTR
loadDefParam( void ) {

	uint16_t i;

	for ( i = 0; i < SPI_FLASH_SEC_SIZE; i++ ) {
		writeFlashTmp[i] = 0xff;
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM )  ) {

		ets_uart_printf("Erase USER_SECTOR_IN_FLASH_MEM fail ");
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

	memcpy( &writeFlashTmp[CLEAR_DB_STATUS_OFSET], CLEAR_DB_STATUS_EMPTY, sizeof(CLEAR_DB_STATUS_EMPTY) );
	writeFlashTmp[ sizeof(CLEAR_DB_STATUS_EMPTY) + CLEAR_DB_STATUS_OFSET ] = '\n';


	memcpy( &writeFlashTmp[MAX_TPW_HEADER_OFSET], MAX_TPW_HEADER, sizeof(MAX_TPW_HEADER) );
	writeFlashTmp[ sizeof(MAX_TPW_HEADER) + MAX_TPW_HEADER_OFSET ] = '\n';

	memcpy( &writeFlashTmp[MAX_TPW_VALUE_OFSET], MAX_TPW_DEFAULT_VALUE, sizeof(MAX_TPW_DEFAULT_VALUE) );
		writeFlashTmp[ sizeof(MAX_TPW_DEFAULT_VALUE) + MAX_TPW_VALUE_OFSET ] = '\n';

	if ( SPI_FLASH_RESULT_OK != spi_flash_write( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
																				(uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE ) ) {

		ets_uart_printf("write default param fail ");
		while(1);
	}

		system_restore();
}

// Перевод числа в последовательность ASCII
uint8_t * ICACHE_FLASH_ATTR
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
uint32_t ICACHE_FLASH_ATTR
StringToInt(uint8_t *data) {
	uint32_t returnedValue = 0;
	for (;*data >= '0' && *data <= '9'; data++) {

		returnedValue = 10 * returnedValue + (*data - '0');
	}
	return returnedValue;
}

// Перевод шестнадцетиричного числа в последовательность ASCII
uint8_t * ICACHE_FLASH_ATTR
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
void ICACHE_FLASH_ATTR
broadcastBuilder( void ) {

	uint8_t macadr[10];

	//выделяем бродкаст айпишку

	if (STATION_GOT_IP == wifi_station_get_connect_status() ) {

		wifi_get_ip_info( STATION_IF, &inf );
		IP4_ADDR((ip_addr_t *)broadcast.proto.udp->remote_ip, (uint8_t)(inf.ip.addr), (uint8_t)(inf.ip.addr >> 8),\
															(uint8_t)(inf.ip.addr >> 16), 255);
	} else {

		inf.ip.addr = 0;
	}

	broadcast.proto.udp->remote_port = StringToInt( &tmpFLASH[DEF_UDP_PORT_OFSET] );

	count = brodcastMessage;

	memcpy( count, NAME, ( sizeof( NAME ) - 1 ) );
	count += sizeof( NAME ) - 1;

	os_sprintf( count, "%s", &tmpFLASH[ BROADCAST_NAME_OFSET ] );
	count += strlen( &tmpFLASH[ BROADCAST_NAME_OFSET ] );

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
    memcpy( count, IP_AP, ( sizeof( IP_AP ) - 1 ) );
    count += sizeof( IP_AP ) - 1;

    os_sprintf( count, "%s", &tmpFLASH[ DEF_IP_SOFT_AP_OFSET ] );
    count += strlen( &tmpFLASH[ DEF_IP_SOFT_AP_OFSET ] );
//================================================================
 /*   memcpy( count, SERVER_PORT, ( sizeof( SERVER_PORT ) - 1 ) );
    count += sizeof( SERVER_PORT ) - 1;

    count = ShortIntToString( TCP_PORT, count ); */
//================================================================
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
//================================================================
    if ( ( rssi = wifi_station_get_rssi() ) < 0 ) {

    	memcpy( count, RSSI, ( sizeof( RSSI ) - 1 ) );
	    count += sizeof( RSSI ) - 1;

	    *count++ = '-';
	    count = ShortIntToString( (rssi * (-1)), count );
    }
//================================================================
//================================================================
	memcpy( count, SSID_STA, ( sizeof( SSID_STA ) - 1 ) );
	count += sizeof( SSID_STA ) - 1;

	os_sprintf( count, "%s", &tmpFLASH[ SSID_STA_OFSET ] );
	count += strlen( &tmpFLASH[ SSID_STA_OFSET ] );
//================================================================
	memcpy( count, PWD_STA, ( sizeof( PWD_STA ) - 1 ) );
	count += sizeof( PWD_STA ) - 1;

	os_sprintf( count, "%s", &tmpFLASH[ PWD_STA_OFSET ] );
	count += strlen( &tmpFLASH[ PWD_STA_OFSET ] );
//================================================================
	memcpy( count, SSID_AP, ( sizeof( SSID_AP ) - 1 ) );
	count += sizeof( SSID_AP ) - 1;

	os_sprintf( count, "%s", &tmpFLASH[ SSID_AP_OFSET ] );
	count += strlen( &tmpFLASH[ SSID_AP_OFSET ] );
//================================================================
	memcpy( count, PWD_AP, ( sizeof( PWD_AP ) - 1 ) );
	count += sizeof( PWD_AP ) - 1;

	os_sprintf( count, "%s", &tmpFLASH[ PWD_AP_OFSET ] );
	count += strlen( &tmpFLASH[ PWD_AP_OFSET ] );
//================================================================
	memcpy( count, MAX_TPW, ( sizeof( MAX_TPW ) - 1 ) );
	count += sizeof( MAX_TPW ) - 1;

	os_sprintf( count, "%s", &tmpFLASH[ MAX_TPW_VALUE_OFSET ] );
	count += strlen( &tmpFLASH[ MAX_TPW_VALUE_OFSET ] );
//================================================================
	memcpy( count, BOADCAST_PORT, ( sizeof( BOADCAST_PORT ) - 1 ) );
	count += sizeof( BOADCAST_PORT ) - 1;

	os_sprintf( count, "%s", &tmpFLASH[ DEF_UDP_PORT_OFSET ] );
	count += strlen( &tmpFLASH[ DEF_UDP_PORT_OFSET ] );
//================================================================
//================================================================
	memcpy( count, OUTPUTS, ( sizeof( OUTPUTS ) - 1 ) );
	broadcastShift = count;
	count += sizeof( OUTPUTS ) - 1;
//================================================================
	memcpy( count, GPIO_1, ( sizeof( GPIO_1 ) - 1 ) );
	count += sizeof( GPIO_1 ) - 1;

	if ( 0 == strcmp( &tmpFLASH[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_IMPULSE_MODE ) ) {

		memcpy( count, GPIO_OUT_IMPULSE_MODE, ( sizeof( GPIO_OUT_IMPULSE_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_IMPULSE_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmpFLASH[ GPIO_OUT_1_DELEY_OFSET ] );
		count += strlen( &tmpFLASH[ GPIO_OUT_1_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;

	} else if ( 0 == strcmp( &tmpFLASH[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) ) {

		memcpy( count, GPIO_OUT_TRIGGER_MODE, ( sizeof( GPIO_OUT_TRIGGER_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_TRIGGER_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmpFLASH[ GPIO_OUT_1_DELEY_OFSET ] );
		count += strlen( &tmpFLASH[ GPIO_OUT_1_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	} else if ( 0 == strcmp( &tmpFLASH[GPIO_OUT_1_MODE_OFSET], GPIO_OUT_COMBINE_MODE ) ) {

		memcpy( count, GPIO_OUT_COMBINE_MODE, ( sizeof( GPIO_OUT_COMBINE_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_COMBINE_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmpFLASH[ GPIO_OUT_1_DELEY_OFSET ] );
		count += strlen( &tmpFLASH[ GPIO_OUT_1_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	}
//================================================================
	memcpy( count, GPIO_2, ( sizeof( GPIO_2 ) - 1 ) );
	count += sizeof( GPIO_2 ) - 1;

	if ( 0 == strcmp( &tmpFLASH[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_IMPULSE_MODE ) ) {

		memcpy( count, GPIO_OUT_IMPULSE_MODE, ( sizeof( GPIO_OUT_IMPULSE_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_IMPULSE_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmpFLASH[ GPIO_OUT_2_DELEY_OFSET ] );
		count += strlen( &tmpFLASH[ GPIO_OUT_2_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	} else if ( 0 == strcmp( &tmpFLASH[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_TRIGGER_MODE ) ) {

		memcpy( count, GPIO_OUT_TRIGGER_MODE, ( sizeof( GPIO_OUT_TRIGGER_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_TRIGGER_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmpFLASH[ GPIO_OUT_2_DELEY_OFSET ] );
		count += strlen( &tmpFLASH[ GPIO_OUT_2_DELEY_OFSET ] );

		memcpy( count, MS, ( sizeof( MS ) - 1 ) );
		count += sizeof( MS ) - 1;
	} else if ( 0 == strcmp( &tmpFLASH[GPIO_OUT_2_MODE_OFSET], GPIO_OUT_COMBINE_MODE ) ) {

		memcpy( count, GPIO_OUT_COMBINE_MODE, ( sizeof( GPIO_OUT_COMBINE_MODE ) - 1 ) );
		count += sizeof( GPIO_OUT_COMBINE_MODE ) - 1;

		memcpy( count, DELAY_NAME, ( sizeof( DELAY_NAME ) - 1 ) );
		count += sizeof( DELAY_NAME ) - 1;

		os_sprintf( count, "%s", &tmpFLASH[ GPIO_OUT_2_DELEY_OFSET ] );
		count += strlen( &tmpFLASH[ GPIO_OUT_2_DELEY_OFSET ] );

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


void ICACHE_FLASH_ATTR
comandParser( void ) {

	int i = 0;

	if ( 0 == strcmp( tmp, TCP_REQUEST ) ) { 																	//+

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
	    	    	memcpy( tmp, TCP_QUERY, sizeof(TCP_QUERY) );
	    	    	tmp[ sizeof(TCP_QUERY) ] = ' ';
	    	    	memcpy( &tmp[ sizeof(TCP_QUERY) + 1 ], TCP_OPERATION_FAIL, ( sizeof(TCP_OPERATION_FAIL) ) );
	    	    	tmp[ sizeof(TCP_QUERY) + 1 + sizeof(TCP_OPERATION_FAIL) ] = '\r';
	    	    	tmp[ sizeof(TCP_QUERY) + 1 + sizeof(TCP_OPERATION_FAIL) + 1 ] = '\n';

	    	    	if ( NULL != pespconn ) {

	    	    		espconn_send( pespconn, tmp, ( sizeof(TCP_QUERY) + 1 + sizeof(TCP_OPERATION_FAIL) + 1 + 1 ) );
	    	    	}

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
	    	broadcast.proto.udp->remote_port = StringToInt( &tmp[ sizeof(TCP_SET_UDP_PORT) + 1 ] );
	    	brodcastSSA.proto.udp->remote_port = broadcast.proto.udp->remote_port;
	    	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    			    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
	    	}
	    	tcpRespounseBuilder( TCP_OPERATION_OK );

	    } else if ( 0 == strcmp( tmp, TCP_SET_IP ) ) {															//+

	    	writeFlash( DEF_IP_SOFT_AP_OFSET, &tmp[ sizeof(TCP_SET_IP) + 1 ] );
	    	tcpRespounseBuilder( TCP_OPERATION_OK );
	    	system_restart();

	    } else if ( 0 == strcmp( tmp, TCP_CLEAR_DB ) ) {														//+

	    	writeFlash( CLEAR_DB_STATUS_OFSET, CLEAR_DB_STATUS );
	    	tcpRespounseBuilder( TCP_OPERATION_OK );
			system_restart();

	    } else if ( 0 == strcmp( tmp, TCP_SSID_STA ) ) { 														//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_SSID_STA) + 1 ],  SSID_MAX_LENGHT ) ) {

	    		writeFlash( SSID_STA_OFSET, &tmp[ sizeof(TCP_SSID_STA) + 1 ] );

	    		os_sprintf( routerSSID, "%s", &tmp[ sizeof(TCP_SSID_STA) + 1 ] );
#ifdef DEBUG
	    os_printf( "routerSSID %s ", routerSSID );
#endif
		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
		}
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    		//system_restart();
	    		/*while (1) {
	   		    ets_wdt_disable();
	    		   	system_soft_wdt_stop();
	    		}*/
	    		initWIFI();
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_PWD_STA ) ) {															//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_PWD_STA) + 1 ],  PWD_MAX_LENGHT ) ) {

	    		writeFlash( PWD_STA_OFSET, &tmp[ sizeof(TCP_PWD_STA) + 1 ] );

	    		os_sprintf( routerPWD, "%s", &tmp[ sizeof(TCP_PWD_STA) + 1 ] );
#ifdef DEBUG
	    		os_printf( "routerPWD %s ", routerPWD );
#endif
	    		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
	    		}
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    		//system_restart();
	    		/*while (1) {
	    		    ets_wdt_disable();
	    		   	system_soft_wdt_stop();
	    		}*/
	    		initWIFI();
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_SSID_AP ) ) {												   			//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_SSID_AP) + 1 ],  SSID_MAX_LENGHT ) ) {

	    		writeFlash( SSID_AP_OFSET, &tmp[ sizeof(TCP_SSID_AP) + 1 ] );

	    		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
	    		}
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    		initWIFI();
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_PWD_AP ) ) {															//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_PWD_AP) + 1 ],  PWD_MAX_LENGHT ) ) {

	    		writeFlash( PWD_AP_OFSET, &tmp[ sizeof(TCP_PWD_AP) + 1 ] );

	    		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
	    		}
	    		tcpRespounseBuilder( TCP_OPERATION_OK );
	    		initWIFI();
	    	} else {

	    		tcpRespounseBuilder( TCP_OPERATION_FAIL );
	    	}

	    } else if ( 0 == strcmp( tmp, TCP_BROADCAST_NAME ) ) {													//+

	    	if ( LENGHT_OK == compareLenght( &tmp[ sizeof(TCP_BROADCAST_NAME) + 1 ],  BROADCAST_NAME_MAX_LENGHT ) ) {

	    		writeFlash( BROADCAST_NAME_OFSET, &tmp[ sizeof(TCP_BROADCAST_NAME) + 1 ] );
	    		if ( 0 != strcmp( &tmp[ sizeof(TCP_BROADCAST_NAME) + 1 ],  wifi_station_get_hostname( ) ) ) {

	    			//wifi_station_set_hostname( &tmp[ sizeof(TCP_BROADCAST_NAME) + 1 ] );
	    		}

	    		if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
	    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
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
	    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
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
		    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
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
		    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
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
			    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
			    	}


	    	} else { 																								      // error

		    	memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
		    	tmp[ sizeof(TCP_ERROR) ] = '\r';
		    	tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';

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
		    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
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
		    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
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
		    					                                                  (uint32 *)tmpFLASH, SPI_FLASH_SEC_SIZE ) ) {
		    	}

	    	} else { // error
		    	memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
		    	tmp[ sizeof(TCP_ERROR) ] = '\r';
		    	tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';

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
	    	if ( NULL != pespconn ) {

	    		espconn_send( pespconn, tmp, ( sizeof( TCP_ERROR ) + 2 ) );
	    	}

	 }
}


void ICACHE_FLASH_ATTR
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
			os_printf("tcp answer ");
			for ( a = 0; a < i; a++) {
				uart_tx_one_char(tmp[a]);
			}
		}
#endif

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

	if ( NULL != pespconn ) {

		espconn_send( pespconn, tmp, i );
	}
}













