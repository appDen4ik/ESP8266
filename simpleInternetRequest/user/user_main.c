#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"

#include "espconn.h"
#include "gpio.h"
#include "user_config.h"
#include "driver/uart.h"



LOCAL uint16_t server_timeover = 180;


LOCAL struct espconn espconnServerTCP;
LOCAL esp_tcp serverEsp_TCP;


LOCAL struct espconn espconnClientTCP;
LOCAL esp_tcp clientEsp_TCP;

#define DELAY 10000 /* milliseconds */



LOCAL os_timer_t blink_timer;

void user_rf_pre_init(void)
{
}

LOCAL void ICACHE_FLASH_ATTR
tcp_recvcb(void *arg, char *pdata, unsigned short len)
{
	int i = 0;
	for ( ; i++ < len; ){
		uart_tx_one_char(*pdata++);
	}
}

void ICACHE_FLASH_ATTR
tcpclient_connect_cb(void *arg){

}

void ICACHE_FLASH_ATTR
sendRequest(char *datagram, uint16 size) {
	 	espconn_delete(&espconnClientTCP);
	 	ets_uart_printf("sending request...\r\n");
		espconn_create(&espconnClientTCP);
 	 	espconn_sent(&espconnClientTCP, "Test", strlen("Test"));
}




void ICACHE_FLASH_ATTR
user_init(void)
{

	const char ssid[] = "TSM_Guest";//"DIR-320";
	const char password[] = "tsmguest";
	struct station_config stationConf;

//	ets_wdt_enable();
//	ets_wdt_disable();

	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	wifi_station_set_auto_connect(1);

	wifi_set_opmode( STATION_MODE );
	memcpy(&stationConf.ssid, ssid, sizeof(ssid));
	memcpy(&stationConf.password, password, sizeof(password));
	wifi_station_set_config(&stationConf);
	wifi_station_connect();

//-----------------------------------------------------------------

	{ //tcp client
			espconnClientTCP.type = ESPCONN_TCP;
			espconnClientTCP.state = ESPCONN_NONE;
			espconnClientTCP.proto.tcp = &clientEsp_TCP;
			espconnClientTCP.proto.tcp->remote_port = 80;
			espconnClientTCP.proto.tcp->local_port = espconn_port();
			IP4_ADDR((ip_addr_t *)espconnClientTCP.proto.udp->remote_ip, 10, 9, 11, 200);
//			espconnClientTCP.recv_callback = tcp_recvcb;

			espconn_regist_connectcb(&espconnClientTCP, tcpclient_connect_cb);
//	 	 	espconn_sent(&espconnClientTCP, "Test", strlen("Test"));
	 	 	espconn_connect(&espconnClientTCP);

	 	 	ets_uart_printf("request created");

	//		espconn_accept(&espconnClientTCP);
//			espconn_regist_recvcb(&espconnClientTCP, tcp_recvcb);

	}

	// Set up a timer to blink the LED
	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&blink_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&blink_timer, (os_timer_func_t *)sendRequest, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&blink_timer, DELAY, 1);

}


