/*
	The hello world demo
*/

#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include "driver/uart.h"
#include "user_interface.h"
#include "espconn.h"


#define DELAY 5000 /* milliseconds */


extern void uart_tx_one_char();


LOCAL esp_tcp tcpServer;
LOCAL struct espconn espconnServer;

LOCAL struct  espconn espconnBroadcastAP;
LOCAL esp_udp espudpBroadcastAP;

LOCAL struct  espconn espconnBroadcastSTA;
LOCAL esp_udp espudpBroadcastSTA;

LOCAL uint8_t brodcastMessage[] = "Hello World!\r\n";

LOCAL os_timer_t task_timer;

typedef void (* ping_recv_function)(void* arg, void *pdata);
typedef void (* ping_sent_function)(void* arg, void *pdata);

struct ping_option{
	uint32 count;
	uint32 ip;
	uint32 coarse_time;
	ping_recv_function recv_function;
	ping_sent_function sent_function;
	void* reverse;
};

struct ping_resp{
	uint32 total_count;
	uint32 resp_time;
	uint32 seqno;
	uint32 timeout_count;
	uint32 bytes;
	uint32 total_bytes;
	uint32 total_time;
	sint8  ping_err;
};

LOCAL struct ping_option ping_opt;


LOCAL void ICACHE_FLASH_ATTR hello_cb(void *arg) {
	ets_uart_printf("Hello World!\r\n");
}


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

void pingResv (void* arg, void *pdata) {


 	os_printf("pingResv  ");

 	os_printf(" ping_resp -> total_count %d, ", ((struct ping_resp *)pdata )->total_count);
 	os_printf(" ping_resp -> resp_time %d, ", ((struct ping_resp *)pdata )->resp_time);
 	os_printf(" ping_resp -> seqno %d, ", ((struct ping_resp *)pdata )->seqno);
 	os_printf(" ping_resp -> timeout_count %d, ", ((struct ping_resp *)pdata )->timeout_count);
 	os_printf(" ping_resp -> bytes %d, ", ((struct ping_resp *)pdata )->bytes);
 	os_printf(" ping_resp -> total_bytes %d, ", ((struct ping_resp *)pdata )->total_bytes);
 	os_printf(" ping_resp -> total_time %d, ", ((struct ping_resp *)pdata )->total_time);
 	os_printf(" ping_resp -> ping_err %d, ", ((struct ping_resp *)pdata )->ping_err);


 	os_printf(" ping_option -> count %d, ", ((struct ping_option *)arg )->count);
 	os_printf(" ping_option -> ip %d, ", ((struct ping_option *)arg )->ip);
 	os_printf(" ping_option -> coarse_time %d, ", ((struct ping_option *)arg )->coarse_time);
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
 		espconn_sent( &espconnBroadcastAP, brodcastMessage, strlen( brodcastMessage ) );
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
  			  	ping_opt.ip = inf.ip.addr;
  				//ping_opt.ip = 0x2700a8c0;
  			  	ping_opt.coarse_time = 2000;
  			  	os_printf("ping_start(struct ping_option *ping_opt) = %d", ping_start(&ping_opt));
  				espconn_create( &espconnBroadcastSTA );
  				espconn_sent( &espconnBroadcastSTA, brodcastMessage, strlen( brodcastMessage ) );
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
  	os_timer_arm( &task_timer, DELAY, 0 );
  }

 void user_rf_pre_init(void) {

 }



void user_init(void) {
	// Configure the UART
	struct softap_config apConfig;
	struct ip_info ipinfo;
	struct station_config stconfig;

	struct station_config stationConfig;


	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_install_putc1(uart_tx_one_char);
	os_delay_us(10000);
	//os_printf("system_get_rst_info() = %d", system_get_rst_info()->reason);
	ets_uart_printf("ESP8266 platform starting...\r\n");

	os_printf("ping_regist_recv() = %d ", ping_regist_recv(&ping_opt, pingResv) );

	wifi_set_opmode(STATIONAP_MODE);

	wifi_station_disconnect();
	wifi_station_dhcpc_stop();

	if(wifi_station_get_config(&stconfig))
	{
		os_memset(stconfig.ssid, 0, sizeof(stconfig.ssid));
		os_memset(stconfig.password, 0, sizeof(stconfig.password));
		os_sprintf(stconfig.ssid, "%s", "SomeHomeNetwork");
		os_sprintf(stconfig.password, "%s", "a053516626");
		if(!wifi_station_set_config(&stconfig)) {

			ets_uart_printf("ESP8266 not set station config!\r\n");
		}
	}
	wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);


	if(wifi_station_get_config(&stationConfig)) {
		os_printf("STA config: SSID: %s, PASSWORD: %s\r\n",
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
	os_timer_arm(&task_timer, DELAY, 0);



}
