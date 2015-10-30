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

static void wifi_check_ip(void *arg);


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



static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	struct ip_info ipConfig;
	os_timer_disarm(&WiFiLinker);
	switch(wifi_station_get_connect_status())
	{
		case STATION_GOT_IP:
			wifi_get_ip_info(STATION_IF, &ipConfig);
			if(ipConfig.ip.addr != 0) {
				ets_uart_printf("WiFi connected\r\n");
				console_printf( "ip : %d.%d.%d.%d ", IP2STR(&ipConfig.ip) );
			}
			break;
		case STATION_WRONG_PASSWORD:

			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting error, wrong password\r\n");
			#endif
			break;
		case STATION_NO_AP_FOUND:

			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting error, ap not found\r\n");
			#endif
			break;
		case STATION_CONNECT_FAIL:

			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting fail\r\n");
			#endif
			break;
		default:

			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting...\r\n");
			#endif
	}
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 1000, 0);
}



void user_rf_pre_init(void)
{
}

//Init function
void ICACHE_FLASH_ATTR user_init()
{

	struct softap_config apConfig;
	struct ip_info ipinfo;
	struct station_config stconfig;

	struct station_config stationConfig;

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

	os_timer_disarm(&WiFiLinker);
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 1000, 0);

}

