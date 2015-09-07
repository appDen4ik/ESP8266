/**
 ************************************************************************************
  * @file    myDB.c
  * @author  Denys
  * @version V0.0.1
  * @date    7-Sept-2015
  * @brief
 ************************************************************************************
 * @info:
 * 		����� ����������� ������ �������� ������� ����������� ��� ������ � �����
 * 		- ����� ������
 * 		- ������� ������
 * 		- ������� ������
 * 		- �������� ���������� ������� ��� �������� ����
 * @note:
 * 	-	��� ��������� ������ � ����� ������� ������ ������� � ASCII (windows - 1251)
 * 	-	� �������� spi_flash_write � spi_flash_read ������ � ������ ������������
 * 		�� 4 �����, ���� ������� � ���� �������� �������� ������ �� ������ �������
 * 		�� ��� ��� ����� ���������� �� ���������� �������� ��������, ������� ������
 * 		�������, ��� ��������� ������ ��� ����� ���������.
 * 	-	� ��� ��� ������ ������ ��������������� ������ ���������� � ������� �����.
 * 		MARKER_ENABLE - ���� ������ ������, � MARKER_DISABLE - ���� � ������� ����
 * 		������/������. ���� ������ ������ �� ������ ������ ������� �� ������� �����.
 * 		���� ������ �� ������ 1-� ���� = MARKER_DISABLE, �� ���������� �����
 * 		��������� ������. ��� ����� ��������� ���� ������ ������ �������������� ���
 * 		������� MARKER_ENABLE/MARKER_DISABLE
 ************************************************************************************
 */

#include "driver/myDB.h"
#include "c_types.h"
#include "spi_flash.h"
#include "osapi.h"



// ���������� ����������� ��������� � ��� (���� �� ������ ������� ������� �� �������
// ��������� ���������� ���������� � ���, �� ��� ����� �� �������� spi_flash_write,
// ������ ����� ��� �� ������� ���� ��� ��� ��������� ���������� �� ������� �����)
static uint8_t markerEnable[4] = { MARKER_ENABLE, 0xff, 0xff, 0xff };
static uint8_t markerDisable[4] = { MARKER_DISABLE, 0xff, 0xff, 0xff };

// ����� ����� ��������� ���������� ��������� �������� �� ������� spi_flash_write
static uint8_t tmp[SPI_FLASH_SEC_SIZE];


/*
 *  writeres ICACHE_FLASH_ATTR writeLine( uint8_t *line )
 *  - ��-������ ��������� ��� ������ ������ �������� �������,
 * 	- ����� ���������� ���� ������ ����� ������ � ���
 * 	- �����, ���� ����� ��������� ������ � ���� ������ ������,
 *    ������ ��������� � ��� �����
*/
writeres ICACHE_FLASH_ATTR
writeLine( uint8_t *line ) {

	uint8_t currentSector = START_SECTOR;
	uint16_t len = strlen(line) + 1; // ������� ���������� ������ ��� ����� \0
	uint8_t alignLine[ALIGN_LINE_SIZE];

	{ //
		uint16_t i;
		for ( i = 0; i < sizeof(alignLine); i++){
			alignLine[i] = 0xff;
		}
	}

	 // ������ ������ �� ������ �������� �������  SPI_FLASH_SEC_SIZE
	 // ������ �������� ������ �� ������ ���� ������ ������������ ������ LINE_SIZE
	if ( len > SPI_FLASH_SEC_SIZE || len > LINE_SIZE ) { return WRITE_FAIL; }

	// ������ ������ ��������� ��������� ( ��������� ��������������� ������ )
	line[LINE_SIZE - 1] = MARKER_ENABLE;

#ifdef  DB_DEBUG
	ets_uart_printf("Begin write");
#endif
	for ( ; currentSector <= END_SECTOR; currentSector++ ) {

#ifdef  DB_DEBUG
		ets_uart_printf("Begin read");
#endif
		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ){
			return WRITE_FAIL;
		}

#ifdef  DB_DEBUG
		ets_uart_printf("read OK");
		{
		uint16_t i;
		  for (  i = 0; i < SPI_FLASH_SEC_SIZE; i++ ){
			  uart_tx_one_char(tmp[i]);
		  }
		}
#endif
		if ( MARKER_ENABLE == tmp[0] ) {// ������ ������
				// �������� ������
			spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&markerDisable,  1);
				// ������ ������
			memcpy( alignLine, line, len ); // ��������� ������� ����, ���� ����������
			spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + 1, (uint32 *)alignLine, ALIGN_LINE_SIZE );

#ifdef  DB_DEBUG
		ets_uart_printf("read the sector after write first line");
		spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE );
		{
		uint16_t i;
		  for (  i = 0; i < SPI_FLASH_SEC_SIZE; i++ ){
			  uart_tx_one_char(tmp[i]);
		  }
		}

#endif
			return WRITE_OK;
		} else if ( MARKER_DISABLE == tmp[0] ) {// ������ �� ������, ���� ��������� ������
			uint16_t counter;	// counter = LINE_SIZE - ������ ��������� ��� ������ ������
#ifdef  DB_DEBUG
	ets_uart_printf("line not first");
#endif
			for ( counter = LINE_SIZE; counter < SPI_FLASH_SEC_SIZE; counter += LINE_SIZE ){
				if ( MARKER_ENABLE == tmp[counter] ) { // ���� ��� ������ ���������
					 // �������� ������
					spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + counter, (uint32 *)&markerDisable,  1);
						// ������ ������
					memcpy( alignLine, line, len ); // ��������� ������� ����, ���� ����������
					spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + counter + 1, (uint32 *)line,  LINE_SIZE);
#ifdef  DB_DEBUG
		ets_uart_printf("read the sector after write first line");
		spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE );
		{
		uint16_t i;
		  for (  i = 0; i < SPI_FLASH_SEC_SIZE; i++ ){
			  uart_tx_one_char(tmp[i]);
		  }
		}

#endif
					return WRITE_OK;
				}
			}
		}
	}

	return NOT_ENOUGH_MEMORY;
 }

operationres ICACHE_FLASH_ATTR
cleanAllSectors( void ) {
	uint8_t currentSector = START_SECTOR;
	for ( ; currentSector <= END_SECTOR; currentSector++ ){
		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( currentSector )  ) {
			return OPERATION_FAIL;
		}
		spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&markerEnable, 1 ); // ������ ����� ��� ������ ������
	}

	return OPERATION_OK;
 }

operationres ICACHE_FLASH_ATTR
foundLine( uint8_t *line ) {

 }

operationres ICACHE_FLASH_ATTR
delLine( uint8_t *line ) {

 }


