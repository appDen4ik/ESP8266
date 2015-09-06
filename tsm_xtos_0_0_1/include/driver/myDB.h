/**
 ************************************************************************************
  * @file    myDB.h
  * @author  Denys
  * @version V0.0.1
  * @date    6-Sept-2015
  * @brief
 ************************************************************************************
**/

#ifndef INCLUDE_DRIVER_MYDB_H_
#define INCLUDE_DRIVER_MYDB_H_

#include "driver/myDB.h"
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
#define LINE_SIZE	14

/*
 * маркеры секторов
 */
#define EMPTY_SECTOR		 3
#define SECTOR_NOT_EMPTY	 0

/*
 * маркеры записей
 */
#define LAST_LINE            3
#define LINE_NOT_LAST	       0



typedef enum {
		WRITE_OK = 0,
		NOT_ENOUGH_MEMORY,
		WRITE_FAIL
} writeres ;


typedef enum {
		OPERATION_FAIL = 0,
		OPERATION_OK
} operationres ;


writeres ICACHE_FLASH_ATTR writeLine( uint8_t *line );
operationres ICACHE_FLASH_ATTR foundLine( uint8_t *line );
operationres ICACHE_FLASH_ATTR delLine( uint8_t *line );
operationres ICACHE_FLASH_ATTR cleanAllSectors( void );

#endif /* INCLUDE_DRIVER_MYDB_H_ */
