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
 * ��������� ������
 */
#define START_SECTOR 	18

/*
 * ��������� ������
 */
#define END_SECTOR 		58

/*
 * ������ ������, ��������� ���� ������ ��� ���
 * ��������� ������ ��� ���
 */
#define LINE_SIZE	13

/*
 * ���������� ������ ������. ������������� �����
 * �������� ������� � ��� ��� ������� ������
 * ������ �� spi flash ����� �� 4 �����. ������������
 * ��������� �������: ���� ������ ������ �������
 * ������ �� 4 �����  ALIGN_LINE_SIZE = LINE_SIZE,
 * ���� �� ������� �� ����� ��������� �� ����������
 * �������� �������� ��� ����� �������� ������
 * ������.
 * 		LINE_SIZE	14 �����  ALIGN_LINE_SIZE 	16
 * 		LINE_SIZE	4 �����  ALIGN_LINE_SIZE 	4
 */
#define ALIGN_LINE_SIZE 	16

/*
 * ������ ��������� ������
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
