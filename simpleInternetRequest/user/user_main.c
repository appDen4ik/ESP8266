/*
 * В данном примере я хотел бы реализовать простейший
 * get запрос какой нибудь страницы из интернета с
 * дальнейшей ее передачей в уарт.
 * Замечание. Стриница должна быть на http сервере
 *
 */


#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"

#include "espconn.h"
#include "gpio.h"
#include "user_config.h"
#include "driver/uart.h"

static void ICACHE_FLASH_ATTR senddata( void );

//LOCAL uint16_t server_timeover = 180;


//LOCAL struct espconn espconnServerTCP;
//LOCAL esp_tcp serverEsp_TCP;


LOCAL struct espconn espconnClientTCP;
LOCAL esp_tcp clientEsp_TCP;


LOCAL os_timer_t WiFiLinker;

void user_rf_pre_init(void)
{
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

static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	struct ip_info ipConfig;
	os_timer_disarm(&WiFiLinker);
	switch(wifi_station_get_connect_status())
	{
		case STATION_GOT_IP:
			wifi_get_ip_info(STATION_IF, &ipConfig);
			if(ipConfig.ip.addr != 0) {
//				connState = WIFI_CONNECTED;
				#ifdef PLATFORM_DEBUG
				ets_uart_printf("WiFi connected\r\n");
				ets_uart_printf("Start TCP connecting...\r\n");
				#endif
//				connState = TCP_CONNECTING;
				senddata();
				return;
			}
			break;
		case STATION_WRONG_PASSWORD:
//			connState = WIFI_CONNECTING_ERROR;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting error, wrong password\r\n");
			#endif
			break;
		case STATION_NO_AP_FOUND:
//			connState = WIFI_CONNECTING_ERROR;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting error, ap not found\r\n");
			#endif
			break;
		case STATION_CONNECT_FAIL:
//			connState = WIFI_CONNECTING_ERROR;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting fail\r\n");
			#endif
			break;
		default:
//			connState = WIFI_CONNECTING;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting...\r\n");
			#endif
	}
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 1000, 0);
}

static void ICACHE_FLASH_ATTR senddata( void )
{
	os_timer_disarm(&WiFiLinker);
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

	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 20000, 0);

}






void ICACHE_FLASH_ATTR
user_init(void)
{

	const char ssid[] = "TEST"; /*//"TSM_Guest";//"DIR-320";*/
	const char password[] = "123456789";//"tsmguest";
	struct station_config stationConf;


	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	wifi_station_disconnect();
	wifi_station_dhcpc_stop();

	wifi_set_opmode( STATION_MODE );
	memcpy(&stationConf.ssid, ssid, sizeof(ssid));
	memcpy(&stationConf.password, password, sizeof(password));
	wifi_station_set_config(&stationConf);

	wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);

	os_timer_disarm(&WiFiLinker);
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, (void *)0);
	os_timer_arm(&WiFiLinker, 1000, 0);

}



