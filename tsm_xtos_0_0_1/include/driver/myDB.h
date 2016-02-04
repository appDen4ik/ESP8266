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


#include "c_types.h"
#include "spi_flash.h"


#define STRING_SIZE	    100 	            // длинна строки

#define START_SECTOR 	19                  // начальный сектор

#define END_SECTOR 		58                  // последний сектор


/*
 ***************************************************************************************************************
 * Выровняная длинна записи. Использование этого значения связано с тем что функция записи данных во spi flash
 * пишет по 4 байта. Определяется следующим образом: если длинна записи делится нацело на 4 тогда
 * ALIGN_STRING_SIZE = STRING_SIZE, если не делится то нужно округлить до ближайшего большего значения так чтобы
 * делилось нацело на 4
 * Пример
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

#define FIELDS_MAX_NUM	10    // максимально допустимое количество записей в одной строке

#define STORAGE_SIZE    1000  // байт

#define START_OF_FIELD	 2    // ascii символ начала поля

#define END_OF_FIELD	'\n'  // символ конца поля

#define END_OF_STRING   '\0'  // символ конца нуль терминальной строки


typedef enum {
		OPERATION_FAIL = 0,
		WRONG_LENGHT,
		NOTHING_FOUND,
		NOT_ENOUGH_MEMORY,
		LINE_ALREADY_EXIST,
		OPERATION_OK,
		READ_DONE
} result;


// все строки в параметрах должны быть нуль терминальными
result ICACHE_FLASH_ATTR insert( uint8_t *string );
uint32_t ICACHE_FLASH_ATTR findString( uint8_t *string );
result ICACHE_FLASH_ATTR delete( uint8_t *removableString );
result ICACHE_FLASH_ATTR clearSectorsDB( void );
result ICACHE_FLASH_ATTR update( uint8_t *oldString, uint8_t *newString );
result ICACHE_FLASH_ATTR requestString( uint8_t *string );
result ICACHE_FLASH_ATTR query( uint8_t *storage, uint16_t *length, uint32_t *absAdrInFlash );

#endif /* INCLUDE_DRIVER_MYDB_H_ */
