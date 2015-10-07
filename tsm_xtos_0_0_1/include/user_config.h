#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define DEBUG

/*
*************************************************************************************************************************************
*                         ������ ������. ������ ��������/������� ���� ������������ �� �����������
*                         ������ � ������ query
*
* heap:
* - query address n length b\r\n
* 					��������� ���� ������. ��� ������ ������� n = 0, b = 0. ��� ������ �����������
* 					� ������� ���������� �� ��� ������ ������� ( n, � b ) ������ ������:
* 					n - ����� �������, b - ������. ��� �������� ���������� ������ ����� ���������:
*		 			query address n length b ������ OPERATION_OK\r\n - ���� ��
*		 			query address n length b ������ READ_DONE\r\n    - ��������� �������
*		 			query OPERATION_FAIL\r\n
*
*
* - insert string\r\n
* 					�������� ������. ������ ���� ������������. ����� �������� � ���������� ����
* 					������. �����
* 					insert string OPERATION_OK\r\n - ���� �������� ����������� �������, �����
* 					insert string WRONG_LENGHT\r\n
* 					insert string LINE_ALREADY_EXIST\r\n
* 					insert string NOT_ENOUGH_MEMORY\r\n
* 					insert string OPERATION_FAIL\r\n
*
*
* - delete string\r\n
* 					������� ������. ������ ���� ������������. ����� �������� � ���������� ����
* 					������. �����
* 					delete string OPERATION_OK\r\n - ���� �������� ����������� �������, �����
* 					delete string WRONG_LENGHT\r\n
* 					delete string NOTHING_FOUND\r\n
* 					delete string OPERATION_FAIL\r\n
*
*
* - update oldString newString\r\n
* 					�������� ������. ������ ���� ������������. ����� �������� � ���������� ����
* 					������, ����� ����������� ���� ���� ������. �����
* 					update oldString newString OPERATION_OK\r\n - ���� �������� ����������� �������, �����
* 					update oldString newString WRONG_LENGHT\r\n
* 					update oldString newString NOTHING_FOUND\r\n
* 					update oldString newString OPERATION_FAIL\r\n
* 					update oldString newString LINE_ALREADY_EXIST\r\n
*
*
* - request string\r\n
* 					����� ������ �� ����/�����.������ ���� ������������. ����� �������� � ����������
* 					���� ������. �����
* 					request string OPERATION_OK\r\n - ���� �������� ����������� �������,
* 					request string NOTHING_FOUND\r\n
* 					request string OPERATION_FAIL\r\n
* 					request string WRONG_LENGHT\r\n
*
* - find string\r\n
* 					����� ������ (��� ���� ���������).������ ���� ������������. ����� �������� �
* 					���������� ���� ������. �����
* 					find string OPERATION_OK\r\n - ���� �������� ����������� �������
* 					find string NOTHING_FOUND\r\n
* 					find string OPERATION_FAIL\r\n
* 					find string WRONG_LENGHT\r\n
*
* - clearDB\r\n
*
* WIFI:
*
* - resetWIFI\r\n
* 				������������� ������. �����
* 				resetWIFI OPERATION_OK\r\n
*
* - ssidSTA ssid\r\n
* 					������ ssid �������, ���� ������ 32 �������. �����
*					ssidSTA ssid OPERATION_OK\r\n
*					ssidSTA ssid OPERATION_FAIL\r\n
*
* - pwdSTA	pwd\r\n
*					������ pwd �������, ���� ������ 64 �������. �����
*					pwdSTA pwd OPERATION_OK\r\n
*					pwdSTA pwd OPERATION_FAIL\r\n
*
* - ssidAP ssid\r\n
* 					������ ssid ���, ���� ������ 32 �������. �����
*					ssidAP ssid OPERATION_OK\r\n
*					ssidAP ssid OPERATION_FAIL\r\n
*
* - pwdAP  pwd\r\n
*					������ pwd ���, ���� ������ 64 �������. �����
*					pwdAP pwd OPERATION_OK\r\n
*					pwdAP pwd OPERATION_FAIL\r\n
*
* - broadcastName name\r\n
* 					������ BROADCAST_NAME ���, ���� ������ 50 ��������. �����
*					broadcastName name OPERATION_OK\r\n
*					broadcastName name OPERATION_FAIL\r\n
*
* - gpioMode_1 Trigger/Impulse delay\r\n
* 					������ ����� ������ out1 Trigger/Impulse, � ���������� ������� delay.
* 					�����
* 					gpioMode_1 Trigger/Impulse delay OPERATION_OK\r\n
* 					gpioMode_1 Trigger/Impulse delay OPERATION_FAIL\r\n
*
* - gpioMode_2 Trigger/Impulse delay\r\n
*					������ ����� ������ out2 Trigger/Impulse, � ���������� ������� delay.
* 					�����
* 					gpioMode_2 Trigger/Impulse delay OPERATION_OK\r\n
* 					gpioMode_2 Trigger/Impulse delay OPERATION_FAIL\r\n
*
* - enableGpio_1\r\n
* 					���������� ���� gpioStatusOut1 = Enable. �����
* 					enableGpio1 OPERATION_OK\r\n
* 					enableGpio1 OPERATION_FAIL\r\n
*
* - enableGpio_2\r\n
*					���������� ���� gpioStatusOut2 = Enable. �����
* 					enableGpio2 OPERATION_OK\r\n
* 					enableGpio2 OPERATION_FAIL\r\n
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

#define USER_SECTOR_IN_FLASH_MEM		( END_SECTOR + 1 ) 	            // ����������� � END_SECTOR � myDB.h

/*  spi_flash layout
 *
 *  0............11  ..12          ������ ������������ ������ �������  (
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
 * |   Trigger\Impulse\0  | \n |           ����� ������
 *  1022.............1031
 * |    deley \0    | \n |                 ms, ������� 10
 *
  *  1200................1212
 * |   GPIO OUT 2:\0   | \n |              header
 *  1213...................1221
 * |   Trigger\Impulse\0  | \n |           ����� ������
 *  1222.............1231
 * |    deley \0    | \n |                 ms, ������� 10
 *
 *
 *  1400....................1416
 * |   BROADCAST NAME:\0   | \n |          header
 *  1417.........1068
 * |    NAME\0   | \n |           		   max lenght 50
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

#define DEF_GPIO_OUT_MODE    	"Impulse"
#define GPIO_OUT_TRIGGER_MODE	"Trigger"
#define DEF_GPIO_OUT_DELEY	 	"1000"

#define BROADCAST_NAME_HEADER			"BROADCAST NAME:"
#define BROADCAST_NAME_HEADER_OFSET		1400
#define BROADCAST_NAME					"WIFI MODULE"
#define BROADCAST_NAME_OFSET			1417
//***********************************************************************************************************************************
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

#define TCP_SERVER_TIMEOUT	60  // SEC
#define TCP_PORT			80

#define UDP_REMOTE_PORT		9876

//STA
//#define SSID_STA /*"TSM_Guest" */"DIR-320"
//#define PWD_STA "tsmguest"
//***********************************************************************************************************************************
//***********************************************************************************************************************************
//Scheduler timeout
#define DELAY 	10 /* milliseconds */
//***********************************************************************************************************************************
// broadcast timer
#define BROADCAST_TIMER		150 //ms
//***********************************************************************************************************************************

typedef enum {
	ENABLE = 0,
	DISABLE

} stat;

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

#define NAME 				"\nname: "
#define MAC 				"\nmacSTA: "
#define IP 					"\nip: "
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

#define TCP_QUERY					"query"
#define TCP_INSERT					"insert" 		//+
#define TCP_DELETE					"delete"		//+
#define TCP_UPDATE					"update"		//+
#define TCP_REQUEST					"request"		//+
#define TCP_FIND					"find"			//+
#define TCP_SSID_STA				"ssidSTA"		//+
#define TCP_PWD_STA					"pwdSTA"		//+
#define TCP_SSID_AP					"ssidAP"		//+
#define TCP_PWD_AP					"pwdAP"			//+
#define TCP_BROADCAST_NAME			"broadcastName"
#define TCP_GPIO_MODE_1				"gpioMode_1"
#define TCP_GPIO_MODE_2				"gpioMode_2"
#define TCP_ENABLE_GPIO_1			"enableGpio_1"  //++
#define TCP_ENABLE_GPIO_2			"enableGpio_2"  //++
#define TCP_CLEAR_HEAP              "clearDB"
#define TCP_RESET                   "resetWIFI"		//++

#define TCP_ERROR					"ERROR"


//***********************************************************************************************************************************
#endif

