/**
 ************************************************************************************************************************
  * @file    myDB.c
  * @author  Denys
  * @version V0.0.1
  * @date    7-Sept-2015
  * @brief
 ************************************************************************************************************************
 * @info:
 * 		����������� ������� ����������� ��� ������ � �����
 * 		- ����� ������ �� ������ 	(static)	- findLine();
 * 		- ����� ������ �� ����/�����			- requestLine();
 * 		� ��� ������ � ����� ������:
 * 		- �������� ���������� ������� ��� �������� ����		- clearSectorsDB();
 * 		- ������� ������                       				- insert();
 * 		- ������� ������                       				- delete();
 * 		- �������� ������                      				- update();
 * 		- ��������� ���� ������ (������������) 				- query();
 *
 * @note:
 * 	-	��� ��������� ������ � ����� ������� ������ ������� � ASCII (windows - 1251)
 *  -   ��������� ������ � �� ���������:
 *  	-    ��� �������� �� �������� ����������� ������� spi flash ������ (���������� �������� ������������ ��������
 *  	     �������� END_SECTOR - START_SECTOR + 1)
 *  	-    �� ������� �� �������, ������ ���� ������� ���������� (������ ������ ������������ LINE_SIZE). ��� ���
 *  	     ������ ������ �� ������ ��������� ������� ������� SPI_FLASH_SEC_SIZE. ������� ����������� ��������
 *  	     ������� � ������� ����� (�� � 0-��, � ���� ����� ��������� ��������� ���� ��� ������ ������, ���� �
 *  	     ��� ���� ������). ������ ������ ������������� �������� \0 ���� ������ 3 (���� ������ ASCII).
 *  	-    ������ ������� �� �����. ���������� ����� �� ���� ������� ���������� � ��� ���������� ������.
 *  	     ������ ������ ������������� �������� \n - ������� �� ����� ������
 * 	-	� �������� spi_flash_write � spi_flash_read ������ � ������ ������������ �� 4 �����, ���� ������� � ����
 * 	    �������� �������� ������ �� ������ ������� �� ��� ��� ����� ���������� �� ���������� �������� ��������,
 * 	    ������� ������ �������, ��� ��������� ������ ��� ����� ���������.
 * 	-	� ��� ��� ������ ������ ��������������� ������ ���������� � ������� �����. MARKER_ENABLE - ���� ������ ������,
 * 	    � MARKER_DISABLE - ���� � ������� ���� ������/������. ���� ������ ������ �� ������ ������ ������� �� �������
 * 	    �����. ���� ������ �� ������ 0-� ���� = MARKER_DISABLE, �� ���������� ����� ��������� ������. ��� �����
 * 	    ��������� ���� ������ ������ �������������� ��� ������� MARKER_ENABLE/MARKER_DISABLE: MARKER_ENABLE - ������
 * 	    ���������, MARKER_DISABLE - ������ �� ���������.
 ************************************************************************************************************************
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
 *  @Description:
 *    - ������ ������ ������ ���� ������ �������� �������, � �����
 *      ����� ������ ������ ���� ����� LINE_SIZE (������������� ������ ������)
 *    - ��������� �� ���������� �� ������ ������ � ���� (������ �� ����������)
 * 	  - ����� ���������� ����� - ���� ��� �� ������ �� ����� � ������� �� �������
 *
 * 	    ��������� �������� ��������� ������ ���������� �����:
 * 	    1) �������� ������ ������ ���� (START_SECTOR), ��������� ������ �� ������ ( 0 - ����
 * 	       ���� - ���� ����� MARKER_ENABLE �� ������ ������ ������, ���� ����� MARKER_DISABLE
 * 	       �� � ������� ������� ������)
 * 	    2) ���� 0 - � ���� ���� ����� MARKER_ENABLE, �� � 0 - �  ���� ����� MARKER_DISABLE �
 * 	       �������� ������ ������ � ���� ������� (������� � ������� �������), � ��������� ����
 * 	       ������ ��������������� MARKER_ENABLE (��� ��������������� � ��� ��� ��� ������
 * 	       ���������)
 * 	    3) ���� 0 - � ���� ���� ����� MARKER_DISABLE (������ �� ������), �� ������������ �����
 * 	       ��������� ������ � ���� �������, ����� ����������� ����������� ������� �� ���� ���
 * 	       ���� ������:
 * 	       	- ���� ������� �� ������ ��������� ������ ������ MARKER_DISABLE ������ ����� ������ �
 * 	       	  ��������� ���� ����� ������ ������ MARKER_ENABLE, �� ���� ������� ����������� ���� ������
 * 	       	- ���� �� �������, �� ����� ������ ���� ������ � ��������� �������� ������� � ������� ������
 *
*/
writeres ICACHE_FLASH_ATTR
insert( uint8_t *line ) {

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
	if ( len > SPI_FLASH_SEC_SIZE || len != LINE_SIZE ) { return WRITE_FAIL; }

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
					spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + counter + 1, (uint32 *)alignLine,  ALIGN_LINE_SIZE);
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


/*
 * operationres ICACHE_FLASH_ATTR update( uint8_t *oldLine, uint8_t *newLine )
 * ������� ����� ������ �� flash ������, ���� ������ �� ������� �� ������ �� ������, � ������� �� �������;
 * ���� �� ������ ������� �� ����� ���������� ������ � ������� ���������� ������, �������� ������ � ������,
 * ����� ���������� ���������� ������ ������ � �������, ���������� ��� ������, �������� ������� ������, �
 * ��������� ��� ����� ������� �� �������
 */
operationres ICACHE_FLASH_ATTR
update( uint8_t *oldLine, uint8_t *newLine ) {
	uint32_t adress;
	uint8_t sectorNumber;
	uint8_t *lineAdressInTmp;
	uint8_t lineLemght;

	if ( (lineLemght = strlen(newLine)) != strlen(oldLine) ){
		return OPERATION_FAIL;   								 		// �������� ��� ������ ������� �����
	}
	if ( lineLemght != LINE_SIZE ){
			return OPERATION_FAIL;   							 		// �������� ��� ������ ����������� ������ �� ������
																 	 	 	// ������� ������ ������ �� ����� ����������
		}

	if ( OPERATION_FAIL == ( adress = foundLine(oldLine) ) ){
		return OPERATION_FAIL;												// ���� ������ �� �������, �� ��������� ������
	} else {

		sectorNumber = (adress / SPI_FLASH_SEC_SIZE) * SPI_FLASH_SEC_SIZE ;							// ���������� � ����� ������� ��������� ������

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( sectorNumber , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
			return OPERATION_FAIL;
		}

		lineAdressInTmp = &tmp[ adress - ( SPI_FLASH_SEC_SIZE * sectorNumber )];		// ���������� ������ ������ � ������e

		memcpy( lineAdressInTmp, newLine, strlen(newLine) );				// ��������� ������ (��� ����� ���������� �����)

																			// ������� ������ flash ��� ���������� ��������� -
																			// ���� ������
		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( sectorNumber )  ) {
					return OPERATION_FAIL;
				}
																				// ���������� ������ � flash ������
		spi_flash_write( sectorNumber, (uint32 *)&tmp,  SPI_FLASH_SEC_SIZE);

		return OPERATION_OK;
	}
 }


/*
 * operationres ICACHE_FLASH_ATTR delete( uint8_t *line )
 * ������� ����� ������ �� flash ������, ���� ������ �� ������� �� ������ �� ������, � ������� �� �������;
 * ���� �� ������ ������� �� ����� ���������� ������ � ������� ���������� ������, �������� ������ � ������,
 * ����� ������ ��������� ������ ����� ������������� � ������, �������� ������ ������� � ������� �������
 * ���������� �� ���� ������ �����, �������� ������, �������� ������ � ������� � ������ flash
 */
operationres ICACHE_FLASH_ATTR
delete( uint8_t *removableLine ) {
	uint8_t   strAdrOfSecThatContRemLine;     // startAdressOfSectorThatContainsRemovableLine
	uint32_t  absAdrOfRemLineInFlash;         // absoluteAdressOfRemovableLineInFlash
	uint16_t  reltAdrOfRemLine;               // relativeAdressOfRemovableLine ����� ��������� ������ ������������
	                                          // ������ ������� ( ���� ����� ��� ������������ ������ tmp[] )
	uint16_t i;

	if ( strlen( removableLine ) != LINE_SIZE ){		// ������ ����������� ������ �� ������
			return OPERATION_FAIL;   					// ������� ������ LINE_SIZE �� ����� ����������
	}

	if ( OPERATION_FAIL == ( absAdrOfRemLineInFlash = foundLine( removableLine ) ) ){
		return OPERATION_FAIL;							// ���� ������ �� �������, �� ������ ������� �� ���� �����������
	} else {
		                                                // ��������� ������ ������� � ������� ��������� ������
		strAdrOfSecThatContRemLine = ( absAdrOfRemLineInFlash / SPI_FLASH_SEC_SIZE ) * SPI_FLASH_SEC_SIZE;

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( strAdrOfSecThatContRemLine, (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
				return OPERATION_FAIL;
		}

		reltAdrOfRemLine = absAdrOfRemLineInFlash - strAdrOfSecThatContRemLine;

		                                                           // �� �������� �� ��������� ������ ���������
		if ( MARKER_ENABLE == tmp[ reltAdrOfRemLine + LINE_SIZE - 1 ] ) {

			tmp[ reltAdrOfRemLine - 1 ] = MARKER_DISABLE;          // ���������� ������ ����� ���������

			i = 1;

		} else {
			                                                   // ���������� ��� ������ ������� ��������� ������
			                                                   // �� ��������� �������� �� ������ ����� ������ �����
			for ( i = 0; tmp[ reltAdrOfRemLine + LINE_SIZE + i ] != MARKER_ENABLE; i++ ){
				tmp[ reltAdrOfRemLine + i ] = tmp[ reltAdrOfRemLine + LINE_SIZE + i ];
			}
			tmp[ reltAdrOfRemLine + LINE_SIZE + i ] = MARKER_ENABLE;

			i++;

		}
		                                               	   // ���������� ��������� ��� ���� ������ �� ��������� ������ 0xFF
		for ( ; ( reltAdrOfRemLine + i ) < LINE_SIZE; i++ ){
			tmp[ reltAdrOfRemLine + i ] = 0xFF;
		}

													   	   	   	   	   	   // ���������� ������� � ��������� �������
		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( strAdrOfSecThatContRemLine / SPI_FLASH_SEC_SIZE ) ) {
			return OPERATION_FAIL;
		}

		if ( SPI_FLASH_RESULT_OK != \
				spi_flash_write( ( strAdrOfSecThatContRemLine / SPI_FLASH_SEC_SIZE ), (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {

			return OPERATION_FAIL;
		}
	}

	return OPERATION_OK;
 }


/*
 * operationres ICACHE_FLASH_ATTR requestLine( uint8_t *line)
 * � ������ ���� ������� ������ ������ ���� �������� \r
 * ������� �������� ��������� �������:
 * - �� ���� �������� ������ � ������� ��������� �� ��� ����
 *   � ���������� ����� � ���� ������ � ������� ���� �� ����
 *   ���� ����� ����� �� ��� � ������� ������
 */
operationres ICACHE_FLASH_ATTR
requestLine( uint8_t *line) {

}


operationres ICACHE_FLASH_ATTR
clearSectorsDB( void ) {
	uint8_t currentSector = START_SECTOR;
	for ( ; currentSector <= END_SECTOR; currentSector++ ){
		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( currentSector )  ) {
			return OPERATION_FAIL;
		}
		spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&markerEnable, 1 ); // ������ ����� ��� ������ ������
	}

	return OPERATION_OK;
 }


/*
 * uint32_t ICACHE_FLASH_ATTR foundLine( uint8_t *line )
 * ������� ���������� 0 ���� �� ������� �� ������ ����������
 * ���� ������ ������ ���� ������� ����������
 * Description:
 * 	������ ��������� ������ START_SECTOR � ���, ��������� ��� ������ �� ������
 * 	���� ������, �� ������ ���������. ���� �� ������ �� ������ �� ���������������
 * 	���������� ������ �� ��� ���:
 * 		- ���� �� ����� ������� ������� ������ ( strcmp ������ 0 )
 * 		- ���� strcmp �� ������ �������� != 0, ��� ���������������
 * 		  � ��� ��� ��� ��������� ������ (��� ������������ �� 0�03 � �� �� 0�00).
 * 		  � ����� ������ ��������� ������ ���������� ��������� ������ �������:
 * 		   - ������ � ��� ������ 0�03 �������� 0�00, ���� �� ������ �� �������
 * 		     �� ������ ����. ������ � ��� �����, ���� �� �������� ��� ������������
 * 		     ������� ������� END_SECTOR, ��� �� ������ ������
 *
 */
uint32_t ICACHE_FLASH_ATTR
findLine( uint8_t *line ) {
	uint8_t currentSector = START_SECTOR;
	uint16_t len = strlen(line) + 1; // ������� ���������� ������ ��� ����� \0
	uint8_t *p = &tmp[1];

	 // ������ �������� ������ �� ������ ���� ������ ������������ ������ LINE_SIZE
	if ( len > LINE_SIZE ) { return OPERATION_FAIL; }


	for ( ; currentSector <= END_SECTOR;  currentSector++) {                // ���� �� ����� �� ����� ���������� ������
#ifdef  DB_DEBUG
		ets_uart_printf("Read iteration");
#endif
		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ){
				return OPERATION_FAIL;
#ifdef  DB_DEBUG
		ets_uart_printf("Read fail");
#endif
		}

		if ( MARKER_DISABLE == tmp[0] ) {   // ������ �� ������

		    for ( p = &tmp[1]; p < &tmp[SPI_FLASH_SEC_SIZE]; p += LINE_SIZE ) { // ���� �� ����� �� ����� �������
			    if ( 0 == strcmp( line, p ) ) { return currentSector*SPI_FLASH_SEC_SIZE + (p - &tmp[0]);      // ������� ����������
			    }
			    else if ( MARKER_ENABLE == p[LINE_SIZE - 1] ) {
				    p[LINE_SIZE - 1] = MARKER_DISABLE;
				    if ( 0 == strcmp( line, p ) ) { return currentSector*SPI_FLASH_SEC_SIZE + (p - &tmp[0]);  // ������� ����������
				    } else { break; }
			    }

		   }
		}
	}
#ifdef  DB_DEBUG
		ets_uart_printf("Nothing founded");
#endif
	return OPERATION_FAIL;        // ���� ����� �� ���� ������� ������ ���������� �� ���� �������
 }

operationres ICACHE_FLASH_ATTR
query() {


}





