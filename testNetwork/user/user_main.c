#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "user_config.h"
#include "espconn.h"


extern int ets_uart_printf(const char *fmt, ...);
int (*console_printf)(const char *fmt, ...) = ets_uart_printf;
static ETSTimer WiFiLinker;
struct espconn espconnServer;
esp_tcp tcpServer;
extern void uart_tx_one_char();

uint8_t writeFlashTmp[4096];

LOCAL struct espconn espconnClientTCP;
LOCAL esp_tcp clientEsp_TCP;


static void wifi_check_ip(void *arg);
LOCAL os_timer_t task_timer;

void tcp_disnconcb( void *arg ) {

	ets_uart_printf (" |tcp_disnconcb| ");
}

void tcp_sentcb( void *arg ) {

	 ets_uart_printf (" |tcp_sentcb| ");
}

void tcp_reconcb( void *arg, sint8 err ) {

	 ets_uart_printf (" |tcp_reconcb| ");
}

 void ICACHE_FLASH_ATTR
tcp_recvcb( void *arg, char *pdata, unsigned short len ) { // data received

    uint16 i;

    for (i = 0; i < len; i++) {
        uart_tx_one_char(pdata[i]);
    }
}

 void ICACHE_FLASH_ATTR tcp_connectcb(void *arg) {

	 ets_uart_printf (" |tcp_connectcb| ");
}


 static void ICACHE_FLASH_ATTR tcpclient_recon_cb(void *arg, sint8 err)
 {
 	ets_uart_printf("tcpclient_recon_cb\r\n");
 	 uart_tx_one_char(err);
 }

 void ICACHE_FLASH_ATTR tcpclient_connect_cb(void *arg){
 	ets_uart_printf("tcpclient_connect_cb\r\n");
 	espconn_disconnect((struct espconn *)arg);
 }

 static void ICACHE_FLASH_ATTR tcpclient_discon_cb(void *arg)
 {
 	ets_uart_printf("tcpclient_discon_cb\r\n");
 }

 static void ICACHE_FLASH_ATTR senddata( void )
 {
 	char info[150];
 	char tcpserverip[15];
 	espconnClientTCP.proto.tcp = &clientEsp_TCP;
 	espconnClientTCP.type = ESPCONN_TCP;
 	espconnClientTCP.state = ESPCONN_NONE;
 	os_sprintf(tcpserverip, "%s", "212.42.83.53");
 	uint32_t ip = ipaddr_addr(tcpserverip);
 	os_memcpy(espconnClientTCP.proto.tcp->remote_ip, &ip, 4);
 	espconnClientTCP.proto.tcp->local_port = espconn_port();
 	espconnClientTCP.proto.tcp->remote_port = 80;
 	espconn_regist_connectcb(&espconnClientTCP, tcpclient_connect_cb);
 	espconn_regist_reconcb(&espconnClientTCP, tcpclient_recon_cb);
 	espconn_regist_disconcb(&espconnClientTCP, tcpclient_discon_cb);
 /*	#ifdef PLATFORM_DEBUG
 	console_printf("Start espconn_connect to " IPSTR ":%d\r\n", IP2STR(espconnClientTCP.proto.tcp->remote_ip), espconnClientTCP.proto.tcp->remote_port);
 	#endif
 	#ifdef LWIP_DEBUG
 	console_printf("LWIP_DEBUG: Start espconn_connect local port %u\r\n", Conn.proto.tcp->local_port);
 	#endif*/
 	sint8 espcon_status = espconn_connect(&espconnClientTCP);
 /*	#ifdef PLATFORM_DEBUG
 	switch(espcon_status)
 	{
 		case ESPCONN_OK:
 			console_printf("TCP created.\r\n");
 			break;
 		case ESPCONN_RTE:
 			console_printf("Error connection, routing problem.\r\n");
 			break;
 		case ESPCONN_TIMEOUT:
 			console_printf("Error connection, timeout.\r\n");
 			break;
 		default:
 			console_printf("Connection error: %d\r\n", espcon_status);
 	}
 	#endif*/

 }


 LOCAL void ICACHE_FLASH_ATTR
 mScheduler(void) {

 	os_timer_disarm(&task_timer);

 	//wifi_station_dhcpc_stop();
 	//wifi_station_dhcpc_start();

 		switch( wifi_station_get_connect_status() ) {
 			case STATION_GOT_IP:
 				ets_uart_printf( "WiFi connected\r\n " );
 				espconn_delete(&espconnServer);
 				espconn_regist_recvcb(&espconnServer, tcp_recvcb);          // data received
 				espconn_regist_connectcb(&espconnServer, tcp_connectcb);    // TCP connected successfully
 				espconn_regist_disconcb(&espconnServer, tcp_disnconcb);     // TCP disconnected successfully
 				espconn_regist_sentcb(&espconnServer, tcp_sentcb);          // data sent
 				espconn_regist_reconcb(&espconnServer, tcp_reconcb);        // error, or TCP disconnected
 				espconn_accept(&espconnServer);
 				espconn_regist_time(&espconnServer, 20, 0);
 				//senddata();
 				break;
 			case STATION_WRONG_PASSWORD:
 				ets_uart_printf( "WiFi connecting error, wrong password\r\n" );
 				break;
 			case STATION_NO_AP_FOUND:
 				ets_uart_printf("WiFi connecting error, ap not found\r\n" );
 				break;
 			case STATION_CONNECT_FAIL:

 				ets_uart_printf( "WiFi connecting fail\r\n" );
 				break;
 			default:
 				ets_uart_printf( "WiFi connecting...\r\n " );
 				break;
 		}


 	os_timer_setfn( &task_timer, (os_timer_func_t *)mScheduler, (void *)0 );
 	os_timer_arm( &task_timer, 300, 0 );
 }



void user_rf_pre_init(void)
{
}

//Init function
void ICACHE_FLASH_ATTR user_init()
{

/*	struct softap_config apConfig;
	struct ip_info ipinfo;
	struct station_config stconfig;

	struct station_config stationConfig;

	system_restore();

	wifi_set_opmode(STATIONAP_MODE);

	os_install_putc1(uart_tx_one_char);

	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_delay_us(1000);

	ets_uart_printf("ESP8266 platform starting...\r\n");

	wifi_station_disconnect();
	wifi_station_dhcpc_stop();

	if(wifi_station_get_config(&stconfig))
	{
		os_memset(stconfig.ssid, 0, sizeof(stconfig.ssid));
		os_memset(stconfig.password, 0, sizeof(stconfig.password));
		os_sprintf(stconfig.ssid, "%s", WIFI_CLIENTSSID);
		os_sprintf(stconfig.password, "%s", WIFI_CLIENTPASSWORD);
		if(!wifi_station_set_config(&stconfig))
		{
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("ESP8266 not set station config!\r\n");
			#endif
		}
	}
	wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);


	if(wifi_station_get_config(&stationConfig)) {
		console_printf("STA config: SSID: %s, PASSWORD: %s\r\n",
			stationConfig.ssid,
			stationConfig.password);
	}



	wifi_softap_dhcps_stop();
	os_sprintf(apConfig.ssid, "%s", "test");
	os_sprintf(apConfig.password, "%s", "76543210");
	apConfig.ssid_len = strlen("test");
	apConfig.authmode = AUTH_WPA_WPA2_PSK;
	apConfig.channel = 7;
	apConfig.max_connection = 4;
	apConfig.ssid_hidden = 0;

	wifi_get_ip_info(SOFTAP_IF, &ipinfo );
	ipinfo.ip.addr = ipaddr_addr("172.18.4.1");
	//ipinfo.gw.addr = ipinfo.ip.addr;  //шлюз
	IP4_ADDR( &ipinfo.netmask, 255, 255, 255, 0 );
	wifi_set_ip_info( SOFTAP_IF, &ipinfo );

	wifi_softap_set_config(&apConfig);

	wifi_softap_dhcps_start();

	espconnServer.type = ESPCONN_TCP;
	espconnServer.state = ESPCONN_NONE;
	espconnServer.proto.tcp = &tcpServer;
	espconnServer.proto.tcp->local_port = 6766;

	//espconn_regist_time(&espconnServer, 60, 0);
	espconn_regist_recvcb(&espconnServer, tcp_recvcb);          // data received
	espconn_regist_connectcb(&espconnServer, tcp_connectcb);    // TCP connected successfully
	espconn_regist_disconcb(&espconnServer, tcp_disnconcb);     // TCP disconnected successfully
	espconn_regist_sentcb(&espconnServer, tcp_sentcb);          // data sent
	espconn_regist_reconcb(&espconnServer, tcp_reconcb);        // error, or TCP disconnected
	espconn_accept(&espconnServer);
	espconn_regist_time(&espconnServer, 20, 0);
	// Wait for Wi-Fi connection and start TCP connection


	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&task_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&task_timer, (os_timer_func_t *)mScheduler, (void *)0);
	 //void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&task_timer, 200, 0);*/

	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_install_putc1(uart_tx_one_char);
	os_printf( " sdk version: %s \r\n", system_get_sdk_version() );
	os_printf(" spi_flash_erase_sector( 63 ) %s ", spi_flash_erase_sector( 63 ) ? "false" : "true" );
	os_printf(" spi_flash_erase_sector( 62 ) %s ", spi_flash_erase_sector( 62 ) ? "false" : "true" );
	os_printf(" spi_flash_erase_sector( 61 ) %s ", spi_flash_erase_sector( 61 ) ? "false" : "true" );
	os_printf(" spi_flash_erase_sector( 60 ) %s ", spi_flash_erase_sector( 60 ) ? "false" : "true" );

	{

	uint16_t c, currentSector;

	for ( currentSector = 60; currentSector <= 63; currentSector++ ) {
			os_printf( " currentSector %d\r\n", currentSector);
			spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE );
			for ( c = 0; SPI_FLASH_SEC_SIZE > c; c++ ) {

				uart_tx_one_char(writeFlashTmp[c]);
			}

			system_soft_wdt_stop();
		}
	}
	os_printf(" spi_flash_erase_sector( 125 ) %s ", spi_flash_erase_sector( 125 ) ? "false" : "true" );
	os_printf(" spi_flash_erase_sector( 124 ) %s ", spi_flash_erase_sector( 124 ) ? "false" : "true" );
	os_printf(" spi_flash_erase_sector( 123 ) %s ", spi_flash_erase_sector( 123 ) ? "false" : "true" );
	os_printf(" spi_flash_erase_sector( 122 ) %s ", spi_flash_erase_sector( 122 ) ? "false" : "true" );

	{

	uint16_t c, currentSector;

	for ( currentSector = 122; currentSector <= 125; currentSector++ ) {
			os_printf( " currentSector %d\r\n", currentSector);
			spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector, (uint32 *)writeFlashTmp, SPI_FLASH_SEC_SIZE );
			for ( c = 0; SPI_FLASH_SEC_SIZE > c; c++ ) {

				uart_tx_one_char(writeFlashTmp[c]);
			}

			system_soft_wdt_stop();
		}
	}
	system_restore();
	system_restart();
}

