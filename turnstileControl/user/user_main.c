/*
 **********************************************************************************************************************************
  * @file    user_main.c
  * @author  Denys
  * @version V0.0.1
  * @date    14-Nov-2015
  * @brief
 *********************************************************************************************************************************/

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "gpio.h"
#include "user_config.h"
#include "driver/uart.h"

extern void uart_tx_one_char();
extern void ets_wdt_restore();

LOCAL inline void ICACHE_FLASH_ATTR tcpRespounseBuilder( uint8_t *responseCode ) __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR comandParser( void ) __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR broadcastBuilder( void ) __attribute__((always_inline));
LOCAL inline res  ICACHE_FLASH_ATTR writeFlash( uint16_t where, uint8_t *what ) __attribute__((always_inline));
LOCAL inline void ICACHE_FLASH_ATTR UpdateCRCForPackage(uint8_t* pack, uint8_t length) __attribute__((always_inline));
LOCAL uint8_t * ICACHE_FLASH_ATTR ShortIntToString(uint32_t data, uint8_t *adressDestenation);
LOCAL void ICACHE_FLASH_ATTR UpdateCRC(uint8_t b);
//**********************************************************************************************************************************
LOCAL struct espconn espconnServer;
LOCAL esp_tcp tcpServer;
LOCAL tcp_stat tcpSt = TCP_FREE;
LOCAL struct espconn *pespconn;
LOCAL uint32_t ipAdd;
//**********************************************************************************************************************************
LOCAL struct  espconn espconnBroadcastAP;
LOCAL esp_udp espudpBroadcastAP;
LOCAL broadcast_stat broadcastStatus = BROADCAST_NEED;
LOCAL os_timer_t timerBroadcast;
//**********************************************************************************************************************************
LOCAL uint8_t flashTmp[SPI_FLASH_SEC_SIZE];
LOCAL uint8_t tmp[TMP_SIZE];
LOCAL uint8_t brodcastMessage[500];
//**********************************************************************************************************************************
LOCAL os_timer_t task_timer;
//**********************************************************************************************************************************
LOCAL user_status groupStatus = USER_FALSE;
LOCAL uint8_t groupCommandCounter = 0;
LOCAL uint8_t groupCommand;
//**********************************************************************************************************************************
LOCAL uint8_t numberTurnstiles = 0;
LOCAL uint16_t turnBroadcastStatuses[32];
LOCAL turnstileOperation currentOperation = TURNSTILE_STATUS;
LOCAL uint8_t currentTurnstileID = 1;
LOCAL uint8_t currentTurnstileCommandId = 1;
LOCAL uint8_t currentcommand;
LOCAL crc_stat crcStatus = CRC_OK;
LOCAL turnstile bufTurnstile;
LOCAL volatile uint8_t counterForBufTurn = 0;
//**********************************************************************************************************************************
LOCAL uint8_t CRC;
//**********************************************************************************************************************************
//tasks
LOCAL os_event_t *disconQueue;
#define DISCON_QUEUE_LENGHT  		1
#define DISCON_TASK_PRIO    		2
#define DISCON_ETS_SIGNAL_TOKEN	 	0xfa

LOCAL os_event_t *cmdPrsQueue;
#define CMD_PRS_QUEUE_LENGHT 				1
#define CMD_PRS_TASK_PRIO   				1
#define CMD_PRS_QUEUE_ETS_SIGNAL_TOKEN		0XF
#define CMD_PRS_QUEUE_ETS_PARAM_TOKEN		0XF

LOCAL os_event_t *turnstQueue;
#define TURNSTILE_QUEUE_LENGHT  						1
#define TURNSTILE_TASK_PRIO    							0
#define TURNSTILE_TASK_PRIO_QUEUE_ETS_SIGNAL_TOKEN		0XF
#define TURNSTILE_TASK_PRIO_QUEUE_ETS_PARAM_TOKEN		0XF
//**********************************************************************************************************************************

//**********************************************************Callbacks***************************************************************

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
		if ( len < TMP_SIZE ){

			memcpy( tmp, pdata, len );
		} else {

			memcpy( tmp, "overflow", sizeof("overflow") );
		}
		system_os_post( CMD_PRS_TASK_PRIO, (ETSSignal)pdata, (ETSParam)len );
	}
}


LOCAL void ICACHE_FLASH_ATTR
tcp_connectcb( void *arg ) { // TCP connected successfully

	struct espconn *conn = (struct espconn *) arg;

	os_printf( " |tcp_connectcb ip : %d.%d.%d.%d Connected\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );

	if ( TCP_BUSY == tcpSt && *(uint32 *)( conn->proto.tcp->remote_ip ) != ipAdd ) {

		os_printf( " |tcp_connectcb ip: busy..\r\n| " );
		system_os_post( DISCON_TASK_PRIO, DISCON_ETS_SIGNAL_TOKEN, (ETSParam)conn );
		os_printf( " |tcp_connectcb ip : %d.%d.%d.%d Connect busy...\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
	}
}


LOCAL void ICACHE_FLASH_ATTR
tcp_disnconcb( void *arg ) { // TCP disconnected successfully

	struct espconn *conn = (struct espconn *) arg;
	os_printf( " |tcp_disnconcb TCP disconnected successfully : %d.%d.%d.%d\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
}


LOCAL void ICACHE_FLASH_ATTR
tcp_reconcb( void *arg, sint8 err ) { // error, or TCP disconnected

	struct espconn *conn = (struct espconn *) arg;
	os_printf( " |tcp_reconcb TCP RECON : %d.%d.%d.%d\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
}


LOCAL void ICACHE_FLASH_ATTR
tcp_sentcb( void *arg ) { // data sent callback

	struct espconn *conn = (struct espconn *) arg;
	os_printf( " |tcp_sentcb DATA sent for ip : %d.%d.%d.%d\r\n| ",  IP2STR( conn->proto.tcp->remote_ip ) );
	system_os_post( DISCON_TASK_PRIO, DISCON_ETS_SIGNAL_TOKEN, (ETSParam)conn );
}

//***********************************************************Handlers***************************************************************

void uart0_rx_intr_handler( void *para ) {
    /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
     * uart1 and uart0 respectively
     */

    if ( UART_RXFIFO_FULL_INT_ST != (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST) ) {
        return;
    }

    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);

    while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {


        ((uint8_t *)( &bufTurnstile ))[ counterForBufTurn++ ] = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;

//      os_printf("| uart interrupt %d|", ((uint8_t *)( &bufTurnstile ))[ counterForBufTurn - 1 ] );
#ifdef DEBUG
 //       os_delay_us(1000);
		os_printf("uart interrupt %d, counterForBufTurn %d", ((uint8_t *)( &bufTurnstile ))[ counterForBufTurn - 1 ], \
				counterForBufTurn);
#endif
        if ( sizeof(turnstile) == counterForBufTurn ) {

        	counterForBufTurn = 0;
//        	os_printf(" | interrupt 1 counterForBufTurn = %d |", counterForBufTurn);

        	WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);//clr status
        	WRITE_PERI_REG(UART_INT_ENA(UART0), 0x0); //disable interput
        	os_timer_disarm(&task_timer);
#ifdef DEBUG
		os_delay_us(DELAY);
        	os_printf("uart0_rx_intr_handler intput response bufTurnstile.address %d, bufTurnstile.numberOfBytes %d, bufTurnstile.typeData %d, \
        				bufTurnstile.data %d, bufTurnstile.crc %d", bufTurnstile.address, bufTurnstile.numberOfBytes, bufTurnstile.typeData, \
        									  	  	  	  	  	  	bufTurnstile.data, bufTurnstile.crc );
		os_delay_us(DELAY);
#endif
        	UpdateCRCForPackage( ( (uint8_t *)&bufTurnstile ), ( sizeof(turnstile) - 1 ) );
#ifdef DEBUG
        	os_printf("crc %d", CRC);
#endif
        	if ( bufTurnstile.crc == CRC ) {

        		turnBroadcastStatuses[currentTurnstileID - 1] = bufTurnstile.data;
        		currentTurnstileID++;
        	} else {

        		if ( CRC_OK == crcStatus) {

        			os_printf( "crc error id: %d, try again...", currentTurnstileID );
        			crcStatus = CRC_ERROR;
        		} else if ( CRC_ERROR == crcStatus ) {

        			os_printf( "crc double error id: %d", currentTurnstileID );
        			turnBroadcastStatuses[currentTurnstileID - 1] = 257;
        			crcStatus = CRC_OK;
        			currentTurnstileID++;
        		}

        	}

//        	os_printf("| counterForBufTurn = %d |", counterForBufTurn);
        	system_os_post( TURNSTILE_TASK_PRIO, TURNSTILE_TASK_PRIO_QUEUE_ETS_SIGNAL_TOKEN, \
        													TURNSTILE_TASK_PRIO_QUEUE_ETS_PARAM_TOKEN );
        }
    }
}

//************************************************************Tasks*****************************************************************

LOCAL void ICACHE_FLASH_ATTR
timeout( void ) {

	os_printf( "timeout, turnstile not response id: %d", currentTurnstileID );
	os_timer_disarm(&task_timer);
	WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);
	WRITE_PERI_REG(UART_INT_ENA(UART0), 0x0); //disable interput
	turnBroadcastStatuses[currentTurnstileID - 1] = 256;
	currentTurnstileID++;
	system_os_post( TURNSTILE_TASK_PRIO, TURNSTILE_TASK_PRIO_QUEUE_ETS_SIGNAL_TOKEN, \
	        													TURNSTILE_TASK_PRIO_QUEUE_ETS_PARAM_TOKEN );
}


LOCAL void ICACHE_FLASH_ATTR
broadcastTimeout( void ) {

	os_timer_disarm(&timerBroadcast);
	broadcastStatus = BROADCAST_NEED;
}


LOCAL void ICACHE_FLASH_ATTR
discon( os_event_t *e ) {

#ifdef DEBUG
		os_printf( " |tcp discon cb\r\n| ");
#endif

	if ( NULL != ( struct espconn * )( e->par ) ) {

		espconn_disconnect( ( struct espconn * )( e->par ) );
	}
}


LOCAL void ICACHE_FLASH_ATTR
cmdPars( os_event_t *e ) {

//#ifdef DEBUG
	os_printf(" cmdPars (char *)(e->sig)  %d,  (unsigned short)(e->par)  %d", (char *)(e->sig), (unsigned short)(e->par) );
	{
		uint32_t i;
		for ( i = 0; i < (uint16_t)(e->par); i++) {

			uart1_tx_one_char( tmp[i] );
		}
	}
//#endif

	comandParser();
	ipAdd = 0;
	pespconn = NULL;
}


LOCAL void ICACHE_FLASH_ATTR
turnstileHandler( os_event_t *e ) {

	os_delay_us(10000); // таймаут между запросами
//	system_soft_wdt_feed();
//	ets_wdt_restore();
	if ( currentTurnstileID > numberTurnstiles ) {

		currentTurnstileID = 1;
	}

	if ( BROADCAST_NEED == broadcastStatus ) {

		broadcastStatus = BROADCAST_NOT_NEED;

		struct station_info *station = wifi_softap_get_station_info();

		broadcastBuilder();

#ifdef DEBUG
		os_delay_us(DELAY);
		os_printf( "%s ", brodcastMessage );
		os_delay_us(DELAY);
#endif

		while ( station ) {

			os_printf( "bssid : %x:%x:%x:%x:%x:%x ip : %d.%d.%d.%d ", MAC2STR( station->bssid ), IP2STR( &station->ip ) );
    		espconnBroadcastAP.type = ESPCONN_UDP;
    		espconnBroadcastAP.state = ESPCONN_NONE;
    		espconnBroadcastAP.proto.udp = &espudpBroadcastAP;
		    espconnBroadcastAP.proto.udp->remote_port = atoi( DEF_UDP_PORT );
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
					break;
				case ESPCONN_ARG:
					os_printf( " |esp soft AP  broadcast illegal argument| " );
					system_restart();
					break;
				case ESPCONN_IF:
					os_printf( " |esp soft AP  broadcast send UDP fail| " );
					system_restart();
					break;
				case ESPCONN_MAXNUM:
					os_printf( " |esp soft AP  broadcast buffer of sending data is full| " );
					system_restart();
					break;
			}

			espconn_delete(&espconnBroadcastAP);
			station = STAILQ_NEXT(station, next);
		}

		wifi_softap_free_station_info();

		os_timer_disarm( &timerBroadcast );
		os_timer_setfn( &timerBroadcast, (os_timer_func_t *)broadcastTimeout, (void *)0 );
		os_timer_arm( &timerBroadcast, BROADCAST_TIMER, 0 );
	}

	if ( TURNSTILE_STATUS == currentOperation ) {

		bufTurnstile.address 		= currentTurnstileID;
		bufTurnstile.numberOfBytes 	= 0x02;
		bufTurnstile.typeData 		= 0xAA;
		bufTurnstile.data 			= 0;
		UpdateCRCForPackage( ( (uint8_t *)&bufTurnstile ), ( sizeof(turnstile) - 1 ) );
		bufTurnstile.crc 			= CRC;

	} else if ( TURNSTILE_COMMAND == currentOperation ) {

		currentOperation = TURNSTILE_STATUS;
		currentTurnstileID = currentTurnstileCommandId;

		bufTurnstile.address 		= currentTurnstileID;
		bufTurnstile.numberOfBytes 	= 0x02;
		bufTurnstile.typeData 		= 0xcc;
		bufTurnstile.data 			= currentcommand;
		UpdateCRCForPackage( ( (uint8_t *)&bufTurnstile ), ( sizeof(turnstile) - 1 ) );
		bufTurnstile.crc 			= CRC;
	}

	uart_tx_one_char( bufTurnstile.address );
	uart_tx_one_char( bufTurnstile.numberOfBytes );
	uart_tx_one_char( bufTurnstile.typeData );
	uart_tx_one_char( bufTurnstile.data );
	uart_tx_one_char( bufTurnstile.crc );

#ifdef DEBUG
	os_delay_us(DELAY*10);
	os_printf("turnstileHandler output request bufTurnstile.address %d, bufTurnstile.numberOfBytes %d, bufTurnstile.typeData %d, \
			bufTurnstile.data %d, bufTurnstile.crc %d", bufTurnstile.address, bufTurnstile.numberOfBytes, bufTurnstile.typeData, \
								  	  	  	  	  	  	bufTurnstile.data, bufTurnstile.crcl;
	os_delay_us(DELAY*10);
#endif


//	os_printf("turnstileHandler counterForBufTurn = %d", counterForBufTurn);

	counterForBufTurn = 0;

   //включаем уарт
   //clear all interrupt
   WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);
   //enable rx_interrupt
   SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_RXFIFO_FULL_INT_ENA);

   //включаем таймер
   os_timer_disarm(&task_timer);
   os_timer_setfn( &task_timer, (os_timer_func_t *)timeout, (void *)0 );
   os_timer_arm( &task_timer, DELAY_TIMER, 0 );
}

//**********************************************************************************************************************************

void ICACHE_FLASH_ATTR
user_rf_pre_init( void ) {

}


LOCAL void ICACHE_FLASH_ATTR
init_done( void ) {

	struct softap_config *softapConf = (struct softap_config *)os_zalloc(sizeof(struct softap_config));
	struct ip_info ipinfo;

	system_restore();

	if ( SOFTAP_MODE != wifi_get_opmode() ) {

		wifi_set_opmode( SOFTAP_MODE );
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
																			(uint32 *)flashTmp, SPI_FLASH_SEC_SIZE ) ) {

		os_printf( " | initWIFI Read fail! |\r\n" );
		system_restart();
		while(1);
	}

	wifi_softap_dhcps_stop();

	softapConf->ssid_len = os_sprintf( softapConf->ssid, "%s", AP_NAME );
	os_printf( " softapConf->ssid  %s, softapConf->ssid_len  %d\r\n", softapConf->ssid, softapConf->ssid_len );

	os_sprintf( softapConf->password, "%s", "76543210" );
	os_printf( " softapConf->password  %s\r\n", softapConf->password );

	softapConf->channel = atoi( &flashTmp[CHANEL_AP_OFSET] );
	os_printf( " softapConf->channel  %d\r\n", softapConf->channel );

	softapConf->authmode = AUTH_WPA_WPA2_PSK;
	os_printf( " softapConf->authmode  %d\r\n", softapConf->authmode );

	softapConf->max_connection = 4;
	os_printf( " softapConf->max_connection  %d\r\n", softapConf->max_connection );

	softapConf->ssid_hidden = 0;
	os_printf( " softapConf->ssid_hidden  %d\r\n", softapConf->ssid_hidden );

	softapConf->beacon_interval = 100;
	os_printf( " softapConf->beacon_interval  %d\r\n", softapConf->beacon_interval );

	if( !wifi_softap_set_config( softapConf ) ) {

		os_printf(" | initWIFI: module not set AP config! |\r\n");
	}

	os_free( softapConf );

	if ( wifi_get_ip_info(SOFTAP_IF, &ipinfo ) ) {

		ipinfo.ip.addr = ipaddr_addr( IP_AP );
		ipinfo.gw.addr = ipinfo.ip.addr;  //шлюз
		IP4_ADDR( &ipinfo.netmask, 255, 255, 255, 0 );

		wifi_set_ip_info( SOFTAP_IF, &ipinfo );

	} else {

		os_printf( " | initWIFI: read ip ap fail! |\r\n" );
	}
	os_printf( " ipinfo.ip.addr  %d.%d.%d.%d ", IP2STR( &(ipinfo.ip.addr) ) );

	wifi_softap_dhcps_start();

   	ets_wdt_init();
//  ets_wdt_disable();
   	ets_wdt_enable();
   	ets_wdt_restore();
   	system_soft_wdt_stop();
    system_soft_wdt_restart();
    system_soft_wdt_feed();

	system_os_post( TURNSTILE_TASK_PRIO, TURNSTILE_TASK_PRIO_QUEUE_ETS_SIGNAL_TOKEN, \
												TURNSTILE_TASK_PRIO_QUEUE_ETS_PARAM_TOKEN );
}


void ICACHE_FLASH_ATTR
user_init( void ) {

	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM );

	os_install_putc1( (void *)uart1_tx_one_char );
	if ( SYS_CPU_160MHZ != system_get_cpu_freq() ) {

		system_update_cpu_freq( SYS_CPU_160MHZ );
	}

	os_printf( " sdk version: %s \r\n", system_get_sdk_version() );

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                                                                  (uint32 *)flashTmp, SPI_FLASH_SEC_SIZE ) ) {

		os_printf( " | user_init: Read USER_SECTOR_IN_FLASH_MEM fail! |\r\n" );
		system_restart();
		while(1);
	}

	if ( 0 != strcmp( flashTmp, FLASH_READY ) ) {

		uint16_t i;

		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM ) ) {

			os_printf( " | user_init: clearSectorsDB() fail! |\r\n" );
			system_restart();
			while(1);
		}

		for ( i = 0; i < SPI_FLASH_SEC_SIZE; i++ ) {

			flashTmp[i] = 0xff;
		}

		memcpy( &flashTmp[FLASH_READY_OFSET], FLASH_READY, sizeof(FLASH_READY) );
		flashTmp[ sizeof(FLASH_READY) ] = '\n';

		memcpy( &flashTmp[CHANEL_AP_OFSET], DEF_CHANEL_AP, sizeof(DEF_CHANEL_AP) );
		flashTmp[ sizeof(DEF_CHANEL_AP) + CHANEL_AP_OFSET ] = '\n';

		flashTmp[NUMBER_OF_TURNSTILES_OFSET] = NUMBER_OF_TURNSTILE_DEFAULT;
		flashTmp[ 1 + NUMBER_OF_TURNSTILES_OFSET ] = '\n';

		if ( SPI_FLASH_RESULT_OK != spi_flash_write( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
																						(uint32 *)flashTmp, SPI_FLASH_SEC_SIZE ) ) {

			os_printf(" | loadDefParam: write default param fail! |\r\n");
			while(1);
		}

		system_restore();
	}

	{

	uint16_t c, currentSector;

	for ( currentSector = USER_SECTOR_IN_FLASH_MEM; currentSector <= USER_SECTOR_IN_FLASH_MEM; currentSector++ ) {
			os_printf( " currentSector %d\r\n", currentSector);
			spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)flashTmp, SPI_FLASH_SEC_SIZE );
			for ( c = 0; SPI_FLASH_SEC_SIZE > c; c++ ) {

				uart_tx_one_char(flashTmp[c]);
			}

			system_soft_wdt_stop();
		}
	}

	numberTurnstiles = flashTmp[NUMBER_OF_TURNSTILES_OFSET];

	{
		uint8_t i;
		for ( i = 0; i < numberTurnstiles; i++ ) {

			turnBroadcastStatuses[i] = 1000;
		}
	}

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

	if( wifi_station_get_auto_connect() == 0 ) {

		wifi_station_set_auto_connect(1);
	}

	if ( 5 != espconn_tcp_get_max_con() ) {

    	espconn_tcp_set_max_con(5);
    }

	if( wifi_get_phy_mode() != PHY_MODE_11N ) {

		wifi_set_phy_mode( PHY_MODE_11N );
	}

	disconQueue = (os_event_t *)os_malloc( sizeof(os_event_t)*DISCON_QUEUE_LENGHT );
	cmdPrsQueue = (os_event_t *)os_malloc( sizeof(os_event_t)*CMD_PRS_QUEUE_LENGHT );
	turnstQueue = (os_event_t *)os_malloc( sizeof(os_event_t)*TURNSTILE_QUEUE_LENGHT );

	system_os_task( discon, DISCON_TASK_PRIO, disconQueue, DISCON_QUEUE_LENGHT );
	system_os_task( cmdPars, CMD_PRS_TASK_PRIO, cmdPrsQueue, CMD_PRS_QUEUE_LENGHT );
	system_os_task( turnstileHandler, TURNSTILE_TASK_PRIO, turnstQueue, TURNSTILE_QUEUE_LENGHT );


	system_init_done_cb(init_done);
}


LOCAL res ICACHE_FLASH_ATTR
writeFlash( uint16_t where, uint8_t *what ) {

	uint16_t i;

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
				                                     (uint32 *)flashTmp, SPI_FLASH_SEC_SIZE ) ) {

		return ERROR;
	}

	for ( i = where ; '\n' != flashTmp[ i ]; i++ ) {

		flashTmp[ i ] = 0xff;
	}

	flashTmp[ i ] = 0xff;

	for (  i = 0; '\0' != what[ i ]; i++, where++ ) {

		flashTmp[ where ] = what[ i ];
	}

	flashTmp[ where++ ] = '\0';
	flashTmp[ where ] = '\n';

	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( USER_SECTOR_IN_FLASH_MEM ) ) {

		return ERROR;
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_write( USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE, \
			                                                                       (uint32 *)flashTmp, SPI_FLASH_SEC_SIZE ) ) {

		return ERROR;
	}

	return DONE;
}


LOCAL void ICACHE_FLASH_ATTR
broadcastBuilder( void ) {

	uint8_t *count;
	uint8_t i;
	count = brodcastMessage;

//=================================================================================================
	memcpy( count, AP_NAME, ( sizeof( AP_NAME ) - 1 ) );
	count += sizeof( AP_NAME ) - 1;
//=================================================================================================
/*	memcpy( count, BROADCAST_IP_AP, ( sizeof( BROADCAST_IP_AP ) - 1 ) );
	count += sizeof( BROADCAST_IP_AP ) - 1;

	os_sprintf( count, "%s", IP_AP );
	count += strlen( IP_AP );
*/
//=================================================================================================
	memcpy( count, BROADCAST_COUNTER_TURNSTILE, ( sizeof( BROADCAST_COUNTER_TURNSTILE ) - 1 ) );
	count += sizeof( BROADCAST_COUNTER_TURNSTILE ) - 1;

	count = ShortIntToString( numberTurnstiles, count );
//=================================================================================================

	for ( i = 0; i < numberTurnstiles; i++ ) {

		memcpy( count, BROADCAST_ID, ( sizeof( BROADCAST_ID ) - 1 ) );
		count += sizeof( BROADCAST_ID ) - 1;

		count = ShortIntToString( ( i + 1 ), count );

		memcpy( count, BROADCAST_STATUS, ( sizeof( BROADCAST_STATUS ) - 1 ) );
		count += sizeof( BROADCAST_STATUS ) - 1;

		count = ShortIntToString( turnBroadcastStatuses[i], count );
	}

	*count++ = '\r';
	*count++ = '\n';
	*count = '\0';
}


LOCAL void ICACHE_FLASH_ATTR
comandParser( void ) {

	if ( 0 == strcmp( tmp, TCP_SET_TURNSTILE ) ) {

		//set\0 id\0 command\0\r\n
		uint8_t step;

		currentTurnstileCommandId = atoi( &tmp[ sizeof(TCP_SET_TURNSTILE) + 1 + sizeof(TCP_ID) + 1 ] );

		if ( 9 > currentTurnstileCommandId ) {

			step = 1;
		} else if ( 99 > currentTurnstileCommandId ) {

			step = 2;
		} else {

			step = 3;
		}

		currentcommand = atoi( &tmp[ sizeof(TCP_SET_TURNSTILE) + 1 + sizeof(TCP_ID) + 1 + step + 1 + sizeof(TCP_COMMAND) + 1 ] );

#ifdef DEBUG
		 os_printf( " | comandParser currentTurnstileCommandId %d, currentcommand %d | ", currentTurnstileCommandId, currentcommand );
#endif
		tcpRespounseBuilder( TCP_OPERATION_OK );
		currentOperation = TURNSTILE_COMMAND;

	} else if ( 0 == strcmp( tmp, TCP_SET_CHANEL ) ) {

		uint8_t chanel = atoi( &tmp[ sizeof(TCP_SET_CHANEL) + 1 ] );
#ifdef DEBUG
		 os_printf( " | comandParser channel %d | ", chanel );
#endif
		if ( 0 < chanel && 14 > chanel) {

			writeFlash( CHANEL_AP_OFSET, &tmp[ sizeof(TCP_SET_CHANEL) + 1 ] );
			tcpRespounseBuilder( TCP_OPERATION_OK );
			system_restart();
		} else {

			tcpRespounseBuilder( TCP_OPERATION_FAIL );
		}

	} else if ( 0 == strcmp( tmp, TCP_GROUP_ACTION ) ) {



	} else if ( 0 == strcmp( tmp, TCP_NUMBER_TURN ) ) {

		uint8_t numb = atoi( &tmp[ sizeof(TCP_NUMBER_TURN) + 1 ] );
#ifdef DEBUG
		os_printf( " | comandParser channel %d | ", chanel );
#endif
				if ( 1 < numb && 32 > numb ) {

					writeFlash( NUMBER_OF_TURNSTILES_OFSET, &tmp[ sizeof(TCP_NUMBER_TURN) + 1 ] );
					tcpRespounseBuilder( TCP_OPERATION_OK );
					numberTurnstiles = numb;
				} else {

					tcpRespounseBuilder( TCP_OPERATION_FAIL );
				}

	} else { // ERROR

		 memcpy( tmp, TCP_ERROR, ( sizeof(TCP_ERROR) ) );
		 tmp[ sizeof(TCP_ERROR) ] = '\r';
		 tmp[ sizeof(TCP_ERROR) + 1 ] = '\n';
#ifdef DEBUG
		 os_printf(" | comandParser can't recognize command | ");
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

//#ifdef DEBUG
	{
			uint16_t a;
			os_printf(" tcp answer ");
			for ( a = 0; a < i; a++) {
				uart1_tx_one_char(tmp[a]);
			}
		}
//#endif
	tcpSt = TCP_FREE;
	if ( NULL != pespconn ) {

		espconn_send( pespconn, tmp, i );
	}
}


void ICACHE_FLASH_ATTR
UpdateCRCForPackage(uint8_t *pack, uint8_t length){
	CRC = 0;
	uint8_t counter = 0;
	for ( ;counter++ < length; UpdateCRC((*pack++)) );
}


void ICACHE_FLASH_ATTR
UpdateCRC(uint8_t b){
	uint8_t i;
	for (i = 8; i > 0; i--){
		if ((b ^ CRC) & 1)
		CRC = ((CRC ^ 0x18) >> 1) | 0x80;
		else
		CRC >>= 1;
		b >>= 1;
	}
}


LOCAL uint8_t * ICACHE_FLASH_ATTR
ShortIntToString( uint32_t data, uint8_t *adressDestenation ) {

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

















