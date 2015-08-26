#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"


#include "espconn.h"
#include "mem.h"
#include "gpio.h"
#include "user_config.h"
#include "driver/uart.h"



LOCAL uint8_t ICACHE_FLASH_ATTR StringToChar(uint8_t *data);
LOCAL uint8_t* ICACHE_FLASH_ATTR  ShortIntToString(uint16_t data, uint8_t *adressDestenation);
LOCAL void ICACHE_FLASH_ATTR senddata( void );
LOCAL void ICACHE_FLASH_ATTR initPeriph( void );


LOCAL struct espconn broadcast;
LOCAL esp_udp udp;


LOCAL uint16_t server_timeover = 180;
LOCAL struct espconn server;
LOCAL esp_tcp tcp1;
struct station_config stationConf;


uint8_t tmp[40] = { 0 };

uint8_t *count;

struct ip_info inf;

uint8_t ledState = 0;

LOCAL os_timer_t blink_timer;

void user_rf_pre_init(void)
{
}


LOCAL void ICACHE_FLASH_ATTR
tcp_recvcb(void *arg, char *pdata, unsigned short len)
{
	int i = 0;
	 uint8_t mes[] = "HELLO MAN";
    struct espconn *pespconn = (struct espconn *) arg;
    espconn_sent(arg, mes, strlen(mes));
    ets_uart_printf("Hello World!\r\n");
//    uart0_tx_buffer(pdata, len);
	for ( ; i++ < len; ){
		uart_tx_one_char(*pdata++);
	}
}

void ICACHE_FLASH_ATTR
sendDatagram(char *datagram, uint16 size) {
	os_timer_disarm(&blink_timer);
	switch(wifi_station_get_connect_status())
	{
		case STATION_GOT_IP:
			GPIO_OUTPUT_SET(LED_GPIO, 1);
			wifi_get_ip_info(STATION_IF, &inf);
			if(inf.ip.addr != 0) {
				#ifdef DEBUG
				ets_uart_printf("WiFi connected\r\n");
				#endif
				senddata();
			}
			break;
		case STATION_WRONG_PASSWORD:
			#ifdef DEBUG
			ets_uart_printf("WiFi connecting error, wrong password\r\n");
			#endif
			GPIO_OUTPUT_SET(LED_GPIO, ledState);
			ledState ^=1;
			break;
		case STATION_NO_AP_FOUND:
			#ifdef DEBUG
			ets_uart_printf("WiFi connecting error, ap not found\r\n");
			#endif
			GPIO_OUTPUT_SET(LED_GPIO, ledState);
			ledState ^=1;
			break;
		case STATION_CONNECT_FAIL:
			#ifdef DEBUG
			ets_uart_printf("WiFi connecting fail\r\n");
			#endif
			GPIO_OUTPUT_SET(LED_GPIO, ledState);
			ledState ^=1;
			break;
		default:
			#ifdef DEBUG
			ets_uart_printf("WiFi connecting...\r\n");
			#endif
			GPIO_OUTPUT_SET(LED_GPIO, ledState);
			ledState ^=1;
			break;

	}

	os_timer_setfn(&blink_timer, (os_timer_func_t *)sendDatagram, (void *)0);
	os_timer_arm(&blink_timer, DELAY, 0);
}

void ICACHE_FLASH_ATTR
user_init(void)
{
	initPeriph( );

	ets_uart_printf("TISO ver0.1");
	uart_tx_one_char(system_update_cpu_freq(SYS_CPU_160MHZ));
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_install_putc1(uart_tx_one_char);

#ifdef DEBUG
	os_printf( "SDK version: %s", system_get_sdk_version() );
#endif

//------------------------------------------------------------------
	wifi_station_disconnect();
	wifi_station_dhcpc_stop();

	wifi_set_opmode( STATION_MODE );
	memcpy(&stationConf.ssid, SSID_STA, sizeof(SSID_STA));
	memcpy(&stationConf.password, PWD_STA, sizeof(PWD_STA));
	wifi_station_set_config(&stationConf);

	wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);

//-----------------------------------------------------------------
	{ //tcp
		server.type = ESPCONN_TCP;
		server.state = ESPCONN_NONE;
		server.proto.tcp = &tcp1;
		server.proto.tcp->local_port = 80;
		espconn_accept(&server);
		espconn_regist_time(&server, server_timeover, 0);
		espconn_regist_recvcb(&server, tcp_recvcb);
	}


    { //udp
    	broadcast.type = ESPCONN_UDP;
    	broadcast.state = ESPCONN_NONE;
    	broadcast.proto.udp = &udp;
    	broadcast.proto.udp->remote_port = 9876;
    }

	// os_timer_disarm(ETSTimer *ptimer)
	os_timer_disarm(&blink_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&blink_timer, (os_timer_func_t *)sendDatagram, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&blink_timer, DELAY, 0);

}

LOCAL void ICACHE_FLASH_ATTR senddata( void )
{
	wifi_get_ip_info( STATION_IF, &inf );


	//выделить бродкаст айпишку
	IP4_ADDR((ip_addr_t *)broadcast.proto.udp->remote_ip, (uint8_t)(inf.ip.addr), (uint8_t)(inf.ip.addr >> 8),\
																(uint8_t)(inf.ip.addr >> 16), 255);

	count = ShortIntToString( (uint8_t)(inf.ip.addr), tmp );
	*count++ = '.';
	count = ShortIntToString( (uint8_t)(inf.ip.addr >> 8), count );
	*count++ = '.';
	count = ShortIntToString( (uint8_t)(inf.ip.addr >> 16) , count );
	*count++ = '.';
	count = ShortIntToString( (uint8_t)(inf.ip.addr  >> 24), count);
	*count = '\0';


	espconn_create(&broadcast);
	espconn_sent(&broadcast, tmp, strlen(tmp));
	espconn_delete(&broadcast);


}

LOCAL void ICACHE_FLASH_ATTR initPeriph( void ){

	PIN_FUNC_SELECT(OUT_1_MUX, OUT_1_FUNC);

	PIN_FUNC_SELECT(OUT_2_MUX, OUT_2_FUNC);

	PIN_FUNC_SELECT(LED_MUX, LED_FUNC);


	PIN_FUNC_SELECT(INP_1_MUX, INP_1_FUNC);
	gpio_output_set(0, 0, 0, INP_1);

	PIN_FUNC_SELECT(INP_2_MUX, INP_2_FUNC);
	gpio_output_set(0, 0, 0, INP_2);

	PIN_FUNC_SELECT(INP_3_MUX, INP_3_FUNC);
	gpio_output_set(0, 0, 0, INP_3);

	PIN_FUNC_SELECT(INP_3_MUX, INP_3_FUNC);
	gpio_output_set(0, 0, 0, INP_3);

}

// Перевод числа в последовательность ASCII
uint8_t * ShortIntToString(uint16_t data, uint8_t *adressDestenation) {
	uint8_t *startAdressDestenation = adressDestenation;
	uint8_t *endAdressDestenation;
	uint8_t buff;

	do {// перевод входного значения в последовательность ASCII кодов
		// в обратном порядке
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

// Перевод последовательности ASCII в число
uint8_t StringToChar(uint8_t *data) {
	uint8_t returnedValue = 0;
	for (;*data >= '0' && *data <= '9'; data++)
	returnedValue = 10 * returnedValue + (*data - '0');
	return returnedValue;
}


