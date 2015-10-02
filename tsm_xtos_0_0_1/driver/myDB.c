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
 * 		- ����� ������ �� ������ 		        - findString();
 * 		- ����� ������ �� ����/�����			- requestString();
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


static ICACHE_FLASH_ATTR result stringEqual( uint8_t* firstString, uint8_t* secondString ); // ��������� �� ���� ������������ �����


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
	uint8_t step;

#if STRING_SIZE % 4 == 0
	step = 1;
#else
	step = ( 4 - ( STRING_SIZE % 4 ) + 1 );
#endif

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

			if ( END_OF_STRING != tmp[ i + ALIGN_STRING_SIZE - step ] ) {                         // ������ �����������

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

	switch ( adressRequestingString = findString( oldString ) ) {          // ���� oldString ����������� ������ ��������� �� �����
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

	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( startAdressSector /  SPI_FLASH_SEC_SIZE ) ) {  // ���������� �������
		return OPERATION_FAIL;
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_write( startAdressSector, (uint32 *)&tmp,  SPI_FLASH_SEC_SIZE ) ) {
		return OPERATION_FAIL;
	}

	return OPERATION_OK;

}


/*
 ****************************************************************************************************************
 * result ICACHE_FLASH_ATTR delete( uint8_t *removableString )
 * ������� ����������:
 * WRONG_LENGHT			-	�������� ������ ������� ������
 * NOTHING_FOUND		- 	����� ������ �� ����������
 * OPERATION_FAIL		-	������ ����� �� ������� ������ � ����� �������
 * OPERATION_OK
 *
 * @Description:
 *	��������:
 *	- ������ ������� ������ ����� �������������
 *	- ������� ������ ���� � ����
 *
 * 		�� ������� ������� ���������� ������� findString ������������ ������ � ������� ��������� ��������� ������.
 * ������������ ������ � ������. ���� ������ ���������, �� ��� ������ ���������� � ������� (����������� 0�ff).
 * ���� �� ������ �� �������� ��������� ����� ��� ������, ������� ���������� ������ �� ��������� ���������� ��
 * ������ ����� ����������� ������ �����. ����� ������� ������ ������ ����������, ��� ��� ��������� ������ � �� -
 * �������� ������ ��������� ����� � ������� �����������.
 ****************************************************************************************************************
 */
result ICACHE_FLASH_ATTR
delete( uint8_t *removableString ) {

	uint32_t  strAdrOfSecThatContRemLine;      // startAdressOfSectorThatContainsRemovableLine
	uint32_t  absAdrOfRemLineInFlash;          // absoluteAdressOfRemovableLineInFlash
	uint16_t  reltAdrOfRemLine;                // relativeAdressOfRemovableLine ����� ��������� ������ ������������
	                                           // ������ ������� ( ���� ����� ��� ������������ ������ tmp[] )
	uint8_t  step;
	uint16_t lastStr;
	uint16_t i;

	if ( strlen( removableString ) + 1 != STRING_SIZE ) {		// ������ ����������� ������ �� ������
			return OPERATION_FAIL;   					        // ������� ������ LINE_SIZE �� ����� ����������
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
	step = ( 4 - ( STRING_SIZE % 4 ) + 1 );
#endif

	reltAdrOfRemLine = absAdrOfRemLineInFlash - strAdrOfSecThatContRemLine;                          // ������ ������ � �������

											                    // ���� ��� ������ ��������� � ������� ��� ������ ����������
	if ( ( reltAdrOfRemLine + 2 * ALIGN_STRING_SIZE ) > SPI_FLASH_SEC_SIZE \
			                                          || tmp[ reltAdrOfRemLine + 2 * ALIGN_STRING_SIZE - step ] !=  END_OF_STRING ) {

		for ( i = 0; i < ALIGN_STRING_SIZE; i++ ) {
			tmp[ reltAdrOfRemLine + i ] = 0xff;
		}

	} else {
																				        // ����� ��������� ������ + ALIGN_STRING_SIZE
	    for ( lastStr = reltAdrOfRemLine + ALIGN_STRING_SIZE - step; \
	                         lastStr + step <= SPI_FLASH_SEC_SIZE && tmp[ lastStr ] == END_OF_STRING; lastStr += ALIGN_STRING_SIZE ) {
	    }

	    lastStr = lastStr - ALIGN_STRING_SIZE + step;
	                                             //������ ������� ��������� ������ �� ��������� �������� �� ������ ����� ������ �����
	    for ( i = 0; i < lastStr - reltAdrOfRemLine - ALIGN_STRING_SIZE; i++ ) {
			    tmp[ reltAdrOfRemLine + i ] = tmp[ reltAdrOfRemLine + ALIGN_STRING_SIZE + i ];
	    }

	    for ( i = lastStr - 1; i >= ( lastStr - ALIGN_STRING_SIZE ); i-- ) {   // ��������� ������ ����������
	    		tmp[ i ] = 0xff;
	    }
	}
																		// ���������� ������� � ��������� �������
	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( strAdrOfSecThatContRemLine / SPI_FLASH_SEC_SIZE ) ) {
		return OPERATION_FAIL;
	}

	if ( SPI_FLASH_RESULT_OK != spi_flash_write( strAdrOfSecThatContRemLine, (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
		return OPERATION_FAIL;
	}

	return OPERATION_OK;
 }



/*
 *****************************************************************************************************************
 * result ICACHE_FLASH_ATTR requestString( uint8_t *string )
 *
 * ������� ����������:
 *  WRONG_LENGHT       - ��������� ������ ������� ������
 *  NOTHING_FOUND      - ������ � ������� ����� �� �������
 *  OPERATION_OK       - ������ �������
 *  OPERATION_FAIL
 *
 * @Description:
 *  ������� ��������:
 *  - ������ ������� ������ ������������� ������������
 *
 *  	�� ������� ������ ������ ���� ��������� ���� �� ���� ���� ( �� ����� ���� ��������� ). ������ ���� ����������
 *  �������� START_OF_FIELD, � ������������� �������� END_OF_FIELD. ������� ���������� ����� ������� �����������
 *  ���� ( ������� START_OF_FIELD ), ����������� �������� ������������ ������ ������. ����� ���������������
 *  ������������ ������� ����, � �� ���������  �������� ���� ������������ ������ ������ + ALIGN_STRING_SIZE
 *  ������������ ���� ������������� ������ � ������� �� ��� ��� ���� �� ���������� ��� ���������� ������, ��� ��
 *  �������� ����������. ���� ���������� �� ���� ������� �� ����������� ���� �� � ������������� ������ ��� ����
 *  ����, ���� ���� �� �������� �� �������� �����������, ���� ��� �� ������� ��������� ���� ������ � ����������
 *  NOTHING_FOUND
 *****************************************************************************************************************
 */
result ICACHE_FLASH_ATTR
requestString( uint8_t *string ) {

	uint8_t currentSector;
	uint16_t i = 0;
	uint16_t c;
	uint16_t relativeShift;


	if ( ( strlen( string ) + 1 ) != STRING_SIZE ) {        // ������ �������� ������ �� ������ ���� ������ ������������ ������

		return WRONG_LENGHT;
	}

	while ( i < STRING_SIZE ) {

		for ( ; i < STRING_SIZE; i++ ) {                   // � ������� ������ ������ ���� ���� �� ���� ����

			if ( START_OF_FIELD == string[ i ] ) {

				relativeShift = i++;
				break;
			}
			else if ( END_OF_STRING == string[ i ] ) {

				return NOTHING_FOUND;
			}
		}
		                                                              // ���� �� ����� �� ����� ���������� ������
		for ( currentSector = START_SECTOR; currentSector <= END_SECTOR; currentSector++ ) {

			if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {

					return OPERATION_FAIL;
			}

			for ( c = relativeShift; c < SPI_FLASH_SEC_SIZE; c += ALIGN_STRING_SIZE ) { // ���� �� ����� �� ����� �������

				if ( OPERATION_OK == stringEqual( &string[ relativeShift ], &tmp[ c ] ) ) {

					return OPERATION_OK;                             // ������� ����������
				}
			}
		}
	}

	return NOTHING_FOUND;        // ���� ����� �� ���� ������� ������ ���������� �� ���� �������
}



/*
 ****************************************************************************************************************
 * result ICACHE_FLASH_ATTR clearSectorsDB( void )
 *
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
	uint8_t step;

#if STRING_SIZE % 4 == 0
	step = 1;
#else
	step = ( 4 - ( STRING_SIZE % 4 ) + 1 );
#endif

	if ( ( strlen( string ) + 1 ) != STRING_SIZE ) {   // ������ �������� ������ �� ������ ���� ������ ������������ ������ LINE_SIZE

		return WRONG_LENGHT;
	}

	for ( ; currentSector <= END_SECTOR; currentSector++ ) {                // ���� �� ����� �� ����� ���������� ������

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {

			return OPERATION_FAIL;
		}

		if ( END_OF_STRING == tmp[ ALIGN_STRING_SIZE - step ] ) {                                  // ������ �� ������

			for ( i = 0; i + ALIGN_STRING_SIZE <= SPI_FLASH_SEC_SIZE; i += ALIGN_STRING_SIZE ) { // ���� �� ����� �� ����� �������

				if ( END_OF_STRING != tmp[ i + ALIGN_STRING_SIZE - step ] ) {                     // ������ ������

					break;
				}

				if ( 0 == strcmp( string, &tmp[i] ) ) {

				    return currentSector * SPI_FLASH_SEC_SIZE + i;                                // ������� ����������
				}
		    }
		}
	}

	return NOTHING_FOUND;
 }



/*
 ****************************************************************************************************************
 * ��������� �� ���� ������������ �����
 * ICACHE_FLASH_ATTR result stringEqual( uint8_t* firstString, uint8_t* secondString )
 *
 * ������� ����������:
 * OPERATION_FAIL  - ������ �� �����
 * OPERATION_OK    - ������ �����
 *
 * @Description:

 ***************************************************************************************************************
 */
ICACHE_FLASH_ATTR
result stringEqual( uint8_t* firstString, uint8_t* secondString ) {

	while( *firstString != '\r' && *firstString != '\n' && *firstString != '\0' ) {

		if ( *firstString++ != *secondString++ ) {

			return OPERATION_FAIL;
		}
	}

	if ( *firstString == *secondString ) {

		return OPERATION_OK;
	}
	else {

		return OPERATION_FAIL;
	}
}



/*
 ****************************************************************************************************************
 * result ICACHE_FLASH_ATTR query( uint8_t *storage, uint16_t *lenght, uint32_t absAdrInFlash )
 * storage       - ��������� ��� ���������� ������ 1��
 * lenght        - ���������� ���������� ����
 * absAdrInFlash - �� ����� ������� ����������� ���������� ������, ���� ��� ������ ������ �� 0
 *
 *
 * ������� ����������:
 * OPERATION_FAIL
 * OPERATION_OK     - ��� ��������� ���� ������
 * READ_DONE        - ������ ���������
 *
 * @Description:

 ***************************************************************************************************************
 */
result ICACHE_FLASH_ATTR
query( uint8_t *storage, uint16_t *lenght, uint32_t *absAdrInFlash ) {

	uint16_t i;
	uint16_t relAdrEndStr;
	uint8_t step;
	uint32_t strAdrOfSec;
	uint8_t currentSector = START_SECTOR;


#if STRING_SIZE % 4 == 0
	step = 1;
#else
	step = ( 4 - ( STRING_SIZE % 4 ) + 1 );
#endif


	if ( 0 == *absAdrInFlash ) {

		*absAdrInFlash = START_SECTOR * SPI_FLASH_SEC_SIZE;
	}


	for ( *lenght = 0; *absAdrInFlash <= ( END_SECTOR + 1 ) * SPI_FLASH_SEC_SIZE; ) {

		strAdrOfSec = ( *absAdrInFlash / SPI_FLASH_SEC_SIZE ) * SPI_FLASH_SEC_SIZE;

		os_printf( " absAdrInFlash   %d", *absAdrInFlash );
		os_delay_us(100000);

		os_printf( " strAdrOfSec   %d", strAdrOfSec );
		os_delay_us(100000);

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( strAdrOfSec, (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {

			return OPERATION_FAIL;
		}
		                                                                          // ������������� ������ ����� n + 1 ������
		for ( relAdrEndStr = 0; relAdrEndStr <= SPI_FLASH_SEC_SIZE; relAdrEndStr +=  ALIGN_STRING_SIZE ) {

			if ( END_OF_STRING != tmp[ relAdrEndStr + ALIGN_STRING_SIZE - step ] ) {

//				relAdrEndStr -= ALIGN_STRING_SIZE;
				break;
			}
		}

		os_printf( " relAdrEndStr   %d", relAdrEndStr );
		os_delay_us(100000);

		if ( 0 != relAdrEndStr && *absAdrInFlash <= ( strAdrOfSec + relAdrEndStr ) ) {

			os_printf( " check point ");
			os_delay_us(100000);

			i = *absAdrInFlash - strAdrOfSec;

			for ( ; *lenght < STORAGE_SIZE && i < relAdrEndStr; (*lenght)++, i++, (*absAdrInFlash)++ ) {

				storage[ *lenght ] = tmp[ i ];
			}

			if ( STORAGE_SIZE == *lenght ) {

				return OPERATION_OK;
			}
		}

		currentSector++;
		*absAdrInFlash =  currentSector * SPI_FLASH_SEC_SIZE;
	}

	return READ_DONE;
}

