#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "user_config.h"
#include "espconn.h"
#define WIFI_CLIENTSSID		"TSM_Guest"
#define WIFI_CLIENTPASSWORD	"tsmguest"

extern int ets_uart_printf(const char *fmt, ...);
int (*console_printf)(const char *fmt, ...) = ets_uart_printf;
struct espconn espconnServer;
esp_tcp tcpServer;
extern void uart_tx_one_char();

LOCAL struct  espconn espconnBroadcastSTA;
LOCAL esp_udp espudpBroadcastSTA;
LOCAL uint8_t brodcastMessage[] = "Hello World!\r\n";

uint8_t data[] = "GET / HTTP/1.1\r\nHost: gcc.gnu.org\r\n\r\n";
LOCAL struct espconn espconnClientTCP;
LOCAL esp_tcp clientEsp_TCP;

LOCAL struct  espconn espconnBroadcastAP;
LOCAL esp_udp espudpBroadcastAP;

LOCAL os_timer_t task_timer;

static void ICACHE_FLASH_ATTR
tcp_disnconcb( void *arg ) {

	ets_uart_printf (" |tcp_disnconcb| ");
}

static void ICACHE_FLASH_ATTR
tcp_sentcb( void *arg ) {

	 ets_uart_printf (" |tcp_sentcb| ");
}

static void ICACHE_FLASH_ATTR
tcp_reconcb( void *arg, sint8 err ) {

	 ets_uart_printf (" |tcp_reconcb| ");
}

 void ICACHE_FLASH_ATTR
tcp_recvcb( void *arg, char *pdata, unsigned short len ) { // data received

    uint16 i;

    for (i = 0; i < len; i++) {
        uart_tx_one_char(pdata[i]);
    }
}

 void ICACHE_FLASH_ATTR
 tcp_connectcb(void *arg) {

	 ets_uart_printf (" |tcp_connectcb| ");
}

 static void ICACHE_FLASH_ATTR
 tcpclient_sent_cb( void *arg ) {

 	 ets_uart_printf (" tcpclient_sent_cb ");
 }

 static void ICACHE_FLASH_ATTR
 tcpclient_recon_cb(void *arg, sint8 err) {

 	ets_uart_printf("tcpclient_recon_cb\r\n");
 	 uart_tx_one_char(err);
 }

 static void ICACHE_FLASH_ATTR
 tcpclient_connect_cb(void *arg) {
 	ets_uart_printf("tcpclient_connect_cb\r\n");
 	os_printf("printf %s", data);
 	espconn_send( arg, data, sizeof(data) );
//espconn_disconnect((struct espconn *)arg);
 }

 static void ICACHE_FLASH_ATTR
 tcpclient_recv_cb ( void *arg, char *pdata, unsigned short len ) {

	 uint16 i;
	  for (i = 0; i < len; i++) {
	     uart_tx_one_char(pdata[i]);
	 }
 }

 static void ICACHE_FLASH_ATTR
 tcpclient_discon_cb(void *arg) {

 	ets_uart_printf("tcpclient_discon_cb\r\n");
 }

 static void ICACHE_FLASH_ATTR senddata( void ) {
 	char info[150];
 	char tcpserverip[15];
 	espconnClientTCP.proto.tcp = &clientEsp_TCP;
 	espconnClientTCP.type = ESPCONN_TCP;
 	espconnClientTCP.state = ESPCONN_NONE;
 	os_sprintf(tcpserverip, "%s", "209.132.180.131");
 	uint32_t ip = ipaddr_addr(tcpserverip);
 	os_memcpy(espconnClientTCP.proto.tcp->remote_ip, &ip, 4);
 	espconnClientTCP.proto.tcp->local_port = espconn_port();
 	espconnClientTCP.proto.tcp->remote_port = 80;
 	espconn_regist_connectcb(&espconnClientTCP, tcpclient_connect_cb);
 	espconn_regist_reconcb(&espconnClientTCP, tcpclient_recon_cb);
 	espconn_regist_disconcb(&espconnClientTCP, tcpclient_discon_cb);
 	espconn_regist_recvcb(&espconnClientTCP, tcpclient_recv_cb);
 	espconn_regist_sentcb(&espconnServer, tcpclient_sent_cb);
 	sint8 espcon_status = espconn_connect(&espconnClientTCP);
 #ifdef PLATFORM_DEBUG
 	switch( espcon_status )
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
 #endif
 }

 LOCAL void ICACHE_FLASH_ATTR
 mScheduler(void) {

 	os_timer_disarm(&task_timer);
 	struct station_info *station = wifi_softap_get_station_info();
 	LOCAL struct ip_info inf;

	while ( station ) {

#ifdef DEBUG
	os_printf( "bssid : %x:%x:%x:%x:%x:%x ip : %d.%d.%d.%d ", MAC2STR( station->bssid ), IP2STR( &station->ip ) );
#endif
		espconnBroadcastAP.type = ESPCONN_UDP;
		espconnBroadcastAP.state = ESPCONN_NONE;
		espconnBroadcastAP.proto.udp = &espudpBroadcastAP;
	    espconnBroadcastAP.proto.udp->remote_port =  9876 ;
	    IP4_ADDR( (ip_addr_t *)espconnBroadcastAP.proto.udp->remote_ip, (uint8_t)(station->ip.addr), \
	    		(uint8_t)(station->ip.addr >> 8),\
				(uint8_t)(station->ip.addr >> 16), (uint8_t)(station->ip.addr >> 24) );
		espconn_create( &espconnBroadcastAP );
		espconn_send( &espconnBroadcastAP, brodcastMessage, strlen( brodcastMessage ) );
		espconn_delete(&espconnBroadcastAP);

		station = STAILQ_NEXT(station, next);
	}
	wifi_softap_free_station_info();

 	switch( wifi_station_get_connect_status() ) {
 			case STATION_GOT_IP:
 				ets_uart_printf( "WiFi connected\r\n " );
 				wifi_get_ip_info( STATION_IF, &inf );
 				os_printf( "ip %u : %d.%d.%d.%d ", (uint32)(inf.ip.addr), IP2STR( &(inf.ip.addr) ) );
 				espconnBroadcastSTA.type = ESPCONN_UDP;
 				espconnBroadcastSTA.state = ESPCONN_NONE;
 				espconnBroadcastSTA.proto.udp = &espudpBroadcastSTA;
 				espconnBroadcastSTA.proto.udp->remote_port =  9876;
 				IP4_ADDR((ip_addr_t *)espconnBroadcastSTA.proto.udp->remote_ip, (uint8_t)(inf.ip.addr), \
 						(uint8_t)(inf.ip.addr >> 8),
                         (uint8_t)(inf.ip.addr >> 16), 255);
 				espconn_create( &espconnBroadcastSTA );
 				espconn_send( &espconnBroadcastSTA, brodcastMessage, strlen( brodcastMessage ) );
 				senddata();
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

 	espconn_delete( &espconnBroadcastSTA );
 	os_timer_setfn( &task_timer, (os_timer_func_t *)mScheduler, (void *)0 );
 	os_timer_arm( &task_timer, 3000, 0 );
 }

 static void ICACHE_FLASH_ATTR
 user_rf_pre_init(void){

}

 void ICACHE_FLASH_ATTR
 user_init() {

	struct softap_config apConfig;
	struct ip_info ipinfo;
	struct station_config stconfig;

	struct station_config stationConfig;

	system_restore();

	wifi_set_opmode(STATIONAP_MODE);

	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_install_putc1(uart_tx_one_char);

	ets_uart_printf("ESP8266 platform starting...\r\n");

	wifi_station_disconnect();
	wifi_station_dhcpc_stop();

	if(wifi_station_get_config(&stconfig))
	{
		os_memset(stconfig.ssid, 0, sizeof(stconfig.ssid));
		os_memset(stconfig.password, 0, sizeof(stconfig.password));
		os_sprintf(stconfig.ssid, "%s", WIFI_CLIENTSSID);
		os_sprintf(stconfig.password, "%s", WIFI_CLIENTPASSWORD);
		if(!wifi_station_set_config(&stconfig)) {

			ets_uart_printf("ESP8266 not set station config!\r\n");
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
	ipinfo.ip.addr = ipaddr_addr("93.158.134.3");
	//ipinfo.gw.addr = ipinfo.ip.addr;  //шлюз
	IP4_ADDR( &ipinfo.netmask, 255, 255, 255, 0 );
	wifi_set_ip_info( SOFTAP_IF, &ipinfo );

	wifi_softap_set_config(&apConfig);

	wifi_softap_dhcps_start();

	espconnServer.type = ESPCONN_TCP;
	espconnServer.state = ESPCONN_NONE;
	espconnServer.proto.tcp = &tcpServer;
	espconnServer.proto.tcp->local_port = 6766;

	espconn_regist_recvcb(&espconnServer, tcp_recvcb);          // data received
	espconn_regist_connectcb(&espconnServer, tcp_connectcb);    // TCP connected successfully
	espconn_regist_disconcb(&espconnServer, tcp_disnconcb);     // TCP disconnected successfully
	espconn_regist_sentcb(&espconnServer, tcp_sentcb);          // data sent
	espconn_regist_reconcb(&espconnServer, tcp_reconcb);        // error, or TCP disconnected
	espconn_accept(&espconnServer);
	espconn_regist_time(&espconnServer, 20, 0);

	os_timer_disarm(&task_timer);
	os_timer_setfn(&task_timer, (os_timer_func_t *)mScheduler, (void *)0);
	os_timer_arm(&task_timer, 1000, 0);
}

