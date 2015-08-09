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


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//#include "ip_addr.h"


#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"


#include "gpio.h"
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



void task1(void *pvParameters)
{

	uint32_t i;
	int state = 0;
    while (1) {

    	state ^=1;
    	GPIO_OUTPUT_SET(LED_GPIO, state);
    	for (i = 0xfffff; i > 0; i--){

    	}

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
	const char password[] = "123456789";
	struct station_config stationConf;

	wifi_station_set_auto_connect(1);
	wifi_set_opmode( STATION_MODE );
	memcpy(&stationConf.ssid, ssid, sizeof(ssid));
	memcpy(&stationConf.password, password, sizeof(password));
	wifi_station_set_config(&stationConf);
	wifi_station_connect();


	vTaskSuspend( task3 );
	while(1);

}

void task2(void *pvParameters)
{
	uint32_t i;
	int state = 0;
    while (1) {
    		for (i = 0xfffff; i > 0; i--){}
    		state ^=1;
    		GPIO_OUTPUT_SET(LED_GPIO1, state);
    }
}


void ICACHE_FLASH_ATTR
user_init(void)
{
	PIN_FUNC_SELECT(LED_GPIO_MUX, LED_GPIO_FUNC);
	PIN_FUNC_SELECT(LED_GPIO_MUX1, LED_GPIO_FUNC1);




    xTaskCreate(task2, "tsk2", 256, NULL, 2, NULL);
    xTaskCreate(task1, "tsk1", 256, NULL, 2, NULL);
    xTaskCreate(task3, "tsk3", 0, NULL, 2, NULL);
    xTaskCreate(task4, "tsk4", 0, NULL, 2, NULL);



}
