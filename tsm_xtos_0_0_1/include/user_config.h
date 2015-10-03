#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define DEBUG


//**********************************************************************************************

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

#define USER_SECTOR_IN_FLASH_MEM		END_SECTOR + 1 	            // согласовать с END_SECTOR в myDB.h

/*  spi_flash layout
 *
 *  0............11  ..12          адресс относительно начала сектора  (
 * | flash_ready\0 | \n |          USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE )
 *
 *  100..104 ..105
 * | STA:\0 | \n |                 		header
 *  106........139
 * | SSID \0 | \n |                   	max lenght 32
 *  140.............205
 * | PASSWORD  \0 | \n |               	max lenght 64
 *
 *  500..........504
 * |   AP:\0   | \n |              		header
 *  505.....
 * |    SSID     |
 *
 *
 */


//**********************************************************************************************
//**********************************************************************************************

//STA
#define SSID_STA /*"TSM_Guest" */"DIR-320"
#define PWD_STA "tsmguest"
//----------------------------------------------------------------
//Udp
#define DELAY 	100 /* milliseconds */
#endif
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

