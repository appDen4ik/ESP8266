/**
 ************************************************************************************
  * @file    myDB.h
  * @author  Denys
  * @version V0.0.1
  * @date    7-Sept-2015
  * @brief
 ************************************************************************************
**/

#ifndef INCLUDE_DRIVER_MYDB_H_
#define INCLUDE_DRIVER_MYDB_H_

#define DB_DEBUG

#include "c_types.h"
#include "spi_flash.h"


#define STRING_SIZE	    256              // длинна записи

#define START_SECTOR 	19               // начальный сектор

#define END_SECTOR 		58               // последний сектор


/*
 ***************************************************************************************************************
 * ¬ыровн€на€ длинна записи. »спользование этого значени€ св€зано с тем что функци€ записи данных во spi flash
 * пишет по 4 байта. ќпредел€етс€ следующим образом: если длинна записи делитс€ нацело на 4 тогда
 * ALIGN_STRING_SIZE = STRING_SIZE, если не делитс€ то нужно округлить до ближайшего большего значени€ так чтобы
 * делилось нацело на 4
 * ѕример
 * 		STRING_SIZE	  14   тогда   ALIGN_STRING_SIZE 	16
 * 		STRING_SIZE    4   тогда   ALIGN_STRING_SIZE 	 4
 ***************************************************************************************************************
 */

#if STRING_SIZE % 4 == 0
     #define ALIGN_STRING_SIZE       STRING_SIZE
#else
	#define ALIGN_STRING_SIZE 	   ( 4 - ( STRING_SIZE % 4 ) + STRING_SIZE )
#endif

#if STRING_SIZE > SPI_FLASH_SEC_SIZE || STRING_SIZE == 0
	#error STRING_SIZE wrong lenght
#endif



#define START_OF_TEXT	 2

#define END_OF_SRING    '\0'


typedef enum {
		OPERATION_FAIL = 0,
		WRONG_LENGHT,
		NOTHING_FOUND,
		NOT_ENOUGH_MEMORY,
		LINE_ALREADY_EXIST,
		OPERATION_OK
} result;



result ICACHE_FLASH_ATTR insert( uint8_t *string );							   //tested
uint32_t ICACHE_FLASH_ATTR findString( uint8_t *string );					   //tested
result ICACHE_FLASH_ATTR delete( uint8_t *removableString );
result ICACHE_FLASH_ATTR clearSectorsDB( void );                               //tested
result ICACHE_FLASH_ATTR update( uint8_t *oldString, uint8_t *newString );     //tested
result ICACHE_FLASH_ATTR requestString( uint8_t *string );

#endif /* INCLUDE_DRIVER_MYDB_H_ */
