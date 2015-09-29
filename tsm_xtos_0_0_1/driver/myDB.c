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
 * 		- найти запись по записи 	(static)	- findLine();
 * 		- найти запись по полю/полям			- requestLine();
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


LOCAL ICACHE_FLASH_ATTR void maskBitsInField ( uint8_t* adressOfField, uint16_t startAbsovuleAdress, uint16_t endAbsovuleAdress );
LOCAL ICACHE_FLASH_ATTR uint8_t CompareStrings( uint8_t* firstString, uint8_t* secondString );


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

#if STRING_SIZE % 4 == 0
			if ( END_OF_SRING != tmp[i + ALIGN_STRING_SIZE - 1] ) {                         // строка отсувствует

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

	switch ( adressRequestingString = findString( oldString ) ) {       // если oldString отсувствует ничего обновлять не нужно
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
																					                      // обновление сектора
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
 * Сначала поиск записи во flash памяти, если запись не найдено то ничего не делаем, и выходим из функции;
 * если же запись найдено то нужно определить сектор в котором находиться запись, вычитать сектор в буффер,
 * найти начало следующей записи после запрашиваемой в буфере, сместить данные начиная с адресса который
 * определили на одну запись влево, затереть сектор, записать данные с буффера в сектор flash
 */
result ICACHE_FLASH_ATTR
delete( uint8_t *removableString ) {

	uint32_t  strAdrOfSecThatContRemLine;      // startAdressOfSectorThatContainsRemovableLine
	uint32_t  absAdrOfRemLineInFlash;          // absoluteAdressOfRemovableLineInFlash
	uint16_t  reltAdrOfRemLine;                // relativeAdressOfRemovableLine адрес удаляемой записи относительно
	                                           // начала сектора ( тоже самое что относительно начала tmp[] )
	uint8_t step;
	uint16_t lastStr;
	uint16_t i;

	if ( strlen( removableString ) + 1 != STRING_SIZE ) {		// длинна обновляемой записи не больше
			return OPERATION_FAIL;   					// заданой длинны LINE_SIZE во время компиляции
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
	step = 4 - ( STRING_SIZE % 4 ) + 1;
#endif

	reltAdrOfRemLine = absAdrOfRemLineInFlash - strAdrOfSecThatContRemLine;                          // начало записи в буффере

											                    // если эта запись последняя в секторе ее нужно просто затереть
	if ( ( reltAdrOfRemLine + 2 * ALIGN_STRING_SIZE ) > SPI_FLASH_SEC_SIZE \
			                           || tmp[ reltAdrOfRemLine + 2 * ALIGN_STRING_SIZE - step ] !=  END_OF_SRING ) {

		for ( i = 0; i < ALIGN_STRING_SIZE; i++ ) {
			tmp[ reltAdrOfRemLine + i ] = 0xff;
		}

	} else {
																								      // конец последней записи
	    for ( lastStr = reltAdrOfRemLine + ALIGN_STRING_SIZE - step; lastStr == END_OF_SRING; lastStr += ALIGN_STRING_SIZE ) {

	    }

	    lastStr -= ALIGN_STRING_SIZE;
			                               //записи которые находятся справа от удаляемой сместить на длинну одной строки влево
	    for ( i = 0; i <= lastStr; i++ ) {
			    tmp[ reltAdrOfRemLine + i ] = tmp[ reltAdrOfRemLine + ALIGN_STRING_SIZE + i ];
	   }

	}
																		// перезапись сектора с удаляемой записью
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
 * - на вход приходит строка в которой заполнены часть полей и необходимо найти в куче запись в
 *   которой хотя бы одно поле будет такое же как в искомой записи. В пустые поля искомой строки
 *   должны быть записаны \r
 *@Description:
 *    - Проверки
 *        - длинна запрашиваемой строки должна быть равна заданной (LINE_SIZE)
 *   	  - в запрашиваемой строке должно быть заполнено хотя бы одно поле
 *    - Если во входящей строке заполнено больше одного поля то необходимо
 *      сначала маскировать все кроме первого, далее искать во флеше по всем
 *      записям совпадение, если его не найдено то тогда маскируются все поля
 *      кроме второго и происходит поиск во флеше, и так далее, до тех пор
 *      пока не будет найдено запись с таким же полем, либо ее не будет найдено
 *      вообще.
 *
 */
/*result ICACHE_FLASH_ATTR
requestLine( uint8_t *line) {

	uint8_t currentSector = START_SECTOR;
	uint16_t i, c;
	uint8_t endAdressOfCarrentField, startAdressOfCarrentField;
	uint8_t copyLine[LINE_SIZE];

	                                      // длинна входящей строки не должна быть больше установленой длинны LINE_SIZE
	if ( ( strlen( line ) + 1 ) != LINE_SIZE ) { return OPERATION_FAIL; }

	                                // в искомой строке должно быть заполнено хотя бы одно поле
	for ( i = 0; i <= LINE_SIZE; i++ ){
		if ( '\n' != tmp[i] || MARKER_ENABLE != tmp[i] ) {		                                // ищется окончание первого попавшего поля
			break;
		} else if ( LINE_SIZE == i ){
			return OPERATION_FAIL;
		}
	}
//***********************************************************************************************************************************************



				// подготовка запрашеваемой строки (маскировка необходимых полей)
	for ( i = 0; i < LINE_SIZE; ) {

				// копируем запрашиваемую строку
		memcpy( copyLine, line, LINE_SIZE );

				// находим начало следующей строки  START_OF_TEXT
		for ( i = 0; ; i++) {
			if ( START_OF_TEXT == copyLine[i] ) {
				startAdressOfCarrentField = i;
				break;
			} else if ( '\0' == copyLine[i] || MARKER_ENABLE != copyLine[i] || LINE_SIZE == i) {
				return OPERATION_FAIL;
			}
		}

			    // находим конец следующей строки \n
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


		for ( ; currentSector <= END_SECTOR;  currentSector++) {                  						// пока не дошли до конца выделенной памяти

																												//чтение сектора
			if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ){
					return OPERATION_FAIL;
			}

			if ( MARKER_DISABLE == tmp[0] ) {   // сектор не пустой
				for ( c = 1; c < SPI_FLASH_SEC_SIZE; c += LINE_SIZE ) { // пока не дошли до конца сектора

					maskBitsInField ( &tmp[c], startAdressOfCarrentField, endAdressOfCarrentField );

					if ( 0 == strcmp( copyLine, &tmp[c] ) ) { return currentSector*SPI_FLASH_SEC_SIZE + c;      // найдено совпадение
					}
					else if ( MARKER_ENABLE == tmp[c] ) {
						tmp[c - 1] = MARKER_DISABLE;
						if ( 0 == strcmp( copyLine, &tmp[c] ) ) { return currentSector*SPI_FLASH_SEC_SIZE + c;  // найдено совпадение
						} else { break; }
					}

				}
			}
		}

	}

	return OPERATION_FAIL;        // если дошли до этой строчки значит совпадений не было найдено
}

*\

/*
 ****************************************************************************************************************
 * result ICACHE_FLASH_ATTR clearSectorsDB( void )
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

	if ( ( strlen( string ) + 1 ) != STRING_SIZE ) {     // длинна входящей строки не должна быть больше установленой длинны LINE_SIZE
		return WRONG_LENGHT;
	}

	for ( ; currentSector <= END_SECTOR; currentSector++ ) {                // пока не дошли до конца выделенной памяти

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE * currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
			return OPERATION_FAIL;
		}

#if STRING_SIZE % 4 == 0
			if ( END_OF_SRING == tmp[ ALIGN_STRING_SIZE - 1 ] ) {                         // сектор не пустой
#else
			if ( END_OF_SRING == tmp[ ALIGN_STRING_SIZE - ( 4 - ( STRING_SIZE % 4 ) + 1 ) ] ) {
#endif
				for ( i = 0; i + ALIGN_STRING_SIZE <= SPI_FLASH_SEC_SIZE; i += ALIGN_STRING_SIZE ) {   // пока не дошли до конца сектора

#if STRING_SIZE % 4 == 0
					if ( END_OF_SRING != tmp[ i + ALIGN_STRING_SIZE - 1 ] ) {                              // сектор пустой
						break;
					}
#else
					if ( END_OF_SRING != tmp[ i + ALIGN_STRING_SIZE - ( 4 - ( STRING_SIZE % 4 ) + 1 ) ] ) {
						break;
					}
#endif

					if ( 0 == strcmp( string, &tmp[i] ) ) {
						return currentSector * SPI_FLASH_SEC_SIZE + i;                                 // найдено совпадение
					}

		    }

		}

	}
	return NOTHING_FOUND;                                 // если дошло до этой строчки значит совпадений не было найдено
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

// Сравнение строк (строки должны заканчиваться на \r либо \n либо \0)
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


