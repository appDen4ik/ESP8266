/**
 ************************************************************************************************************************
  * @file    myDB.c
  * @author  Denys
  * @version V0.0.1
  * @date    7-Sept-2015
  * @brief
 ************************************************************************************************************************
 * @info:
 * 		Реализованы функции необходимые для работы с кучей
 * 		- найти запись по записи 		        - findString();
 * 		- найти запись по полю/полям			- requestString();
 * 		И для работы с базой данных:
 * 		- очистить выделенную область под хранение кучи		- clearSectorsDB();
 * 		- сделать запись                       				- insert();
 * 		- удалить запись                       				- delete();
 * 		- обновить запись                      				- update();
 * 		- запросить базу данных (подключиться) 				- query();
 *
 * @note:
 * 	-	Для коректной работы с флешь памятью данные хранить в ASCII (windows - 1251)
 *  -   Идеология работы с БД следующая:
 *  	-    Под хранение БД выделена неприрывная область spi flash памяти (количество секторов опредиляется разницой
 *  	     констант END_SECTOR - START_SECTOR + 1)
 *  	-    БД состоит из записей, длинна всех записей одинаковая (длинна записи опредиляется LINE_SIZE). При чем
 *  	     длинна записи не должна превышать размера сектора SPI_FLASH_SEC_SIZE. Сектора заполняются записями
 *  	     начиная с первого байта (не с 0-го, в этом байте храниться индикатор того что сектор пустой, либо в
 *  	     нем есть записи). Каждая запись заканчивается символом \0 либо цифрой 3 (спец символ ASCII).
 *  	-    Записи состоят из полей. Количество полей во всех записях одинаковое и они одинаковой длинны.
 *  	     Каждое поле заканчивается символом \n - переход на новую строку, а начинается с 2 START_OF_TEXT
 * 	-	В функциях spi_flash_write и spi_flash_read запись и чтение производится по 4 байта, если указать в этих
 * 	    функциях значение которе не кратно четырем то оно все равно округлится до ближайшего большего значения,
 * 	    которое кратно четырем, для коректной работы это нужно учитывать.
 * 	-	О том что сектор пустой свидетельствует маркер записанный в первмом байте. MARKER_ENABLE - если сектор пустой,
 * 	    и MARKER_DISABLE - если в секторе есть запись/записи. Если сектор пустой то запись делать начиная со второго
 * 	    байта. Если сектор не пустой 0-й байт = MARKER_DISABLE, то необходимо найти последнюю запись. Для этого
 * 	    последний байт каждой записи зарезервирован для маркера MARKER_ENABLE/MARKER_DISABLE: MARKER_ENABLE - запись
 * 	    последняя, MARKER_DISABLE - запись не последняя.
 ************************************************************************************************************************
 */


#include "user_interface.h"

#include "driver/myDB.h"
#include "c_types.h"
#include "spi_flash.h"
#include "osapi.h"
#include "espconn.h"


static ICACHE_FLASH_ATTR result stringEqual( uint8_t* firstString, uint8_t* secondString ); // сравнение не ноль терминальных строк


// константы необходимо переносить в ОЗУ, без этого не работает spi_flash_write,
// скорее всего это по причине того что что программа находиться на внешней флеше)


// когда делаю локальной переменной программа зависает на функции spi_flash_write
LOCAL uint8_t tmp[SPI_FLASH_SEC_SIZE];



/*
 ****************************************************************************************************************
 *  result ICACHE_FLASH_ATTR insert( uint8_t *line )
 *  Функция возвращает:
 *  WRONG_LENGHT       - длинна входной строки отличается от установленой LINE_SIZE
 *  OPERATION_FAIL     - не получилось прочитать/записать сектор
 *  LINE_ALREADY_EXIST - такия запись уже есть
 *  NOT_ENOUGH_MEMORY  - память заполнена
 *  OPERATION_OK
 *
 *  @Description:
 *  	Проверки:
 *    - длина записи должна быть равна LINE_SIZE (установленная длинна записи)
 *    - проверяем не находиться ли данная запись в куче (защита от повторения)
 *
 * 	    Поиск свободного места:
 * 	    - читается первый сектор выделенной памяти (START_SECTOR), ищется последняя запись в секторе. Для этого
 * 	      со сдвигом:
 * 	         1) ALIGN_STRING_SIZE * i - 1,  либо
 * 	         2) ALIGN_STRING_SIZE * i - ( 4 - ( STRING_SIZE % 4 ) + 1 ),                     (i = 1, 2...n)
 *		          1 - длинна записи кратна 4, 2 - длинна записи не кратна 4
 *        проверяется наличие нуль терминального символа, если его нет то проверяется влезет ли еще одна запись
 *        в этот сектор, если да - то производится запись в текущий сектор и функция заканчивает свою работу.
 *        Если же еще одна запись не влезает, то читается следующий сектор и повторяется вся процедура поиска
 *        с начала. Если дошли до конца последнего выделенного сектора то значит в кучу больше записей произвести
 *        нельзя - тоесть функция ничего не пишет и завершает свою работу.
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

	if ( strlen( string ) + 1 != STRING_SIZE ) {     // длинна входящей записи не должна быть больше установленой длинны LINE_SIZE
		return WRONG_LENGHT;				       // функция strlen возвращает длинну без учета \0
	}

	switch ( findString( string ) ) {                  // такая запись уже существует
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

		for ( i = 0; i + ALIGN_STRING_SIZE <= SPI_FLASH_SEC_SIZE ; i += ALIGN_STRING_SIZE ) { // ищем последнюю запись в текущем секторе

			if ( END_OF_STRING != tmp[ i + ALIGN_STRING_SIZE - step ] ) {                         // строка отсувствует

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
 * Функция возвращает:
 * WRONG_LENGHT			-	неверная длинна одной из входящих строк
 * NOTHING_FOUND		- 	oldString записи не существует
 * OPERATION_FAIL		-	ошибка одной из функций работы с флешь памятью
 * LINE_ALREADY_EXIST	-   newString запись уже существует
 * OPERATION_OK
 *
 * @Description:
 *	Проверки:
 *	- Проверка входящих строк на равенство по длинне
 *	- Проверка что newString равна установленной длинне записи
 *	- Проверка что oldString существует в бд
 *	- Проверка что newString не существует в бд
 *
 *
 *		Вычитывается полностью сектор в котором находится запись в буффер. В буффере oldString заменяется на
 *	newString. Сектор в котором находиться обновляемая запись затирается. Данные из буффера записиваются в этот
 *  сектор.
 ****************************************************************************************************************
 */
result ICACHE_FLASH_ATTR
update( uint8_t *oldString, uint8_t *newString ) {

	uint32_t  adressRequestingString;
	uint32_t  startAdressSector;							// начало сектора в котором находится запрашиваемая запись
	uint8_t   stringLenght;

	if ( ( stringLenght = strlen( newString ) ) != strlen( oldString ) ) {
		return WRONG_LENGHT;   								 	           // записи имеют одинаковую длинну
	}
	if ( ( stringLenght + 1 ) != STRING_SIZE ) {                           //strlen возвращает длинну без учета \0
		return WRONG_LENGHT;   							 		           // длинна обновляемой записи не больше заданой
	}

	switch ( adressRequestingString = findString( oldString ) ) {          // если oldString отсувствует ничего обновлять не нужно
		case NOTHING_FOUND:
			return NOTHING_FOUND;
		case WRONG_LENGHT:
			return WRONG_LENGHT;
		case OPERATION_FAIL:
			return OPERATION_FAIL;
		default:
			break;
	}

	switch ( findString( newString ) ) {        // если newString уже есть ничего обновлять не нужно
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

	if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( startAdressSector /  SPI_FLASH_SEC_SIZE ) ) {  // обновление сектора
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
 * Функция возвращает:
 * WRONG_LENGHT			-	неверная длинна входной строки
 * NOTHING_FOUND		- 	такой записи не существует
 * OPERATION_FAIL		-	ошибка одной из функций работы с флешь памятью
 * OPERATION_OK
 *
 * @Description:
 *	Проверки:
 *	- длинна входной строки равна установленной
 *	- входная строка есть в куче
 *
 * 		По адрессу которой возвращает функция findString опредиляется сектор в котором находится удаляемая запись.
 * Вычитывается сектор в буффур. Если запись последняя, то она просто затирается в буффере (заполняется 0хff).
 * Если же запись не является последней тогда все записи, которые находяться справа от удаляемой сдвигаются на
 * длинну одной выровнянной строки влево. Самая крайняя запись справа затирается, так как последняя запись в ре -
 * зультате сдвига последняя запсь в секторе дублируется.
 ****************************************************************************************************************
 */
result ICACHE_FLASH_ATTR
delete( uint8_t *removableString ) {

	uint32_t  strAdrOfSecThatContRemLine;      // startAdressOfSectorThatContainsRemovableLine
	uint32_t  absAdrOfRemLineInFlash;          // absoluteAdressOfRemovableLineInFlash
	uint16_t  reltAdrOfRemLine;                // relativeAdressOfRemovableLine адрес удаляемой записи относительно
	                                           // начала сектора ( тоже самое что относительно начала tmp[] )
	uint8_t  step;
	uint16_t lastStr;
	uint16_t i;

	if ( strlen( removableString ) + 1 != STRING_SIZE ) {		// длинна обновляемой записи не больше
			return OPERATION_FAIL;   					        // заданой длинны LINE_SIZE во время компиляции
	}

	switch ( absAdrOfRemLineInFlash = findString( removableString ) ) {  // если oldString отсувствует ничего обновлять не нужно
		case NOTHING_FOUND:
			return NOTHING_FOUND;
		case WRONG_LENGHT:
			return WRONG_LENGHT;
		case OPERATION_FAIL:
			return OPERATION_FAIL;
		default:
			break;
	}
          	                                                              // начальный адресс сектора в котором находится запись
	strAdrOfSecThatContRemLine = ( absAdrOfRemLineInFlash / SPI_FLASH_SEC_SIZE ) * SPI_FLASH_SEC_SIZE;

	if ( SPI_FLASH_RESULT_OK != spi_flash_read( strAdrOfSecThatContRemLine, (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
				return OPERATION_FAIL;
	}

#if STRING_SIZE % 4 == 0
	step = 1;
#else
	step = ( 4 - ( STRING_SIZE % 4 ) + 1 );
#endif

	reltAdrOfRemLine = absAdrOfRemLineInFlash - strAdrOfSecThatContRemLine;                          // начало записи в буффере

											                    // если эта запись последняя в секторе она просто затирается
	if ( ( reltAdrOfRemLine + 2 * ALIGN_STRING_SIZE ) > SPI_FLASH_SEC_SIZE \
			                                          || tmp[ reltAdrOfRemLine + 2 * ALIGN_STRING_SIZE - step ] !=  END_OF_STRING ) {

		for ( i = 0; i < ALIGN_STRING_SIZE; i++ ) {
			tmp[ reltAdrOfRemLine + i ] = 0xff;
		}

	} else {
																				        // конец последней записи + ALIGN_STRING_SIZE
	    for ( lastStr = reltAdrOfRemLine + ALIGN_STRING_SIZE - step; \
	                         lastStr + step <= SPI_FLASH_SEC_SIZE && tmp[ lastStr ] == END_OF_STRING; lastStr += ALIGN_STRING_SIZE ) {
	    }

	    lastStr = lastStr - ALIGN_STRING_SIZE + step;
	                                             //записи которые находятся справа от удаляемой сместить на длинну одной строки влево
	    for ( i = 0; i < lastStr - reltAdrOfRemLine - ALIGN_STRING_SIZE; i++ ) {
			    tmp[ reltAdrOfRemLine + i ] = tmp[ reltAdrOfRemLine + ALIGN_STRING_SIZE + i ];
	    }

	    for ( i = lastStr - 1; i >= ( lastStr - ALIGN_STRING_SIZE ); i-- ) {   // последняя запись затирается
	    		tmp[ i ] = 0xff;
	    }
	}
																		// перезапись сектора с удаляемой записью
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
 * Функция возвращает:
 *  WRONG_LENGHT       - нвеверная длинна входной строки
 *  NOTHING_FOUND      - строка с искомым полем не найдено
 *  OPERATION_OK       - строка найдена
 *  OPERATION_FAIL
 *
 * @Description:
 *  Входные проверки:
 *  - длинна входной строки соответствует установленой
 *
 *  	Во входной строке должно быть заполнено хотя бы одно поле ( их может быть несколько ). Каждое поле начинается
 *  символом START_OF_FIELD, а заканчивается символом END_OF_FIELD. Сначала происходит поиск первого попавшегося
 *  поля ( символа START_OF_FIELD ), сохраняется смещение относительно начала строки. Потом последовательно
 *  вычитываются сектора кучи, и со смещением  смещение поля относительно начала строки + ALIGN_STRING_SIZE
 *  сравниваются поля запрашиваемой строки и текущей до тех пор пока не проверится вся выделенная память, или не
 *  найдется совпадение. Если совпадений не было найдено то проверяется есть ли в запрашиваемой строке еще одно
 *  поле, если есть то операции по проверке повторяются, если нет то функция завершает свою работу и возвращает
 *  NOTHING_FOUND
 *****************************************************************************************************************
 */
result ICACHE_FLASH_ATTR
requestString( uint8_t *string ) {

	uint8_t currentSector;
	uint16_t i = 0;
	uint16_t c;
	uint16_t relativeShift;


	if ( ( strlen( string ) + 1 ) != STRING_SIZE ) {        // длинна входящей строки не должна быть больше установленой длинны

		return WRONG_LENGHT;
	}

	while ( i < STRING_SIZE ) {

		for ( ; i < STRING_SIZE; i++ ) {                   // в искомой строке должно быть хотя бы одно поле

			if ( START_OF_FIELD == string[ i ] ) {

				relativeShift = i++;
				break;
			}
			else if ( END_OF_STRING == string[ i ] ) {

				return NOTHING_FOUND;
			}
		}
		                                                              // пока не дошли до конца выделенной памяти
		for ( currentSector = START_SECTOR; currentSector <= END_SECTOR; currentSector++ ) {

			if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {

					return OPERATION_FAIL;
			}

			for ( c = relativeShift; c < SPI_FLASH_SEC_SIZE; c += ALIGN_STRING_SIZE ) { // пока не дошли до конца сектора

				if ( OPERATION_OK == stringEqual( &string[ relativeShift ], &tmp[ c ] ) ) {

					return OPERATION_OK;                             // найдено совпадение
				}
			}
		}
	}

	return NOTHING_FOUND;        // если дошли до этой строчки значит совпадений не было найдено
}



/*
 ****************************************************************************************************************
 * result ICACHE_FLASH_ATTR clearSectorsDB( void )
 *
 * Функция возвращает:
 * OPERATION_FAIL  - не получилось очистить сектор
 * OPERATION_OK    - память готова к использованию
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
 * Функция возвращает:
 * WRONG_LENGHT    - длинна входной строки не совпала с установленой
 * OPERATION_FAIL  - не получилось прочитать сектор
 * NOTHING_FOUNDED - ничего не найдено
 * либо абсолютный адресс записи, если найдено совпадение
 *
 * @Description:
 * 	Читаем начальный сектор START_SECTOR в ОЗУ, проверяем что сектор не пустой
 * 	Если пустой, то читаем следующий. Если же сектор не пустой то последовательно сравниваем записи до тех пор:
 * 		- пока не будет найдена искомая строка ( strcmp вернет 0 )
 * 		- либо пока функция не дойдет до конца сектора. В таком случае читается след сектор и повторяется
 * 		  процедура сначала и так далее, либо пока не найдется запрашиваемая строка, либо функция не проверит
 * 		  всю выделенную память
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

	if ( ( strlen( string ) + 1 ) != STRING_SIZE ) {   // длинна входящей строки не должна быть больше установленой длинны LINE_SIZE

		return WRONG_LENGHT;
	}

	for ( ; currentSector <= END_SECTOR; currentSector++ ) {                // пока не дошли до конца выделенной памяти

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {

			return OPERATION_FAIL;
		}

		if ( END_OF_STRING == tmp[ ALIGN_STRING_SIZE - step ] ) {                                  // сектор не пустой

			for ( i = 0; i + ALIGN_STRING_SIZE <= SPI_FLASH_SEC_SIZE; i += ALIGN_STRING_SIZE ) { // пока не дошли до конца сектора

				if ( END_OF_STRING != tmp[ i + ALIGN_STRING_SIZE - step ] ) {                     // сектор пустой

					break;
				}

				if ( 0 == strcmp( string, &tmp[i] ) ) {

				    return currentSector * SPI_FLASH_SEC_SIZE + i;                                // найдено совпадение
				}
		    }
		}
	}

	return NOTHING_FOUND;
 }



/*
 ****************************************************************************************************************
 * Сравнение не ноль терминальных строк
 * ICACHE_FLASH_ATTR result stringEqual( uint8_t* firstString, uint8_t* secondString )
 *
 * Функция возвращает:
 * OPERATION_FAIL  - строки не равны
 * OPERATION_OK    - строки равны
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
 * storage       - хранилище для вычитанных данных 1КБ
 * lenght        - количество вычитанных байт
 * absAdrInFlash - на каком адрессе закончилось предыдущее чтение, если это первый запрос то 0
 *
 *
 * Функция возвращает:
 * OPERATION_FAIL
 * OPERATION_OK     - это последний блок данных
 * READ_DONE        - чтение закончено
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
		                                                                          // относительный адресс конца n + 1 записи
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

