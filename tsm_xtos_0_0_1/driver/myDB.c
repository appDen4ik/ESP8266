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
 *  	     ������ ���� ������������� �������� \n - ������� �� ����� ������, � ���������� � 2 START_OF_TEXT
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


#include "user_interface.h"

#include "driver/myDB.h"
#include "c_types.h"
#include "spi_flash.h"
#include "osapi.h"
#include "espconn.h"


ICACHE_FLASH_ATTR static void maskBitsInField ( uint8_t* adressOfField, uint16_t startAbsovuleAdress, uint16_t endAbsovuleAdress );


// ���������� ����������� ��������� � ��� (���� �� ������ ������� ������� �� �������
// ��������� ���������� ���������� � ���, �� ��� ����� �� �������� spi_flash_write,
// ������ ����� ��� �� ������� ���� ��� ��� ��������� ���������� �� ������� �����)
static uint8_t markerEnable[4] = { MARKER_ENABLE, 0xff, 0xff, 0xff };
static uint8_t markerDisable[4] = { MARKER_DISABLE, 0xff, 0xff, 0xff };

// ����� ����� ��������� ���������� ��������� �������� �� ������� spi_flash_write
static uint8_t tmp[SPI_FLASH_SEC_SIZE];


/*
 *  writeres ICACHE_FLASH_ATTR insert( uint8_t *line )
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
result ICACHE_FLASH_ATTR
insert( uint8_t *line ) {

	uint8_t currentSector = START_SECTOR;
	uint16_t len = strlen(line) + 1;              // ������� ���������� ������ ��� ����� \0
	uint8_t alignLine[ALIGN_LINE_SIZE];
	uint16_t i;


	if ( len != LINE_SIZE ) {                   // ������ �������� ������ �� ������ ���� ������ ������������ ������ LINE_SIZE
		return WRONG_LENGHT;
	}

	if ( NOTHING_FOUNDED != findLine( line ) ) {
		return LINE_ALREADY_EXIST;                       // ����� ������ ��� ����
	}

	for ( i = 0; i < ALIGN_LINE_SIZE; i++){
		alignLine[i] = 0xff;
	}

	line[LINE_SIZE - 1] = MARKER_ENABLE;        // ������ ������ ��������� ��������� ( ��������� ��������������� ������ )

	memcpy( alignLine, line, LINE_SIZE );


	for ( ; currentSector <= END_SECTOR; currentSector++ ) {

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ){
			return OPERATION_FAIL;
		}

		if ( MARKER_ENABLE == tmp[0] ) {                                                               // ������ ������

			spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&markerDisable,  1);          // �������� ������

			spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + 1, (uint32 *)alignLine, ALIGN_LINE_SIZE );

			return OPERATION_OK;

		} else if ( MARKER_DISABLE == tmp[0] ) {                                  // ������ �� ������, ���� ��������� ������

			i = LINE_SIZE;                                        // counter = LINE_SIZE - ������ ��������� ��� ������ ������

			for ( i = LINE_SIZE; i < SPI_FLASH_SEC_SIZE; i += LINE_SIZE ){

				if ( MARKER_ENABLE == tmp[i] ) {                                 // ���� ��� ������ ���������

					spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + i, (uint32 *)&markerDisable,  1);   // �������� ������

					spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + i + 1, (uint32 *)alignLine,  ALIGN_LINE_SIZE);

					return OPERATION_OK;

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
result ICACHE_FLASH_ATTR
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

	if ( OPERATION_FAIL == ( adress = findLine(oldLine) ) ){
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
result ICACHE_FLASH_ATTR
delete( uint8_t *removableLine ) {
	uint8_t   strAdrOfSecThatContRemLine;     // startAdressOfSectorThatContainsRemovableLine
	uint32_t  absAdrOfRemLineInFlash;         // absoluteAdressOfRemovableLineInFlash
	uint16_t  reltAdrOfRemLine;               // relativeAdressOfRemovableLine ����� ��������� ������ ������������
	                                          // ������ ������� ( ���� ����� ��� ������������ ������ tmp[] )
	uint16_t i;

	if ( strlen( removableLine ) != LINE_SIZE ){		// ������ ����������� ������ �� ������
			return OPERATION_FAIL;   					// ������� ������ LINE_SIZE �� ����� ����������
	}

	if ( OPERATION_FAIL == ( absAdrOfRemLineInFlash = findLine( removableLine ) ) ){
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
 * - �� ���� �������� ������ � ������� ��������� ����� ����� � ���������� ����� � ���� ������ �
 *   ������� ���� �� ���� ���� ����� ����� �� ��� � ������� ������. � ������ ���� ������� ������
 *   ������ ���� �������� \r
 *@Description:
 *    - ��������
 *        - ������ ������������� ������ ������ ���� ����� �������� (LINE_SIZE)
 *   	  - � ������������� ������ ������ ���� ��������� ���� �� ���� ����
 *    - ���� �� �������� ������ ��������� ������ ������ ���� �� ����������
 *      ������� ����������� ��� ����� �������, ����� ������ �� ����� �� ����
 *      ������� ����������, ���� ��� �� ������� �� ����� ����������� ��� ����
 *      ����� ������� � ���������� ����� �� �����, � ��� �����, �� ��� ���
 *      ���� �� ����� ������� ������ � ����� �� �����, ���� �� �� ����� �������
 *      ������.
 *
 */
result ICACHE_FLASH_ATTR
requestLine( uint8_t *line) {

	uint8_t currentSector = START_SECTOR;
	uint16_t i, c;
	uint8_t endAdressOfCarrentField, startAdressOfCarrentField;
	uint8_t copyLine[LINE_SIZE];

	                                      // ������ �������� ������ �� ������ ���� ������ ������������ ������ LINE_SIZE
	if ( ( strlen( line ) + 1 ) != LINE_SIZE ) { return OPERATION_FAIL; }

	                                // � ������� ������ ������ ���� ��������� ���� �� ���� ����
	for ( i = 0; i <= LINE_SIZE; i++ ){
		if ( '\n' != tmp[i] || MARKER_ENABLE != tmp[i] ) {		                                // ������ ��������� ������� ��������� ����
			break;
		} else if ( LINE_SIZE == i ){
			return OPERATION_FAIL;
		}
	}
//***********************************************************************************************************************************************



				// ���������� ������������� ������ (���������� ����������� �����)
	for ( i = 0; i < LINE_SIZE; ) {

				// �������� ������������� ������
		memcpy( copyLine, line, LINE_SIZE );

				// ������� ������ ��������� ������  START_OF_TEXT
		for ( i = 0; ; i++) {
			if ( START_OF_TEXT == copyLine[i] ) {
				startAdressOfCarrentField = i;
				break;
			} else if ( '\0' == copyLine[i] || MARKER_ENABLE != copyLine[i] || LINE_SIZE == i) {
				return OPERATION_FAIL;
			}
		}

			    // ������� ����� ��������� ������ \n
		for ( ; ; i++ ) {
			if ( START_OF_TEXT == copyLine[i] ) {
				endAdressOfCarrentField = i;
					break;
				} else if ( '\0' == copyLine[i] || MARKER_ENABLE != copyLine[i] || LINE_SIZE == i) {
					return OPERATION_FAIL;
				}
		}


		maskBitsInField( copyLine, startAdressOfCarrentField, endAdressOfCarrentField );

//************************************************************************************************************************************************


		for ( ; currentSector <= END_SECTOR;  currentSector++) {                  						// ���� �� ����� �� ����� ���������� ������

																												//������ �������
			if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ){
					return OPERATION_FAIL;
			}

			if ( MARKER_DISABLE == tmp[0] ) {   // ������ �� ������
				for ( c = 1; c < SPI_FLASH_SEC_SIZE; c += LINE_SIZE ) { // ���� �� ����� �� ����� �������

					maskBitsInField ( &tmp[c], startAdressOfCarrentField, endAdressOfCarrentField );

					if ( 0 == strcmp( copyLine, &tmp[c] ) ) { return currentSector*SPI_FLASH_SEC_SIZE + c;      // ������� ����������
					}
					else if ( MARKER_ENABLE == tmp[c] ) {
						tmp[c - 1] = MARKER_DISABLE;
						if ( 0 == strcmp( copyLine, &tmp[c] ) ) { return currentSector*SPI_FLASH_SEC_SIZE + c;  // ������� ����������
						} else { break; }
					}

				}
			}
		}

	}

	return OPERATION_FAIL;        // ���� ����� �� ���� ������� ������ ���������� �� ���� �������
}



/*
 ****************************************************************************************************************
 * result ICACHE_FLASH_ATTR clearSectorsDB( void )
 * ������� ����������:
 * OPERATION_FAIL  - �� ���������� �������� ������, ���� ��������� �����
 * OPERATION_OK    - ������ ������ � �������������
 *
 * @Description:
 *
 ****************************************************************************************************************
 */
result ICACHE_FLASH_ATTR
clearSectorsDB( void ) {

	uint8_t currentSector = START_SECTOR;

	for ( ; currentSector <= END_SECTOR; currentSector++ ){

		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( currentSector )  ) {
			return OPERATION_FAIL;
		}
		                                                                                      // ������ ����� ��� ������ ������
	    if ( SPI_FLASH_RESULT_OK != spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&markerEnable, 1 ) ) {
	    	return OPERATION_FAIL;
	    }
	}

	return OPERATION_OK;
 }




/*
 ****************************************************************************************************************
 * uint32_t ICACHE_FLASH_ATTR findLine( uint8_t *line )
 * ������� ����������:
 * WRONG_LENGHT    - ������ ������� ������ �� ������� � ������������
 * OPERATION_FAIL  - �� ���������� ��������� ������
 * NOTHING_FOUNDED - ������ �� �������
 * ���� ������ ������ ���� ������� ����������
 *
 * @Description:
 * 	������ ��������� ������ START_SECTOR � ���, ��������� ��� ������ �� ������
 * 	���� ������, �� ������ ���������. ���� �� ������ �� ������ �� ��������������� ���������� ������ �� ��� ���:
 * 		- ���� �� ����� ������� ������� ������ ( strcmp ������ 0 )
 * 		- ���� ���� �� ���������� ��� ���������� ������
 * 	���� strcmp ������ �������� != 0, ��� ����� ����������������� � ��� ��� ��� ������ ��������� � ���� �������
 * (��� ������������ �� 0�03 � �� �� 0�00). � ����� ������ ��������� ������ ���������� ��������� ������ �������:
 *      - ������ � ��� ������ 0�03 �������� 0�00, ���� �� ������ �� ������� �� ������ ����. ������ � ��� �����,
 *        ���� �� �������� ��� ������������ ������� ������� END_SECTOR, ��� �� ������ ������
 ***************************************************************************************************************
 */
uint32_t ICACHE_FLASH_ATTR
findLine( uint8_t *line ) {

	uint8_t currentSector = START_SECTOR;
	uint16_t i;

	if ( ( strlen( line ) + 1 ) != LINE_SIZE ) { // ������ �������� ������ �� ������ ���� ������ ������������ ������ LINE_SIZE
		return WRONG_LENGHT;
	}

	for ( ; currentSector <= END_SECTOR;  currentSector++) {                // ���� �� ����� �� ����� ���������� ������

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ){
				return OPERATION_FAIL;
		}

		if ( MARKER_DISABLE == tmp[0] ) {                                                  // ������ �� ������
		    for ( i = 1; i < SPI_FLASH_SEC_SIZE; i += LINE_SIZE ) {                       // ���� �� ����� �� ����� �������
			    if ( 0 == strcmp( line, &tmp[i] ) ) {
			    	return currentSector*SPI_FLASH_SEC_SIZE + i;                            // ������� ����������
			    }
			    else if ( MARKER_ENABLE == tmp[i + LINE_SIZE - 1] ) {                       // ������ ��������� � �������
				    tmp[i + LINE_SIZE - 1] = MARKER_DISABLE;
				    if ( 0 == strcmp( line, &tmp[i] ) ) {
				    	return currentSector*SPI_FLASH_SEC_SIZE + i;                        // ������� ����������
				    } else {
				    	break;
				    }
			    }
		    }
		}
	}

	return NOTHING_FOUNDED;                                 // ���� ����� �� ���� ������� ������ ���������� �� ���� �������
 }




result ICACHE_FLASH_ATTR
query( struct espconn* transmiter ) {

 return 0;
}

ICACHE_FLASH_ATTR
void maskBitsInField ( uint8_t* adressOfField, uint16_t startAbsovuleAdress, uint16_t endAbsovuleAdress ) {

	uint16_t i;

	for ( i = 0; i < LINE_SIZE - 1; i++ ) {
		if ( i == startAbsovuleAdress ) {
			i = endAbsovuleAdress + 1;
		}

		adressOfField[i] = '\r';
	}


}




