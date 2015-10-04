#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define DEBUG

/*
*************************************************************************************************
*                                   ������ ������:
*
* ��:
* - query\r\n
* 					��������� ���� ������. ������ ������: "query db n lenght b ������ \r\n"
* 					n - ����� �������, b - ������. ��� �������� ���������� ������ ����� ���������:
*		 			"query done db n lenght b ������ \r\n". ��� ������ ERROR
*
* - insert string\r\n
* 					�������� ������. ������ ���� ������������. ����� �������� � ���������� ����
* 					������. ����� insert string OK\r\n - ���� �������� ����������� �������, �����
* 					insert string ERROR\r\n
*
* - delete string\r\n
* 					������� ������. ������ ���� ������������. ����� �������� � ���������� ����
* 					������. ����� delete string OK\r\n - ���� �������� ����������� �������, �����
* 					delete string ERROR\r\n
*
* - update oldString newString\r\n
* 					�������� ������. ������ ���� ������������. ����� �������� � ���������� ����
* 					������, ����� ����������� ���� ���� ������. �����
* 					update oldString newString OK\r\n - ���� �������� ����������� �������, �����
* 					update oldString newString ERROR\r\n
*
* - request string\r\n
* 					����� ������ �� ����/�����.������ ���� ������������. ����� �������� � ����������
* 					���� ������. ����� request string OK\r\n - ���� �������� ����������� �������,
* 					����� request string ERROR\r\n
*
* - find string\r\n
* 					����� ������ (��� ���� ���������).������ ���� ������������. ����� �������� �
* 					���������� ���� ������. ����� request string OK\r\n - ���� �������� �����������
* 					�������, ����� request string ERROR\r\n
*
* WIFI:
*
* - ssidSTA ssid\r\n
*
* - pwdSTA	pwd\r\n
*
* - ssidAP ssid\r\n
*
* - pwdAP  pwd\r\n
*************************************************************************************************
*/

#define OUT_1_GPIO 		4
#define OUT_1_MUX 		PERIPHS_IO_MUX_GPIO4_U
#define OUT_1_FUNC 		FUNC_GPIO4

#define OUT_2_GPIO		5
#define OUT_2_MUX 		PERIPHS_IO_MUX_GPIO5_U
#define OUT_2_FUNC 		FUNC_GPIO5
//**********************************************************************************************

//status_led
#define LED_GPIO		15
#define LED_MUX 		PERIPHS_IO_MUX_MTDO_U
#define LED_FUNC 		FUNC_GPIO15
//**********************************************************************************************

#define INP_1 			BIT12
#define INP_1_MUX 		PERIPHS_IO_MUX_MTDI_U
#define INP_1_FUNC 		FUNC_GPIO12
#define INP_1_PIN		12

#define INP_2 			BIT13
#define INP_2_MUX 		PERIPHS_IO_MUX_MTCK_U
#define INP_2_FUNC 		FUNC_GPIO13
#define INP_2_PIN		13

#define INP_3 			BIT14
#define INP_3_MUX 		PERIPHS_IO_MUX_GPIO2_U
#define INP_3_FUNC 		FUNC_GPIO14
#define INP_3_PIN		14

#define INP_4 			BIT2
#define INP_4_MUX 		PERIPHS_IO_MUX_MTMS_U
#define INP_4_FUNC 		FUNC_GPIO2
#define INP_4_PIN		2
//**********************************************************************************************
//**********************************************************************************************

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
 * |   BROADCAST NAME:\0   | \n |              header
 *  1417.........1068
 * |    NAME\0   | \n |           				max lenght 50
 */


#define FLASH_READY		     	 "flash ready"
#define FLASH_READY_OFSET		 0
#define ALIGN_FLASH_READY_SIZE   ( 4 - (  sizeof(FLASH_READY) % 4 ) +  sizeof(FLASH_READY) )



#define DEF_SSID_STA         	"Default STA"
#define SSID_STA_OFSET			106
#define DEF_PWD_STA         	""
#define PWD_STA_OFSET       	140
#define HEADER_STA		 	    "STA:"
#define HEADER_STA_OFSET	    100


#define DEF_SSID_AP          	"Default AP"
#define SSID_AP_OFSET			505
#define DEF_PWD_AP		        "Default AP"
#define PWD_AP_OFSET			539
#define HEADER_AP				"AP:"
#define HEADER_AP_OFSET			500


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
#define DEF_GPIO_OUT_DELEY	 	"500"


#define BROADCAST_NAME_HEADER			"BROADCAST NAME:"
#define BROADCAST_NAME_HEADER_OFSET		1400
#define BROADCAST_NAME					"WIFI MODULE"
#define BROADCAST_NAME_OFSET			1417

//**********************************************************************************************
//**********************************************************************************************

#define DEF_CHANEL       7
#define DEF_AUTH		AUTH_WPA_WPA2_PSK
#define MAX_CON			4
#define NO_HIDDEN 		0
#define BEACON_INT		100

//**********************************************************************************************
//**********************************************************************************************

#define TMP_SIZE		10000

//STA
//#define SSID_STA /*"TSM_Guest" */"DIR-320"
//#define PWD_STA "tsmguest"
//----------------------------------------------------------------

//**********************************************************************************************
//**********************************************************************************************
//Scheduler
#define DELAY 	10 /* milliseconds */


typedef enum {
	ENABLE = 0,
	DISABLE

} stat;

//----------------------------------------------------------------
//brodcast constant strings
#define NAME 				"name: "
#define MAC 				"\nmacSTA: "
#define IP 					"\nip: "
#define SERVER_PORT 		"\nserver port: "
#define RSSI 				"\nrssi: "
#define STATUSES 			"\nSTATUSES: "
#define PHY_MODE 			"\nphy mode: "
#define PHY_MODE_B 			"IEEE 802.11b"
#define PHY_MODE_G     		"IEEE 802.11g"
#define PHY_MODE_N      	"IEEE 802.11n"
#define DOOR_CLOSE_SENSOR 	"\n\tdoorCloseSensor: "
#define DOOR_OPEN_SENSOR 	"\n\tdoorOpenSensor: "
#define OPEN 				"open "
#define CLOSE 				"close "

#endif

