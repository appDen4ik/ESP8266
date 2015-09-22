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
#define START_SECTOR 	19

/*
 * ��������� ������
 */
#define END_SECTOR 		58

/*
 * ������ ������, ��������� ���� ������ ��� ���
 * ��������� ������ ������� ��� ���
 */
#define LINE_SIZE	101


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
#define ALIGN_LINE_SIZE 	104//( ( LINE_SIZE % 4 ) + LINE_SIZE )

/*
 * ������ ��������� ������
 */
//#define MARKER_ENABLE		 3
//#define MARKER_DISABLE		 0

#define START_OF_TEXT	2

#define END_OF_SRING     0


typedef enum {
		OPERATION_FAIL = 0,
		WRONG_LENGHT,
		NOTHING_FOUND,
		NOT_ENOUGH_MEMORY,
		LINE_ALREADY_EXIST,
		OPERATION_OK
} result;



result ICACHE_FLASH_ATTR insert( uint8_t *line );
uint32_t ICACHE_FLASH_ATTR findLine( uint8_t *line );
result ICACHE_FLASH_ATTR delete( uint8_t *line );
result ICACHE_FLASH_ATTR clearSectorsDB( void );  					      //tested
result ICACHE_FLASH_ATTR update( uint8_t *oldLine, uint8_t *newLine );
result ICACHE_FLASH_ATTR requestLine( uint8_t *line );

#endif /* INCLUDE_DRIVER_MYDB_H_ */
