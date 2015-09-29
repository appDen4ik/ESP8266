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


LOCAL ICACHE_FLASH_ATTR void maskBitsInField ( uint8_t* adressOfField, uint16_t startAbsovuleAdress, uint16_t endAbsovuleAdress );
LOCAL ICACHE_FLASH_ATTR uint8_t CompareStrings( uint8_t* firstString, uint8_t* secondString );


// ��������� ���������� ���������� � ���, ��� ����� �� �������� spi_flash_write,
// ������ ����� ��� �� ������� ���� ��� ��� ��������� ���������� �� ������� �����)


// ����� ����� ��������� ���������� ��������� �������� �� ������� spi_flash_write
LOCAL uint8_t tmp[SPI_FLASH_SEC_SIZE];



/*
 ****************************************************************************************************************
 *  result ICACHE_FLASH_ATTR insert( uint8_t *line )
 *  ������� ����������:
 *  WRONG_LENGHT       - ������ ������� ������ ���������� �� ������������ LINE_SIZE
 *  OPERATION_FAIL     - �� ���������� ���������/�������� ������
 *  LINE_ALREADY_EXIST - ����� ������ ��� ����
 *  NOT_ENOUGH_MEMORY  - ������ ���������
 *  OPERATION_OK
 *
 *  @Description:
 *  	��������:
 *    - ����� ������ ������ ���� ����� LINE_SIZE (������������� ������ ������)
 *    - ��������� �� ���������� �� ������ ������ � ���� (������ �� ����������)
 *
 * 	    ����� ���������� �����:
 * 	    - �������� ������ ������ ���������� ������ (START_SECTOR), ������ ��������� ������ � �������. ��� �����
 * 	      �� �������:
 * 	         1) ALIGN_STRING_SIZE * i - 1,  ����
 * 	         2) ALIGN_STRING_SIZE * i - ( 4 - ( STRING_SIZE % 4 ) + 1 ),                     (i = 1, 2...n)
 *		          1 - ������ ������ ������ 4, 2 - ������ ������ �� ������ 4
 *        ����������� ������� ���� ������������� �������, ���� ��� ��� �� ����������� ������ �� ��� ���� ������
 *        � ���� ������, ���� �� - �� ������������ ������ � ������� ������ � ������� ����������� ���� ������.
 *        ���� �� ��� ���� ������ �� �������, �� �������� ��������� ������ � ����������� ��� ��������� ������
 *        � ������. ���� ����� �� ����� ���������� ����������� ������� �� ������ � ���� ������ ������� ����������
 *        ������ - ������ ������� ������ �� ����� � ��������� ���� ������.
 ****************************************************************************************************************
*/
result ICACHE_FLASH_ATTR
insert( uint8_t *string ) {

	uint8_t alignLine[ALIGN_STRING_SIZE];
	uint8_t currentSector;
	uint32_t i;


	if ( strlen( string ) + 1 != STRING_SIZE ) {     // ������ �������� ������ �� ������ ���� ������ ������������ ������ LINE_SIZE
		return WRONG_LENGHT;				       // ������� strlen ���������� ������ ��� ����� \0
	}

	switch ( findString( string ) ) {                  // ����� ������ ��� ����������
		case OPERATION_FAIL:
			 return OPERATION_FAIL;
		case WRONG_LENGHT:
		 	 return WRONG_LENGHT;
		case NOTHING_FOUND:
			break;
		default:
		    return LINE_ALREADY_EXIST;
	}

	for ( i = STRING_SIZE; i < ALIGN_STRING_SIZE; i++ ) {
		alignLine[i] = 0xff;
	}

	memcpy( alignLine, string, STRING_SIZE );

	for ( currentSector = START_SECTOR; currentSector <= END_SECTOR; currentSector++ ) {

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
			return OPERATION_FAIL;
		}

		for ( i = 0; i + ALIGN_STRING_SIZE <= SPI_FLASH_SEC_SIZE ; i += ALIGN_STRING_SIZE ) { // ���� ��������� ������ � ������� �������

#if STRING_SIZE % 4 == 0
			if ( END_OF_SRING != tmp[i + ALIGN_STRING_SIZE - 1] ) {                         // ������ �����������

#else
			if ( END_OF_SRING != tmp[ i + ALIGN_STRING_SIZE - ( 4 - ( STRING_SIZE % 4 ) + 1 ) ] ) {
#endif
				if ( SPI_FLASH_RESULT_OK !=  \
							       spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + i, (uint32 *)alignLine, ALIGN_STRING_SIZE ) ) {
						return OPERATION_FAIL;
					}

				  return OPERATION_OK;

			}

	    }

   }

	return NOT_ENOUGH_MEMORY;
}


/*
 ****************************************************************************************************************
 * result ICACHE_FLASH_ATTR update( uint8_t *oldString, uint8_t *newString )
 * ������� ����������:
 * WRONG_LENGHT			-	�������� ������ ����� �� �������� �����
 * NOTHING_FOUND		- 	oldString ������ �� ����������
 * OPERATION_FAIL		-	������ ����� �� ������� ������ � ����� �������
 * LINE_ALREADY_EXIST	-   newString ������ ��� ����������
 * OPERATION_OK
 *
 * @Description:
 *	��������:
 *	- �������� �������� ����� �� ��������� �� ������
 *	- �������� ��� newString ����� ������������� ������ ������
 *	- �������� ��� oldString ���������� � ��
 *	- �������� ��� newString �� ���������� � ��
 *
 *
 *		������������ ��������� ������ � ������� ��������� ������ � ������. � ������� oldString ���������� ��
 *	newString. ������ � ������� ���������� ����������� ������ ����������. ������ �� ������� ������������ � ����
 *  ������.
 ****************************************************************************************************************
 */
result ICACHE_FLASH_ATTR
update( uint8_t *oldString, uint8_t *newString ) {

	uint32_t  adressRequestingString;
	uint32_t  startAdressSector;							// ������ ������� � ������� ��������� ������������� ������
	uint8_t   stringLenght;

	if ( ( stringLenght = strlen( newString ) ) != strlen( oldString ) ) {
		return WRONG_LENGHT;   								 	           // ������ ����� ���������� ������
	}
	if ( ( stringLenght + 1 ) != STRING_SIZE ) {                           //strlen ���������� ������ ��� ����� \0
		return WRONG_LENGHT;   							 		           // ������ ����������� ������ �� ������ �������
	}

	switch ( adressRequestingString = findString( oldString ) ) {       // ���� oldString ����������� ������ ��������� �� �����
		case NOTHING_FOUND:
			return NOTHING_FOUND;
		case WRONG_LENGHT:
			return WRONG_LENGHT;
		case OPERATION_FAIL:
			return OPERATION_FAIL;
		default:
			break;
	}

	switch ( findString( newString ) ) {        // ���� newString ��� ���� ������ ��������� �� �����
		case NOTHING_FOUND:
			break ;
		case WRONG_LENGHT:
			return WRONG_LENGHT;
		case OPERATION_FAIL:
			return OPERATION_FAIL;
		default:
			return LINE_ALREADY_EXIST;
	}

	startAdressSector = ( adressRequestingString / SPI_FLASH_SEC_SIZE ) * SPI_FLASH_SEC_SIZE;

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( startAdressSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
		return OPERATION_FAIL;
	}

	memcpy( &tmp[ adressRequestingString - startAdressSector ], newString, strlen( newString ) );
																					                      // ���������� �������
	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( startAdressSector /  SPI_FLASH_SEC_SIZE ) ) {
		return OPERATION_FAIL;
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_write( startAdressSector, (uint32 *)&tmp,  SPI_FLASH_SEC_SIZE ) ) {
		return OPERATION_FAIL;
	}

	return OPERATION_OK;

 }


/*
 * operationres ICACHE_FLASH_ATTR delete( uint8_t *line )
 * ������� ����� ������ �� flash ������, ���� ������ �� ������� �� ������ �� ������, � ������� �� �������;
 * ���� �� ������ ������� �� ����� ���������� ������ � ������� ���������� ������, �������� ������ � ������,
 * ����� ������ ��������� ������ ����� ������������� � ������, �������� ������ ������� � ������� �������
 * ���������� �� ���� ������ �����, �������� ������, �������� ������ � ������� � ������ flash
 */
result ICACHE_FLASH_ATTR
delete( uint8_t *removableString ) {

	uint32_t  strAdrOfSecThatContRemLine;      // startAdressOfSectorThatContainsRemovableLine
	uint32_t  absAdrOfRemLineInFlash;          // absoluteAdressOfRemovableLineInFlash
	uint16_t  reltAdrOfRemLine;                // relativeAdressOfRemovableLine ����� ��������� ������ ������������
	                                           // ������ ������� ( ���� ����� ��� ������������ ������ tmp[] )
	uint8_t step;
	uint16_t lastStr;
	uint16_t i;

	if ( strlen( removableString ) + 1 != STRING_SIZE ) {		// ������ ����������� ������ �� ������
			return OPERATION_FAIL;   					// ������� ������ LINE_SIZE �� ����� ����������
	}

	switch ( absAdrOfRemLineInFlash = findString( removableString ) ) {  // ���� oldString ����������� ������ ��������� �� �����
		case NOTHING_FOUND:
			return NOTHING_FOUND;
		case WRONG_LENGHT:
			return WRONG_LENGHT;
		case OPERATION_FAIL:
			return OPERATION_FAIL;
		default:
			break;
	}
          	                                                        // ��������� ������ ������� � ������� ��������� ������
	strAdrOfSecThatContRemLine = ( absAdrOfRemLineInFlash / SPI_FLASH_SEC_SIZE ) * SPI_FLASH_SEC_SIZE;

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( strAdrOfSecThatContRemLine, (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
				return OPERATION_FAIL;
	}

#if STRING_SIZE % 4 == 0
	step = 1;
#else
	step = 4 - ( STRING_SIZE % 4 ) + 1;
#endif

	reltAdrOfRemLine = absAdrOfRemLineInFlash - strAdrOfSecThatContRemLine;                          // ������ ������ � �������

											                    // ���� ��� ������ ��������� � ������� �� ����� ������ ��������
	if ( ( reltAdrOfRemLine + 2 * ALIGN_STRING_SIZE ) > SPI_FLASH_SEC_SIZE \
			                           || tmp[ reltAdrOfRemLine + 2 * ALIGN_STRING_SIZE - step ] !=  END_OF_SRING ) {

		for ( i = 0; i < ALIGN_STRING_SIZE; i++ ) {
			tmp[ reltAdrOfRemLine + i ] = 0xff;
		}

	} else {
																								      // ����� ��������� ������
	    for ( lastStr = reltAdrOfRemLine + ALIGN_STRING_SIZE - step; lastStr == END_OF_SRING; lastStr += ALIGN_STRING_SIZE ) {

	    }

	    lastStr -= ALIGN_STRING_SIZE;
			                               //������ ������� ��������� ������ �� ��������� �������� �� ������ ����� ������ �����
	    for ( i = 0; i <= lastStr; i++ ) {
			    tmp[ reltAdrOfRemLine + i ] = tmp[ reltAdrOfRemLine + ALIGN_STRING_SIZE + i ];
	   }

	}
																		// ���������� ������� � ��������� �������
	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( strAdrOfSecThatContRemLine / SPI_FLASH_SEC_SIZE ) ) {
		return OPERATION_FAIL;
	}

	if ( SPI_FLASH_RESULT_OK != \
				spi_flash_write( ( strAdrOfSecThatContRemLine / SPI_FLASH_SEC_SIZE ), (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
		return OPERATION_FAIL;
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
/*result ICACHE_FLASH_ATTR
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

*\

/*
 ****************************************************************************************************************
 * result ICACHE_FLASH_ATTR clearSectorsDB( void )
 * ������� ����������:
 * OPERATION_FAIL  - �� ���������� �������� ������
 * OPERATION_OK    - ������ ������ � �������������
 *
 * @Description:
 *
 ****************************************************************************************************************
 */
result ICACHE_FLASH_ATTR
clearSectorsDB( void ) {

	uint8_t currentSector = START_SECTOR;

	for ( ; currentSector <= END_SECTOR; currentSector++ ) {

		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( currentSector )  ) {
			return OPERATION_FAIL;
		}

	}
	return OPERATION_OK;
 }



/*
 ****************************************************************************************************************
 * uint32_t ICACHE_FLASH_ATTR findString( uint8_t *string )
 * ������� ����������:
 * WRONG_LENGHT    - ������ ������� ������ �� ������� � ������������
 * OPERATION_FAIL  - �� ���������� ��������� ������
 * NOTHING_FOUNDED - ������ �� �������
 * ���� ���������� ������ ������, ���� ������� ����������
 *
 * @Description:
 * 	������ ��������� ������ START_SECTOR � ���, ��������� ��� ������ �� ������
 * 	���� ������, �� ������ ���������. ���� �� ������ �� ������ �� ��������������� ���������� ������ �� ��� ���:
 * 		- ���� �� ����� ������� ������� ������ ( strcmp ������ 0 )
 * 		- ���� ���� ������� �� ������ �� ����� �������. � ����� ������ �������� ���� ������ � �����������
 * 		  ��������� ������� � ��� �����, ���� ���� �� �������� ������������� ������, ���� ������� �� ��������
 * 		  ��� ���������� ������
 ***************************************************************************************************************
 */
uint32_t ICACHE_FLASH_ATTR
findString( uint8_t *string ) {

	uint8_t currentSector = START_SECTOR;
	uint16_t i;

	if ( ( strlen( string ) + 1 ) != STRING_SIZE ) {     // ������ �������� ������ �� ������ ���� ������ ������������ ������ LINE_SIZE
		return WRONG_LENGHT;
	}

	for ( ; currentSector <= END_SECTOR; currentSector++ ) {                // ���� �� ����� �� ����� ���������� ������

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
			return OPERATION_FAIL;
		}

#if STRING_SIZE % 4 == 0
			if ( END_OF_SRING == tmp[ ALIGN_STRING_SIZE - 1 ] ) {                         // ������ �� ������
#else
			if ( END_OF_SRING == tmp[ ALIGN_STRING_SIZE - ( 4 - ( STRING_SIZE % 4 ) + 1 ) ] ) {
#endif
				for ( i = 0; i + ALIGN_STRING_SIZE <= SPI_FLASH_SEC_SIZE; i += ALIGN_STRING_SIZE ) {   // ���� �� ����� �� ����� �������

#if STRING_SIZE % 4 == 0
					if ( END_OF_SRING != tmp[ i + ALIGN_STRING_SIZE - 1 ] ) {                              // ������ ������
						break;
					}
#else
					if ( END_OF_SRING != tmp[ i + ALIGN_STRING_SIZE - ( 4 - ( STRING_SIZE % 4 ) + 1 ) ] ) {
						break;
					}
#endif

					if ( 0 == strcmp( string, &tmp[i] ) ) {
						return currentSector * SPI_FLASH_SEC_SIZE + i;                                 // ������� ����������
					}

		    }

		}

	}
	return NOTHING_FOUND;                                 // ���� ����� �� ���� ������� ������ ���������� �� ���� �������
 }



result ICACHE_FLASH_ATTR
query( struct espconn* transmiter ) {

 return 0;
}

ICACHE_FLASH_ATTR
void maskBitsInField ( uint8_t* adressOfField, uint16_t startAbsovuleAdress, uint16_t endAbsovuleAdress ) {

	uint16_t i;

	for ( i = 0; i < STRING_SIZE - 1; i++ ) {
		if ( i == startAbsovuleAdress ) {
			i = endAbsovuleAdress + 1;
		}

		adressOfField[i] = '\r';
	}

}

// ��������� ����� (������ ������ ������������� �� \r ���� \n ���� \0)
// 1 - true
// 0 - false
ICACHE_FLASH_ATTR
uint8_t CompareStrings( uint8_t* firstString, uint8_t* secondString ) {

	while( *firstString != '\r' && *firstString != '\n' && *firstString != '\0' ) {
		if ( *firstString++ != *secondString++ ) { return 0; }
	}

	if ( *(firstString - 1) == *(secondString - 1) ) { return 1; }
	else { return 0; }
}


