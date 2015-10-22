#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"

#include "espconn.h"
#include "mem.h"
#include "gpio.h"
#include "user_config.h"



LOCAL struct espconn sendResponse;
LOCAL esp_udp udp;


// see eagle_soc.h for these definitions
#define LED_GPIO 2
#define LED_GPIO_MUX PERIPHS_IO_MUX_GPIO2_U
#define LED_GPIO_FUNC FUNC_GPIO2

#define DELAY 100 /* milliseconds */


void setup_wifi_ap_mode(void)
{
//	wifi_set_opmode((wifi_get_opmode()|STATIONAP_MODE)&USE_WIFI_MODE);
	struct softap_config apconfig;
	if(wifi_softap_get_config(&apconfig))
	{
		wifi_softap_dhcps_stop();
		os_memset(apconfig.ssid, 0, sizeof(apconfig.ssid));
		os_memset(apconfig.password, 0, sizeof(apconfig.password));
//		apconfig.ssid_len = os_sprintf(apconfig.ssid, WIFI_AP_NAME);
//		os_sprintf(apconfig.password, "%s", WIFI_AP_PASSWORD);
		apconfig.authmode = AUTH_WPA_WPA2_PSK;
		apconfig.ssid_hidden = 0;
		apconfig.channel = 7;
		apconfig.max_connection = 4;
		if(!wifi_softap_set_config(&apconfig))
		{
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("ESP8266 not set AP config!\r\n");
			#endif
		};
		struct ip_info ipinfo;
		wifi_get_ip_info(SOFTAP_IF, &ipinfo);
		IP4_ADDR(&ipinfo.ip, 192, 168, 4, 1);
		IP4_ADDR(&ipinfo.gw, 192, 168, 4, 1);
		IP4_ADDR(&ipinfo.netmask, 255, 255, 255, 0);
		wifi_set_ip_info(SOFTAP_IF, &ipinfo);
		wifi_softap_dhcps_start();
	}
	#ifdef PLATFORM_DEBUG
	ets_uart_printf("ESP8266 in AP mode configured.\r\n");
	#endif
}

void setup_wifi_st_mode(void)
{
//	wifi_set_opmode((wifi_get_opmode()|STATIONAP_MODE)&USE_WIFI_MODE);
	struct station_config stconfig;
	wifi_station_disconnect();
	wifi_station_dhcpc_stop();
	if(wifi_station_get_config(&stconfig))
	{
		os_memset(stconfig.ssid, 0, sizeof(stconfig.ssid));
		os_memset(stconfig.password, 0, sizeof(stconfig.password));
		os_sprintf(stconfig.ssid, "%s", "Default STA");
		os_sprintf(stconfig.password, "%s", "76543210");
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
	#ifdef PLATFORM_DEBUG
	ets_uart_printf("ESP8266 in STA mode configured.\r\n");
	#endif
}


LOCAL os_timer_t blink_timer;

void user_rf_pre_init(void)
{
}


void sendDatagram(char *datagram, uint16 size) {

		sendResponse.type = ESPCONN_UDP;
		sendResponse.state = ESPCONN_NONE;
		sendResponse.proto.udp = &udp;

		IP4_ADDR((ip_addr_t *)sendResponse.proto.udp->remote_ip, 10, 10, 10, 255);

		sendResponse.proto.udp->remote_port = 9876; // Remote port

		espconn_create(&sendResponse);
 	 	 espconn_sent(&sendResponse, "hi123", 5);
 	 	 espconn_delete(&sendResponse);
}

void ICACHE_FLASH_ATTR
user_init(void)
{

	struct softap_config apConfig;
	struct ip_info ipinfo;
	char ssid[33] = "TEST";
	char password[33] = "123456789";





//	system_restore();

	ets_wdt_enable();
	ets_wdt_disable();

	wifi_set_opmode( STATIONAP_MODE );



	wifi_softap_dhcps_stop();

	IP4_ADDR(&ipinfo.ip, 10, 10, 10, 1);
	IP4_ADDR(&ipinfo.gw, 10, 10, 10, 1);
	IP4_ADDR(&ipinfo.netmask, 255, 255, 255, 0);
	wifi_set_ip_info(SOFTAP_IF, &ipinfo);

	wifi_softap_dhcps_start();

	wifi_softap_get_config(&apConfig);

	os_memset(apConfig.ssid, 0, sizeof(apConfig.ssid));
	os_memcpy(apConfig.ssid, ssid, os_strlen(ssid));

	os_memset(apConfig.password, 0, sizeof(apConfig.password));
	os_memcpy(apConfig.password, password, os_strlen(password));

	apConfig.authmode = AUTH_WPA_WPA2_PSK;
	apConfig.channel = 7;
	apConfig.max_connection = 255;
	apConfig.ssid_hidden = 0;


	wifi_softap_set_config(&apConfig);



	struct station_config stationConf;

	wifi_station_disconnect();
	wifi_station_dhcpc_stop();
	wifi_station_get_config(&stationConf);

	os_memset(stationConf.ssid, 0, sizeof(stationConf.ssid));
	os_memset(stationConf.password, 0, sizeof(stationConf.password));
	os_sprintf(stationConf.ssid,     "%s" ,"Default STA" );
	os_sprintf(stationConf.password, "%s", "76543210" );
//	os_sprintf(stationConf.ssid, "%s", "Default STA");
//	os_sprintf(stationConf.password, "%s", "76543210");
	wifi_station_set_config(&stationConf);

	wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);


//	struct station_config stconfig;

/*	wifi_station_disconnect();
	wifi_station_dhcpc_stop();
	wifi_station_get_config(&stationConf);

		os_memset(stationConf.ssid, 0, sizeof(stationConf.ssid));
		os_memset(stationConf.password, 0, sizeof(stationConf.password));
		os_sprintf(stationConf.ssid, "%s", "Default STA");
		os_sprintf(stationConf.password, "%s", "76543210");
		wifi_station_set_config(&stationConf);

	wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);

//	PIN_FUNC_SELECT(LED_GPIO_MUX, LED_GPIO_FUNC);



	// Set up a timer to blink the LED
	// os_timer_disarm(ETSTimer *ptimer)
//	os_timer_disarm(&blink_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
//	os_timer_setfn(&blink_timer, (os_timer_func_t *)sendDatagram, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
//	os_timer_arm(&blink_timer, DELAY, 1);


*/
}
