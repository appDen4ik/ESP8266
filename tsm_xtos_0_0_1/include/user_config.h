/**
 ************************************************************************************
  * @file    user_config.h
  * @author  Denys
  * @version V0.1
  * @date    7-Sept-2015
  * @desription: - список констант для tcp, udp
  * 			 - описание доступных настроек, с указанием их адрессов, а также здесь
  * 			   указаны настройки по умолчанию
  *              - список доступных команид для работы с есп
 ************************************************************************************
**/

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define DEBUG

/*
*************************************************************************************************************************************
*                         список команд. каждый параметр/команда нуль терминальные за исключением
*                         данных в методе query
*
* heap:
* - query address n \r\n
* 					Запросить базу данных. При первом запросе n = 0. При каждом последующем
* 					в запросе передается то что модуль ответил в предыдущий раз ( n ) Формат ответа:
* 					n - номер посилки, b - длина. Ответ:
*		 			query address n length b данные OPERATION_OK\r\n - если ОК
*		 			query address n length b данные READ_DONE\r\n    - последняя посилка
*		 			query OPERATION_FAIL\r\n
*
*
* - insert string\r\n
* 					Вставить запись. Между командой и параметром один
* 					пробел. Ответ
* 					insert string OPERATION_OK\r\n - если операция выполнилась успешно, иначе
* 					insert string WRONG_LENGHT\r\n
* 					insert string LINE_ALREADY_EXIST\r\n
* 					insert string NOT_ENOUGH_MEMORY\r\n
* 					insert string OPERATION_FAIL\r\n
*
*
* - delete string\r\n
* 					Удалить запись. Между командой и параметром один
* 					пробел. Ответ
* 					delete string OPERATION_OK\r\n - если операция выполнилась успешно, иначе
* 					delete string WRONG_LENGHT\r\n
* 					delete string NOTHING_FOUND\r\n
* 					delete string OPERATION_FAIL\r\n
*
*
* - update oldString newString\r\n
* 					Обновить запись. Между командой и параметром один
* 					пробел, между параметрами тоже один пробел. Ответ
* 					update oldString newString OPERATION_OK\r\n - если операция выполнилась успешно, иначе
* 					update oldString newString WRONG_LENGHT\r\n
* 					update oldString newString NOTHING_FOUND\r\n
* 					update oldString newString OPERATION_FAIL\r\n
* 					update oldString newString LINE_ALREADY_EXIST\r\n
*
*
* - request string\r\n  ( команда ассоциирована с out1 )
* 					Найти запись по полю/полям. Между командой и параметром
* 					один пробел. Ответ
* 					request string OPERATION_OK\r\n - если операция выполнилась успешно,
* 					request string NOTHING_FOUND\r\n
* 					request string OPERATION_FAIL\r\n
* 					request string WRONG_LENGHT\r\n
*
*
* - find string\r\n
* 					Найти запись (все поля заполнены). Между командой и
* 					параметром один пробел. Ответ
* 					find string OPERATION_OK\r\n - если операция выполнилась успешно
* 					find string NOTHING_FOUND\r\n
* 					find string OPERATION_FAIL\r\n
* 					find string WRONG_LENGHT\r\n
*
* - clearDB\r\n
*
*
* WIFI:
*
*  - restore\r\n
* 					Восстановить настройки по умаолчанию
*
* - resetWIFI\r\n
* 					перезагрузить модуль. Ответ
* 					resetWIFI OPERATION_OK\r\n
*
* - ssidSTA ssid\r\n
* 					Задать ssid роутера, макс длинна 32 символа. Ответ
*					ssidSTA ssid OPERATION_OK\r\n
*					ssidSTA ssid OPERATION_FAIL\r\n
*
* - pwdSTA	pwd\r\n
*					Задать pwd роутера, макс длинна 64 символа. Ответ
*					pwdSTA pwd OPERATION_OK\r\n
*					pwdSTA pwd OPERATION_FAIL\r\n
*
* - ssidAP ssid\r\n
* 					Задать ssid есп soft ap, макс длинна 32 символа. Ответ
*					ssidAP ssid OPERATION_OK\r\n
*					ssidAP ssid OPERATION_FAIL\r\n
*
* - pwdAP  pwd\r\n
*					Задать pwd есп soft ap, макс длинна 64 символа. Ответ
*					pwdAP pwd OPERATION_OK\r\n
*					pwdAP pwd OPERATION_FAIL\r\n
*
* - broadcastName name\r\n
* 					Задать BROADCAST_NAME есп (имя есп при широковещательных запросах),
* 					макс длинна 32 символа. Ответ
*					broadcastName name OPERATION_OK\r\n
*					broadcastName name OPERATION_FAIL\r\n
*
* - gpioMode_1 Trigger/Impulse/Combine delay\r\n
* 					Задать режим работы out1 Trigger/Impulse, и установить таймаут delay ms.
* 					(время приблизительное)
* 					Ответ
* 					gpioMode_1 Trigger/Impulse/Combine delay OPERATION_OK\r\n
* 					gpioMode_1 Trigger/Impulse/Combine delay OPERATION_FAIL\r\n
*
* - gpioMode_2 Trigger/Impulse/Combine delay\r\n
*					Задать режим работы out2 Trigger/Impulse, и установить таймаут delay ms.
*					(время приблизительное)
* 					Ответ
* 					gpioMode_2 Trigger/Impulse/Combine delay OPERATION_OK\r\n
* 					gpioMode_2 Trigger/Impulse/Combine delay OPERATION_FAIL\r\n
*
* - enableGpio_1\r\n
* 					enableGpio1 OPERATION_OK\r\n
*
* - enableGpio_2\r\n
* 					enableGpio2 OPERATION_OK\r\n
*
* - disableGpio_1\r\n
* 					enableGpio1 OPERATION_OK\r\n
*
* - disableGpio_2\r\n
* 					enableGpio2 OPERATION_OK\r\n
*
* - setIP ip\r\n
* 					setIP ip OPERATION_OK\r\n
* 					setIP ip OPERATION_FAIL\r\n
*
* - setUdpPort port\r\n
* 					setUdpPort port OPERATION_OK\r\n
* 					setUdpPort port OPERATION_FAIL\r\n
*
* - setMaxTpw tpw\r\n
*					setMaxTpw  tpw OPERATION_OK\r\n
*************************************************************************************************************************************
*/

#define OUT_1_GPIO 		4
#define OUT_1_MUX 		PERIPHS_IO_MUX_GPIO4_U
#define OUT_1_FUNC 		FUNC_GPIO4

#define OUT_2_GPIO		5
#define OUT_2_MUX 		PERIPHS_IO_MUX_GPIO5_U
#define OUT_2_FUNC 		FUNC_GPIO5
//***********************************************************************************************************************************
//status_led

#define LED_GPIO		15
#define LED_MUX 		PERIPHS_IO_MUX_MTDO_U
#define LED_FUNC 		FUNC_GPIO15
//***********************************************************************************************************************************

#define INP_1 			BIT12
#define INP_1_MUX 		PERIPHS_IO_MUX_MTDI_U
#define INP_1_FUNC 		FUNC_GPIO12
#define INP_1_PIN		12

#define INP_2 			BIT13
#define INP_2_MUX 		PERIPHS_IO_MUX_MTCK_U
#define INP_2_FUNC 		FUNC_GPIO13
#define INP_2_PIN		13

#define INP_3 			BIT14
#define INP_3_MUX 		PERIPHS_IO_MUX_MTMS_U
#define INP_3_FUNC 		FUNC_GPIO14
#define INP_3_PIN		14

#define INP_4 			BIT2
#define INP_4_MUX 		PERIPHS_IO_MUX_GPIO2_U
#define INP_4_FUNC 		FUNC_GPIO2
#define INP_4_PIN		2
//***********************************************************************************************************************************
//***********************************************************************************************************************************
// user data/parameters

#define USER_SECTOR_IN_FLASH_MEM		( END_SECTOR + 1 ) 	            // согласовать с END_SECTOR в myDB.h

/*  spi_flash layout
 *
 *  0............11  ..12          адресс относительно начала сектора  (
 * | flash ready\0 | \n |          USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE )
 *
 *  100..104 ..105
 * | STA:\0 | \n |                 		   header
 *  106........139
 * | SSID \0 | \n |                   	   max lenght 32
 *  140.............205
 * |  PASSWORD\0  | \n |               	   max lenght 64
 *
 *  500..........504
 * |   AP:\0   | \n |              		   header
 *  505.............538
 * |    SSID \0   | \n |                   max lenght 32
 *  539.............602
 * |  PASSWORD\0  | \n |               	   max lenght 64
 *
 *
 *  1000................1012
 * |   GPIO OUT 1:\0   | \n |              header
 *  1013...................1021
 * |   Trigger/Impulse/Combine\0  | \n |           режим работы
 *  1022.............1031
 * |    deley\0    | \n |                 ms, кратное 10
 *
  *  1200................1212
 * |   GPIO OUT 2:\0   | \n |              header
 *  1213...................1221
 * |   Trigger/Impulse/Combine\0  | \n |           режим работы
 *  1222.............1231
 * |    deley\0    | \n |                 ms, кратное 10
 *
 *
 *  1400....................1416
 * |   BROADCAST NAME:\0   | \n |          header
 *  1417.........1450
 * |    NAME\0   | \n |           		   max lenght 32
 *
 *  1600.................1612
 * |  IP SOFT AP:\0   |  \n  |
 *  1613............1629
 * |     ip\0    | \n |
 *
 *  1700................1710
 * |   UDP PORT:\0   |  \n  |
 *  1711.............1717
 * |      port\0   | \n  |
 *
 *  1800...1805
 * |  clear\0  |  \n  |
 *
 *  2000..........2009
 * | MAX TPW:\0 | \n  | мощьность TX, RX
 *  2010......2013
 * | value\0 | \n |
 */


#define FLASH_READY		     	 "flash ready"
#define FLASH_READY_OFSET		 0
#define ALIGN_FLASH_READY_SIZE   ( 4 - (  sizeof(FLASH_READY) % 4 ) +  sizeof(FLASH_READY) )

#define DEF_SSID_STA         	"Default STA"
#define SSID_STA_OFSET			106
#define DEF_PWD_STA         	"Default STA"
#define PWD_STA_OFSET       	140
#define HEADER_STA		 	    "STA:"
#define HEADER_STA_OFSET	    100

#define DEF_SSID_AP          	"Default AP"
#define SSID_AP_OFSET			505
#define DEF_PWD_AP		        "Default AP"
#define PWD_AP_OFSET			539
#define HEADER_AP				"AP:"
#define HEADER_AP_OFSET			500

#define  SSID_MAX_LENGHT         		32
#define  PWD_MAX_LENGHT         		64
#define  BROADCAST_NAME_MAX_LENGHT		32

#define  GPIO_OUT_1_HEADER		 	"GPIO OUT 1:"
#define  GPIO_OUT_1_HEADER_OFSET	1000
#define  GPIO_OUT_1_MODE_OFSET		1013
#define  GPIO_OUT_1_DELEY_OFSET		1022

#define  GPIO_OUT_2_HEADER		 	"GPIO OUT 2:"
#define  GPIO_OUT_2_HEADER_OFSET	1200
#define  GPIO_OUT_2_MODE_OFSET		1213
#define  GPIO_OUT_2_DELEY_OFSET		1222

#define GPIO_OUT_IMPULSE_MODE    "Impulse"
#define GPIO_OUT_TRIGGER_MODE	"Trigger"
#define GPIO_OUT_COMBINE_MODE	"Combine"
#define DEF_GPIO_OUT_DELEY	 	"1000"

#define BROADCAST_NAME_HEADER			"BROADCAST NAME:"
#define BROADCAST_NAME_HEADER_OFSET		1400
#define BROADCAST_NAME					"WIFI MODULE"
#define BROADCAST_NAME_OFSET			1417

#define MAX_TPW_HEADER                "MAX TPW:"
#define MAX_TPW_HEADER_OFSET          2000
#define MAX_TPW_DEFAULT_VALUE         "82"
#define MAX_TPW_VALUE_OFSET           2010
#define MAX_TPW_MIN_VALUE             15
#define MAX_TPW_MAX_VALUE             82
//***********************************************************************************************************************************
#define DEF_IP_SOFT_AP_HEADER        	"IP SOFT AP:"
#define DEF_IP_SOFT_AP_HEADER_OFSET		1600
#define DEF_IP_SOFT_AP					"172.18.4.1"
#define DEF_IP_SOFT_AP_OFSET			1613

#define DEF_UDP_PORT_HEADER				"UDP PORT:"
#define DEF_UDP_PORT_HEADER_OFSET		1700
#define DEF_UDP_PORT					"19876"
#define DEF_UDP_PORT_OFSET				1711
//***********************************************************************************************************************************
#define CLEAR_DB_STATUS			"clear"
#define CLEAR_DB_STATUS_EMPTY	" "
#define CLEAR_DB_STATUS_OFSET	1800
//***********************************************************************************************************************************
// wifi AP parameters
#define DEF_CHANEL      7
#define DEF_AUTH		AUTH_WPA_WPA2_PSK
#define MAX_CON			4
#define NO_HIDDEN 		0
#define BEACON_INT		100
//***********************************************************************************************************************************
//***********************************************************************************************************************************
// tcp, udp parameters

#define TMP_SIZE		    10000

#define TCP_SERVER_TIMEOUT	25  // SEC
#define TCP_PORT			80

//STA
//#define SSID_STA /*"TSM_Guest" */"DIR-320"
//#define PWD_STA "tsmguest"
//***********************************************************************************************************************************
//***********************************************************************************************************************************
//Scheduler timeout
#define DELAY 	10 /* milliseconds */
//***********************************************************************************************************************************
// broadcast timer
#define BROADCAST_TIMER		300 //ms
//***********************************************************************************************************************************

typedef enum {
	ENABLE = 0,
	DISABLE

} stat;

typedef enum {
	mSET= 0,
	mCLEAR

} mark;

typedef enum {
	DONE = 0,
	ERROR

} res;

typedef enum {
	TCP_FREE = 0,
	TCP_BUSY

} tcp_stat;

typedef enum {
	LENGHT_ERROR = 0,
	LENGHT_OK

} compStr;

//***********************************************************************************************************************************
//brodcast constant strings
#define SSID_STA            "\nssidSTA: "
#define PWD_STA             "\npwdSTA: "
#define SSID_AP			    "\nssidAP: "
#define PWD_AP				"\nwdAP: "
#define MAX_TPW             "\nmaxTpw: "
#define BOADCAST_PORT       "\nbtoadcastPort: "
#define NAME 				"\nname: "
#define MAC 				"\nmacSTA: "
#define IP 					"\nipSTA: "
#define IP_AP 				"\nipAP: "
#define SERVER_PORT 		"\nserver port: "
#define RSSI 				"\nrssi: "
#define STATUSES 			"\nstatuses: "
#define PHY_MODE 			"\nphy mode: "
#define PHY_MODE_B 			"IEEE 802.11b"
#define PHY_MODE_G     		"IEEE 802.11g"
#define PHY_MODE_N      	"IEEE 802.11n"
#define OUTPUTS				"\noutputs: "
#define GPIO_1				"\ngpio_1: "
#define GPIO_2				"\ngpio_2: "
#define DELAY_NAME			" delay: "
#define INPUTS              "\ninputs: "
#define INPUT_1				"\ninp_1: "
#define INPUT_2				"\ninp_2: "
#define INPUT_3				"\ninp_3: "
#define INPUT_4				"\ninp_4: "
#define STATUS_HIGH			"high"
#define STATUS_LOW			"low"
#define MS					"ms"
//***********************************************************************************************************************************
// TCP response constants

#define TCP_OPERATION_FAIL			"OPERATION_FAIL"
#define TCP_WRONG_LENGHT			"WRONG_LENGHT"
#define TCP_NOTHING_FOUND			"NOTHING_FOUND"
#define TCP_NOT_ENOUGH_MEMORY		"NOT_ENOUGH_MEMORY"
#define TCP_LINE_ALREADY_EXIST		"LINE_ALREADY_EXIST"
#define TCP_OPERATION_OK			"OPERATION_OK"
#define TCP_READ_DONE				"READ_DONE"

#define TCP_ADRESS					"address"
#define TCP_LENGHT					"length"

#define TCP_QUERY					"query"			//+
#define TCP_INSERT					"insert" 		//+
#define TCP_DELETE					"delete"		//+
#define TCP_UPDATE					"update"		//+
#define TCP_REQUEST					"request"		//+
#define TCP_FIND					"find"			//+
#define TCP_SSID_STA				"ssidSTA"		//+
#define TCP_PWD_STA					"pwdSTA"		//+
#define TCP_SSID_AP					"ssidAP"		//+
#define TCP_PWD_AP					"pwdAP"			//+
#define TCP_BROADCAST_NAME			"broadcastName" //+
#define TCP_GPIO_MODE_1				"gpioMode_1"    //+
#define TCP_GPIO_MODE_2				"gpioMode_2"	//+
#define TCP_DISABLE_GPIO_1			"disableGpio_1" //+
#define TCP_DISABLE_GPIO_2   		"disableGpio_2" //+
#define TCP_ENABLE_GPIO_1			"enableGpio_1"  //+
#define TCP_ENABLE_GPIO_2			"enableGpio_2"  //+
#define TCP_RESET                   "resetWIFI"		//+
#define TCP_SET_IP					"setIP"			//+
#define TCP_SET_UDP_PORT			"setUdpPort"    //+
#define TCP_CLEAR_DB				"clearDB"		//+
#define TCP_RESTORE                 "restore"		//+
#define TCP_MAX_TPW                 "setMaxTpw"

#define TCP_ERROR					"ERROR"


//***********************************************************************************************************************************
#endif

