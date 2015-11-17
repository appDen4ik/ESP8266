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

#define UART0   0
/*
*************************************************************************************************************************************
* set\0 id\0 command\0\r\n	- задать command для id турникета
*
* setChanel\0 chanel\0\r\n  - установить канал AP
*
*************************************************************************************************************************************
*/

//***********************************************************************************************************************************
//***********************************************************************************************************************************

/*  spi_flash layout
 *
 *  0............11  ..12          адресс относительно начала сектора  (
 * | flash ready\0 | \n |          USER_SECTOR_IN_FLASH_MEM * SPI_FLASH_SEC_SIZE )
 *
 *  100..104 ..105
 * | chanel\0 | \n |               chanel
 */


#define FLASH_READY		     	 "flash ready"
#define FLASH_READY_OFSET		 0

#define DEF_CHANEL_AP			"7"
#define CHANEL_AP_OFSET	    	100

//***********************************************************************************************************************************
#define IP_AP	"172.168.0.1"
//***********************************************************************************************************************************
#define TMP_SIZE    1000
//***********************************************************************************************************************************
#define DELAY 				10   //mcs
#define DELAY_TIMER 		80 	 //ms
#define BROADCAST_TIMER  	200  //ms
//***********************************************************************************************************************************
// tcp, udp parameters

#define DEF_UDP_PORT					"19876"

#define TCP_SERVER_TIMEOUT	20  // SEC
#define TCP_PORT			6766
//***********************************************************************************************************************************
//turnstile

#define TURSTILE_NOT_RESPONSE	 	256
#define TURSTILE_CRC_ERROR		 	257
//***********************************************************************************************************************************

#define USER_SECTOR_IN_FLASH_MEM		59

typedef enum {

				DONE = 0,
				ERROR      	} res;

typedef enum {
				TCP_FREE = 0,
				TCP_BUSY       } tcp_stat;

typedef enum {
				CRC_OK = 0,
				CRC_ERROR       } crc_stat;

typedef enum {
				BROADCAST_NEED = 0,
				BROADCAST_NOT_NEED     } broadcast_stat;

typedef enum {
				TURNSTILE_STATUS = 0,
				TURNSTILE_COMMAND		} turnstileOperation;

typedef struct {
					uint8_t address;
					uint8_t numberOfBytes;
					uint8_t typeData;
					uint8_t data;
					uint8_t crc;			} turnstile;

//***********************************************************************************************************************************
#define BROADCAST_IP_AP 				"ipAP: "
#define BROADCAST_COUNTER_TURNSTILE		"\nnumber turnstiles: "
#define BROADCAST_ID					"\nid: "
#define BROADCAST_STATUS				" status: "
//***********************************************************************************************************************************
// TCP response constants
#define TCP_OPERATION_FAIL			"OPERATION_FAIL"
#define TCP_OPERATION_OK			"OPERATION_OK"

#define TCP_ID						"id"
#define TCP_COMMAND					"command"

#define TCP_SET_TURNSTILE			"set"
#define	TCP_SET_CHANEL				"setChanel"

#define TCP_ERROR					"ERROR"

//***********************************************************************************************************************************
#endif

