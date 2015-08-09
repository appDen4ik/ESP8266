#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"

#include "espconn.h"
#include "mem.h"
#include "gpio.h"
#include "user_config.h"
#include "driver/uart.h"


LOCAL uint8_t /*ICACHE_FLASH_ATTR*/  StringToChar(uint8_t *data);
LOCAL uint8_t * /*ICACHE_FLASH_ATTR */ ShortIntToString(uint16_t data, uint8_t *adressDestenation);


LOCAL uint16_t server_timeover = 180;
LOCAL struct espconn masterconn;

LOCAL struct espconn sendResponse;
LOCAL esp_udp udp;

struct ip_info inf;


// see eagle_soc.h for these definitions
#define LED_GPIO 2
#define LED_GPIO_MUX PERIPHS_IO_MUX_GPIO2_U
#define LED_GPIO_FUNC FUNC_GPIO2

#define DELAY 100 /* milliseconds */



LOCAL os_timer_t blink_timer;



/*
LOCAL void ICACHE_FLASH_ATTR
shell_tcp_disconcb(void *arg) {
    struct espconn *pespconn = (struct espconn *) arg;

    os_printf("tcp connection disconnected\n");
}*/

LOCAL void ICACHE_FLASH_ATTR
tcp_recvcb(void *arg, char *pdata, unsigned short len)
{
	int i = 0;
    struct espconn *pespconn = (struct espconn *) arg;
    ets_uart_printf("Hello World!\r\n");
//    uart0_tx_buffer(pdata, len);
	for ( ; i++ < len; ){
		uart_tx_one_char(*pdata++);
	}
        // espconn_sent(pespconn, pusrdata, length); //echo

}
/*
LOCAL void ICACHE_FLASH_ATTR
tcpserver_connectcb(void *arg)
{
	GPIO_OUTPUT_SET(LED_GPIO, 1);
    struct espconn *pespconn = (struct espconn *)arg;

    os_printf("tcp connection established\n");

    espconn_regist_recvcb(pespconn, shell_tcp_recvcb);
    // espconn_regist_reconcb(pespconn, tcpserver_recon_cb);
    espconn_regist_disconcb(pespconn, shell_tcp_disconcb);
    // espconn_regist_sentcb(pespconn, tcpclient_sent_cb);
}*/


void ICACHE_FLASH_ATTR
sendDatagram(char *datagram, uint16 size) {

		//посмотреть что там с портами

		//выделить бродкаст айпишку
	if ( wifi_station_get_connect_status() == STATION_CONNECTING ) {

		wifi_get_ip_info( STATION_IF, &inf );

		IP4_ADDR((ip_addr_t *)sendResponse.proto.udp->remote_ip, 192, 168, 0, 255);

		espconn_create(&sendResponse);
 	 	espconn_sent(&sendResponse, "hi123", 5);
 	 	espconn_delete(&sendResponse);
	}
}

void ICACHE_FLASH_ATTR
user_init(void)
{
	LOCAL struct espconn conn1;
	LOCAL esp_tcp tcp1;

	const char ssid[] = "DIR-320";
	const char password[] = "123456789";
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

	PIN_FUNC_SELECT(LED_GPIO_MUX, LED_GPIO_FUNC);

//-----------------------------------------------------------------
	{ //tcp
		masterconn.type = ESPCONN_TCP;
		masterconn.state = ESPCONN_NONE;
		masterconn.proto.tcp = &tcp1;
		masterconn.proto.tcp->local_port = 2300;
		espconn_accept(&masterconn);
		espconn_regist_time(&masterconn, server_timeover, 0);
		espconn_regist_recvcb(&masterconn, tcp_recvcb);
	}

    { //udp
    	sendResponse.type = ESPCONN_UDP;
    	sendResponse.state = ESPCONN_NONE;
    	sendResponse.proto.udp = &udp;
    	sendResponse.proto.udp->remote_port = 9876;
    }

	// Set up a timer to blink the LED
	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&blink_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&blink_timer, (os_timer_func_t *)sendDatagram, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&blink_timer, DELAY, 1);

}

// ѕеревод числа в последовательность ASCII
uint8_t * ShortIntToString(uint16_t data, uint8_t *adressDestenation) {
	uint8_t *startAdressDestenation = adressDestenation;
	uint8_t *endAdressDestenation;
	uint8_t buff;

	do {// перевод входного значени€ в последовательность ASCII кодов
		// в обратном пор€дке
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

// ѕеревод последовательности ASCII в число
uint8_t StringToChar(uint8_t *data) {
	uint8_t returnedValue = 0;
	for (;*data >= '0' && *data <= '9'; data++)
	returnedValue = 10 * returnedValue + (*data - '0');
	return returnedValue;
}


