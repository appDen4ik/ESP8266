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


/*
 * начальный сектор
 */
#define START_SECTOR 	18

/*
 * последний сектор
 */
#define END_SECTOR 		58

/*
 * длинна записи, последний байт маркер что это
 * последняя запись или нет
 */
#define LINE_SIZE	13

/*
 * выровняная длинна записи. Использование этого
 * значения связано с тем что функция записи
 * данных во spi flash пишет по 4 байта. Определяется
 * следующим образом: если длинна записи делится
 * нацело на 4 тогда  ALIGN_LINE_SIZE = LINE_SIZE,
 * если не делится то нужно округлить до ближайшего
 * большего значения так чтобы делилось нацело
 * Пример.
 * 		LINE_SIZE	14 тогда  ALIGN_LINE_SIZE 	16
 * 		LINE_SIZE	4 тогда  ALIGN_LINE_SIZE 	4
 */
#define ALIGN_LINE_SIZE 	16

/*
 * маркер последней записи
 */
#define MARKER_ENABLE		 3
#define MARKER_DISABLE		 0


typedef enum {
		WRITE_OK = 0,
		NOT_ENOUGH_MEMORY,
		WRITE_FAIL
} writeres ;


typedef enum {
		OPERATION_FAIL = 0,
		OPERATION_OK
} operationres ;


writeres ICACHE_FLASH_ATTR insert( uint8_t *line );
uint32_t ICACHE_FLASH_ATTR findLine( uint8_t *line );
operationres ICACHE_FLASH_ATTR delete( uint8_t *line );
operationres ICACHE_FLASH_ATTR clearSectors( void );

#endif /* INCLUDE_DRIVER_MYDB_H_ */
