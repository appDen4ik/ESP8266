/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/12/1, v1.0 create this file.
*******************************************************************************/
#include "esp_common.h"

#include <esp8266/ets_sys.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espconn.h"

#include "uart.h"
//#include "ip_addr.h"


#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"


//#include "espconn.h"

void task4(void *pvParameters);
void task3(void *pvParameters);
void task2(void *pvParameters);
void task1(void *pvParameters);


#define LED_GPIO 2
#define LED_GPIO_MUX PERIPHS_IO_MUX_GPIO2_U
#define LED_GPIO_FUNC FUNC_GPIO2

#define LED_GPIO1 0
#define LED_GPIO_MUX1 PERIPHS_IO_MUX_GPIO0_U
#define LED_GPIO_FUNC1 FUNC_GPIO0
#define server_ip "192.168.0.255"
#define server_port 9876

extern void uart1_write_char(char);

static void ICACHE_FLASH_ATTR
at_tcpclient_discon_cb(void *arg) {
	struct espconn *pespconn = (struct espconn *)arg;
	free(pespconn->proto.tcp);
	free(pespconn);
	printf("Disconnect callback\r\n");
}

static void ICACHE_FLASH_ATTR
at_tcpclient_sent_cb(void *arg) {
	printf("Send callback\r\n");
	struct espconn *pespconn = (struct espconn *)arg;
	espconn_disconnect(pespconn);
}

static void ICACHE_FLASH_ATTR
at_tcpclient_connect_cb(void *arg)
{
	struct espconn *pespconn = (struct espconn *)arg;

	printf("TCP client connect\r\n");

	espconn_regist_sentcb(pespconn, at_tcpclient_sent_cb);
	espconn_regist_disconcb(pespconn, at_tcpclient_discon_cb);
	espconn_sent(pespconn, "ESP8266", strlen("ESP8266"));
}

static void ICACHE_FLASH_ATTR
senddata()
{

	struct espconn *pCon = (struct espconn *)zalloc(sizeof(struct espconn));

	pCon->type = ESPCONN_TCP;
	pCon->state = ESPCONN_NONE;
	uint32_t ip = ipaddr_addr("192.168.0.21");
	pCon->proto.tcp = (esp_tcp *)zalloc(sizeof(esp_tcp));
	pCon->proto.tcp->local_port = espconn_port();
	pCon->proto.tcp->remote_port = 6666;
	memcpy(pCon->proto.tcp->remote_ip, &ip, 4);
	espconn_regist_connectcb(pCon, at_tcpclient_connect_cb);
	printf("send %d", *(uint32 *)(&(pCon->proto.tcp->remote_ip)));
	espconn_connect(pCon);
}

void task1(void *pvParameters)
{

	uint32_t i;
	int state = 0;
    while (1) {

    	//printf("task1");
    	for (i = 0xfffffff; i > 0; i--){
    	/*	for (i = 0xff; i > 0; i--){

    		    	}
    	*/}
    	senddata();
    }
}


void task4(void *pvParameters){

	while (1) {

/*		int sta_socket;

		struct sockaddr_in remote_ip;

		sta_socket = socket(PF_LOCAL, SOCK_DGRAM, 0);


		bzero(&remote_ip, sizeof(struct sockaddr_in));
		remote_ip.sin_family = AF_INET;
		remote_ip.sin_addr.s_addr = inet_addr(server_ip);
		remote_ip.sin_port = htons(server_port);


		connect(sta_socket, (struct sockaddr *)(&remote_ip), sizeof(struct sockaddr));

		char *pbuf = (char *)zalloc(1024);

		sprintf(pbuf, "HELLO WORLD");

		write(sta_socket, pbuf, strlen(pbuf) + 1);

		closesocket(sta_socket);*/


	}

}


void task3(void *pvParameters){

	const char ssid[] = "DIR-320";
	const char password[] = "";
	struct station_config stationConf;

	wifi_station_set_auto_connect(1);
	wifi_set_opmode( STATIONAP_MODE );
	memcpy(&stationConf.ssid, ssid, sizeof(ssid));
	memcpy(&stationConf.password, password, sizeof(password));
	wifi_station_set_config(&stationConf);
	wifi_station_connect();



	while(1) {
		printf("Hello  ");
	}

}

void task2(void *pvParameters)
{
	uint32_t i;
	int state = 0;
    while (1) {
    		for (i = 0xffffff; i > 0; i--){}
    		printf("task2");
    		uart1_write_char('2');

    }
}


void ICACHE_FLASH_ATTR
user_init(void)
{
//	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	uart_init_new();
	printf("Hello  ");
	  wifi_set_opmode(STATIONAP_MODE);
#define DEMO_AP_SSID "DEMO_AP"
#define DEMO_AP_PASSWORD "12345678"

	  wifi_station_disconnect();
	  wifi_station_dhcpc_stop();
	  struct station_config *stationConf = (struct station_config *)zalloc(sizeof(struct station_config));
	  sprintf( stationConf->ssid, "%s", "DIR-320" );
	  sprintf( stationConf->password, "%s", "" );
	  wifi_station_set_config(stationConf);
	  wifi_station_connect();
	  wifi_station_dhcpc_start();

 struct softap_config *config = (struct softap_config *)zalloc(sizeof(struct softap_config));
 wifi_softap_get_config(config); // Get soft-AP config first.

 sprintf(config->ssid, DEMO_AP_SSID);
 sprintf(config->password, DEMO_AP_PASSWORD);

 config->authmode = AUTH_WPA_WPA2_PSK;
  config->ssid_len = 0; // or its actual SSID length
  config->max_connection = 4;
  wifi_softap_set_config(config); // Set ESP8266 soft-AP config
  free(config);
  	  xTaskCreate( task2, "Task 2", 1000, NULL, 1, NULL );
  	  xTaskCreate( task1, "Task 1", 1000, NULL, 1, NULL );
}
