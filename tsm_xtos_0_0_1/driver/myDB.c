/**
 ************************************************************************************
  * @file    myDB.c
  * @author  Denys
  * @version V0.0.1
  * @date    6-Sept-2015
  * @brief
 ************************************************************************************
 * @info:
 * 		����� ����������� ������ �������� ������� ����������� ��� ������ � �����
 * 		- ����� ������
 * 		- ������� ������
 * 		- ������� ������
 * 		- �������� ���������� ������� ��� �������� ����
 * @note:
 * 		��� ��������� ������ � ����� ������� ������ ������� � ASCII (1251)
 ************************************************************************************
 */

#include "driver/myDB.h"
#include "c_types.h"
#include "spi_flash.h"
#include "osapi.h"


/*
 *  - ��-������ ��������� ��� ������ ������ �������� �������,
 * 	- ���������������� ����������� �������� ����� ������ � ���
 * 	- ����� ����� ���� ��� ��������� ������ � ���, ���� ���������
 * 	  �����, ��������� �������:
 *
 * 	- ����� ��������� ������ EMPTY_SECTOR, ���� ������ ������
 * 	  �� ������ ������ �� ��������� 1 ���� ( ������ EMPTY_SECTOR )
 * 	  ���� ������ �� ������ �� ���� ��������� ������, ��� �����
 * 	  ������ ��������� ���� ������ ( ������ )
 */

// ���������� ����������� ��������� � ���
static uint8_t emptySector[4] = { EMPTY_SECTOR, 0xff, 0xff, 0xff };

writeres ICACHE_FLASH_ATTR
writeLine( uint8_t *line ) {

	uint8_t currentSector = START_SECTOR;
	uint8_t tmp[SPI_FLASH_SEC_SIZE] = { 0 };
	uint16_t len = strlen(line);

	if ( len > SPI_FLASH_SEC_SIZE || len > LINE_SIZE ) {
		return WRITE_FAIL;
	}

	line[LINE_SIZE - 1] = LAST_LINE; // ���������� ������

	for ( ; currentSector <= END_SECTOR; currentSector++ ) {

		// ������ ������ ���� ������ � ���
		if ( SPI_FLASH_RESULT_OK != spi_flash_read( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)tmp, SPI_FLASH_SEC_SIZE) ) {
			return WRITE_FAIL;
		}

		// ��������� ������ ������� EMPTY_SECTOR
		if ( EMPTY_SECTOR == tmp[0] ) {// ������ ������
				// �������� ������
			spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)LINE_NOT_LAST,  1);
				// ������ ������
			spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + 1, (uint32 *)line, LINE_SIZE );
			return WRITE_OK;
		} else if ( SECTOR_NOT_EMPTY == tmp[0] ){// ������ �� ������, ���� ��������� ������
			uint16_t counter;	// counter = LINE_SIZE - ������ ��������� ��� ������ ������
			for ( counter = LINE_SIZE; counter < SPI_FLASH_SEC_SIZE; counter += LINE_SIZE ){
				if ( LAST_LINE == tmp[counter] ) { // ���� ��� ������ ���������
					 // �������� ������
					spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + counter, (uint32 *)LINE_NOT_LAST,  1);
						// ������ ������
					spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + counter + 1, (uint32 *)line,  LINE_SIZE);
					return WRITE_OK;
				}
			}
		}

	}

	return NOT_ENOUGH_MEMORY;
 }

operationres ICACHE_FLASH_ATTR foundLine( uint8_t *line ) {

 }

operationres ICACHE_FLASH_ATTR delLine( uint8_t *line ) {

 }

operationres ICACHE_FLASH_ATTR cleanAllSectors( void ) {
	uint8_t currentSector = START_SECTOR;
	for ( ; currentSector <= END_SECTOR; currentSector++ ){
		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( currentSector )  ) {
			return OPERATION_FAIL;
		}
		spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&emptySector, 1 ); // ������ ����� ��� ������ ������
	}

	return OPERATION_OK;
 }
