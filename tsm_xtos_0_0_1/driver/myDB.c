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


ICACHE_FLASH_ATTR static void maskBitsInField ( uint8_t* adressOfField, uint16_t startAbsovuleAdress, uint16_t endAbsovuleAdress );


// подгружаем необходимые константы в ОЗУ (пока не совсем понятно причина по которой
// константы необходимо переносить в ОЗУ, но без этого не работает spi_flash_write,
// скорее всего это по причине того что что программа находиться на внешней флеше)
static uint8_t markerEnable[4] = { MARKER_ENABLE, 0xff, 0xff, 0xff };
static uint8_t markerDisable[4] = { MARKER_DISABLE, 0xff, 0xff, 0xff };

// когда делаю локальной переменной программа зависает на функции spi_flash_write
static uint8_t tmp[SPI_FLASH_SEC_SIZE];


/*
 *  writeres ICACHE_FLASH_ATTR insert( uint8_t *line )
 *  @Description:
 *    - длинна записи должна быть меньше величины сектора, а также
 *      длина записи должна быть равна LINE_SIZE (установленная длинна записи)
 *    - проверяем не находиться ли данная запись в куче (защита от повторения)
 * 	  - поиск свободного места - если нет то ничего не пишем и выходим из функции
 *
 * 	    Подробное описание механизма поиска свободного места:
 * 	    1) читается первый сектор кучи (START_SECTOR), проверяем пустой ли сектор ( 0 - байт
 * 	       кучи - если равен MARKER_ENABLE то значит сектор пустой, если равен MARKER_DISABLE
 * 	       то в секторе имеются записи)
 * 	    2) если 0 - й байт кучи равен MARKER_ENABLE, то в 0 - й  байт пишем MARKER_DISABLE и
 * 	       делается первая запись в этом секторе (начиная с первого адресса), а последний байт
 * 	       записи устанавливается MARKER_ENABLE (что свидетильствует о том что эта запись
 * 	       последняя)
 * 	    3) если 0 - й байт кучи равен MARKER_DISABLE (сектор не пустой), то производится поиск
 * 	       последней записи в этом секторе, далее проверяется проверяется влезает ли туда еще
 * 	       одна запись:
 * 	       	- если влезает то маркер последней записи делаем MARKER_DISABLE вносим новую запись и
 * 	       	  последний байт новой записи делаем MARKER_ENABLE, на этом функция заканчивает свою работу
 * 	       	- если не влезает, то тогда читаем след сектор и повтоярем действия начиная с первого пункта
 *
*/
result ICACHE_FLASH_ATTR
insert( uint8_t *line ) {

	uint8_t currentSector = START_SECTOR;
	uint16_t len = strlen(line) + 1;              // функция возвращает длинну без учета \0
	uint8_t alignLine[ALIGN_LINE_SIZE];
	uint16_t i;


	if ( len != LINE_SIZE ) {                   // длинна входящей записи не должна быть больше установленой длинны LINE_SIZE
		return WRONG_LENGHT;
	}

	if ( NOTHING_FOUNDED != findLine( line ) ) {
		return LINE_ALREADY_EXIST;                       // такая запись уже есть
	}

	for ( i = 0; i < ALIGN_LINE_SIZE; i++){
		alignLine[i] = 0xff;
	}

	line[LINE_SIZE - 1] = MARKER_ENABLE;        // данная строка являеться последней ( записываю соответствующий маркер )

	memcpy( alignLine, line, LINE_SIZE );


	for ( ; currentSector <= END_SECTOR; currentSector++ ) {

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ){
			return OPERATION_FAIL;
		}

		if ( MARKER_ENABLE == tmp[0] ) {                                                               // сектор пустой

			spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&markerDisable,  1);          // затираем маркер

			spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + 1, (uint32 *)alignLine, ALIGN_LINE_SIZE );

			return OPERATION_OK;

		} else if ( MARKER_DISABLE == tmp[0] ) {                                  // сектор не пустой, ищем последнюю запись

			i = LINE_SIZE;                                        // counter = LINE_SIZE - маркер последний бит первой записи

			for ( i = LINE_SIZE; i < SPI_FLASH_SEC_SIZE; i += LINE_SIZE ){

				if ( MARKER_ENABLE == tmp[i] ) {                                 // если эта запись последняя

					spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + i, (uint32 *)&markerDisable,  1);   // затираем маркер

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
 * Сначала поиск записи во flash памяти, если запись не найдено то ничего не делаем, и выходим из функции;
 * если же запись найдено то нужно определить сектор в котором находиться запись, вычитать сектор в буффер,
 * далее необходимо определить адресс записи в буффере, переписать эту запись, очистить текущий сектор, а
 * заполнить его снова данными из буффера
 */
result ICACHE_FLASH_ATTR
update( uint8_t *oldLine, uint8_t *newLine ) {
	uint32_t adress;
	uint8_t sectorNumber;
	uint8_t *lineAdressInTmp;
	uint8_t lineLemght;

	if ( (lineLemght = strlen(newLine)) != strlen(oldLine) ){
		return OPERATION_FAIL;   								 		// проверка что длинны записей равны
	}
	if ( lineLemght != LINE_SIZE ){
			return OPERATION_FAIL;   							 		// проверка что длинна обновляемой записи не больше
																 	 	 	// заданой длинны записи во время компиляции
		}

	if ( OPERATION_FAIL == ( adress = findLine(oldLine) ) ){
		return OPERATION_FAIL;												// если запись не найдено, то завершаем работу
	} else {

		sectorNumber = (adress / SPI_FLASH_SEC_SIZE) * SPI_FLASH_SEC_SIZE ;							// определяем в каком секторе находится запись

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( sectorNumber , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
			return OPERATION_FAIL;
		}

		lineAdressInTmp = &tmp[ adress - ( SPI_FLASH_SEC_SIZE * sectorNumber )];		// определяем адресс записи в буфферe

		memcpy( lineAdressInTmp, newLine, strlen(newLine) );				// обновляем запись (все кроме последнего байта)

																			// очищаем сектор flash где находиться запрашива -
																			// емая запись
		if ( SPI_FLASH_RESULT_OK != spi_flash_erase_sector( sectorNumber )  ) {
					return OPERATION_FAIL;
				}
																				// записываем буффер в flash память
		spi_flash_write( sectorNumber, (uint32 *)&tmp,  SPI_FLASH_SEC_SIZE);

		return OPERATION_OK;
	}
 }


/*
 * operationres ICACHE_FLASH_ATTR delete( uint8_t *line )
 * Сначала поиск записи во flash памяти, если запись не найдено то ничего не делаем, и выходим из функции;
 * если же запись найдено то нужно определить сектор в котором находиться запись, вычитать сектор в буффер,
 * найти начало следующей записи после запрашиваемой в буфере, сместить данные начиная с адресса который
 * определили на одну запись влево, затереть сектор, записать данные с буффера в сектор flash
 */
result ICACHE_FLASH_ATTR
delete( uint8_t *removableLine ) {
	uint8_t   strAdrOfSecThatContRemLine;     // startAdressOfSectorThatContainsRemovableLine
	uint32_t  absAdrOfRemLineInFlash;         // absoluteAdressOfRemovableLineInFlash
	uint16_t  reltAdrOfRemLine;               // relativeAdressOfRemovableLine адрес удаляемой записи относительно
	                                          // начала сектора ( тоже самое что относительно начала tmp[] )
	uint16_t i;

	if ( strlen( removableLine ) != LINE_SIZE ){		// длинна обновляемой записи не больше
			return OPERATION_FAIL;   					// заданой длинны LINE_SIZE во время компиляции
	}

	if ( OPERATION_FAIL == ( absAdrOfRemLineInFlash = findLine( removableLine ) ) ){
		return OPERATION_FAIL;							// если запись не найдено, то работа функции на этом завершается
	} else {
		                                                // начальный адресс сектора в котором находится запись
		strAdrOfSecThatContRemLine = ( absAdrOfRemLineInFlash / SPI_FLASH_SEC_SIZE ) * SPI_FLASH_SEC_SIZE;

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( strAdrOfSecThatContRemLine, (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ) {
				return OPERATION_FAIL;
		}

		reltAdrOfRemLine = absAdrOfRemLineInFlash - strAdrOfSecThatContRemLine;

		                                                           // не является ли удаляемая запись последней
		if ( MARKER_ENABLE == tmp[ reltAdrOfRemLine + LINE_SIZE - 1 ] ) {

			tmp[ reltAdrOfRemLine - 1 ] = MARKER_DISABLE;          // предыдущая запись стает последней

			i = 1;

		} else {
			                                                   // необходимо все записи которые находятся справа
			                                                   // от удаляемой сместить на длинну одной строки влево
			for ( i = 0; tmp[ reltAdrOfRemLine + LINE_SIZE + i ] != MARKER_ENABLE; i++ ){
				tmp[ reltAdrOfRemLine + i ] = tmp[ reltAdrOfRemLine + LINE_SIZE + i ];
			}
			tmp[ reltAdrOfRemLine + LINE_SIZE + i ] = MARKER_ENABLE;

			i++;

		}
		                                               	   // необходимо заполнить все биты справа от последней записи 0xFF
		for ( ; ( reltAdrOfRemLine + i ) < LINE_SIZE; i++ ){
			tmp[ reltAdrOfRemLine + i ] = 0xFF;
		}

													   	   	   	   	   	   // перезапись сектора с удаляемой записью
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
result ICACHE_FLASH_ATTR
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



/*
 ****************************************************************************************************************
 * result ICACHE_FLASH_ATTR clearSectorsDB( void )
 * Функция возвращает:
 * OPERATION_FAIL  - не получилось очистить сектор, либо поставить метку
 * OPERATION_OK    - память готова к использованию
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
		                                                                                      // ставим метку что сектор пустой
	    if ( SPI_FLASH_RESULT_OK != spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&markerEnable, 1 ) ) {
	    	return OPERATION_FAIL;
	    }
	}

	return OPERATION_OK;
 }




/*
 ****************************************************************************************************************
 * uint32_t ICACHE_FLASH_ATTR findLine( uint8_t *line )
 * Функция возвращает:
 * WRONG_LENGHT    - длинна входной строки не совпала с установленой
 * OPERATION_FAIL  - не получилось прочитать сектор
 * NOTHING_FOUNDED - ничего не найдено
 * либо адресс записи если найдено совпадение
 *
 * @Description:
 * 	Читаем начальный сектор START_SECTOR в ОЗУ, проверяем что сектор не пустой
 * 	Если пустой, то читаем следующий. Если же сектор не пустой то последовательно сравниваем записи до тех пор:
 * 		- пока не будет найдена искомая строка ( strcmp вернет 0 )
 * 		- либо пока не проверится вся выдиленная память
 * 	Если strcmp вернет значение != 0, это может свидетильствовать о том что эта запись последняя в этом секторе
 * (она закнчивается на 0х03 а не на 0х00). В таком случае последнюю запись необходимо проверить другим образом:
 *      - просто в ОЗУ вместо 0х03 записать 0х00, если не строки не совпали то читаем след. сектор и так далее,
 *        пока не проверим все выделиненные сектора включая END_SECTOR, или не найдем запись
 ***************************************************************************************************************
 */
uint32_t ICACHE_FLASH_ATTR
findLine( uint8_t *line ) {

	uint8_t currentSector = START_SECTOR;
	uint16_t i;

	if ( ( strlen( line ) + 1 ) != LINE_SIZE ) { // длинна входящей строки не должна быть больше установленой длинны LINE_SIZE
		return WRONG_LENGHT;
	}

	for ( ; currentSector <= END_SECTOR;  currentSector++) {                // пока не дошли до конца выделенной памяти

		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ){
				return OPERATION_FAIL;
		}

		if ( MARKER_DISABLE == tmp[0] ) {                                                  // сектор не пустой
		    for ( i = 1; i < SPI_FLASH_SEC_SIZE; i += LINE_SIZE ) {                       // пока не дошли до конца сектора
			    if ( 0 == strcmp( line, &tmp[i] ) ) {
			    	return currentSector*SPI_FLASH_SEC_SIZE + i;                            // найдено совпадение
			    }
			    else if ( MARKER_ENABLE == tmp[i + LINE_SIZE - 1] ) {                       // запись последняя в секторе
				    tmp[i + LINE_SIZE - 1] = MARKER_DISABLE;
				    if ( 0 == strcmp( line, &tmp[i] ) ) {
				    	return currentSector*SPI_FLASH_SEC_SIZE + i;                        // найдено совпадение
				    } else {
				    	break;
				    }
			    }
		    }
		}
	}

	return NOTHING_FOUNDED;                                 // если дошло до этой строчки значит совпадений не было найдено
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




