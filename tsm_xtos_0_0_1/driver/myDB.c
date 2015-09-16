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
 *  	     Каждая запись заканчивается символом \n - переход на новую строку
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

#include "driver/myDB.h"
#include "c_types.h"
#include "spi_flash.h"
#include "osapi.h"



// подгружаем необходимые константы в ОЗУ (пока не совсем понятно причина по которой
// константы необходимо переносить в ОЗУ, но без этого не работает spi_flash_write,
// скорее всего это по причине того что что программа находиться на внешней флеше)
static uint8_t markerEnable[4] = { MARKER_ENABLE, 0xff, 0xff, 0xff };
static uint8_t markerDisable[4] = { MARKER_DISABLE, 0xff, 0xff, 0xff };

// когда делаю локальной переменной программа зависает на функции spi_flash_write
static uint8_t tmp[SPI_FLASH_SEC_SIZE];


/*
 *  writeres ICACHE_FLASH_ATTR writeLine( uint8_t *line )
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
writeres ICACHE_FLASH_ATTR
insert( uint8_t *line ) {

	uint8_t currentSector = START_SECTOR;
	uint16_t len = strlen(line) + 1; // функция возвращает длинну без учета \0
	uint8_t alignLine[ALIGN_LINE_SIZE];

	{ //
		uint16_t i;
		for ( i = 0; i < sizeof(alignLine); i++){
			alignLine[i] = 0xff;
		}
	}

	 // длинна записи не больше величины сектора  SPI_FLASH_SEC_SIZE
	 // длинна входящей записи не должна быть больше установленой длинны LINE_SIZE
	if ( len > SPI_FLASH_SEC_SIZE || len != LINE_SIZE ) { return WRITE_FAIL; }

	// данная строка являеться последней ( записываю соответствующий маркер )
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
		if ( MARKER_ENABLE == tmp[0] ) {// сектор пустой
				// затираем маркер
			spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&markerDisable,  1);
				// делаем запись
			memcpy( alignLine, line, len ); // маскируем старшие биты, если необходимо
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
		} else if ( MARKER_DISABLE == tmp[0] ) {// сектор не пустой, ищем последнюю запись
			uint16_t counter;	// counter = LINE_SIZE - маркер последний бит первой записи
#ifdef  DB_DEBUG
	ets_uart_printf("line not first");
#endif
			for ( counter = LINE_SIZE; counter < SPI_FLASH_SEC_SIZE; counter += LINE_SIZE ){
				if ( MARKER_ENABLE == tmp[counter] ) { // если эта запись последняя
					 // затираем маркер
					spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE + counter, (uint32 *)&markerDisable,  1);
						// делаем запись
					memcpy( alignLine, line, len ); // маскируем старшие биты, если необходимо
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
 * Сначала поиск записи во flash памяти, если запись не найдено то ничего не делаем, и выходим из функции;
 * если же запись найдено то нужно определить сектор в котором находиться запись, вычитать сектор в буффер,
 * далее необходимо определить адресс записи в буффере, переписать эту запись, очистить текущий сектор, а
 * заполнить его снова данными из буффера
 */
operationres ICACHE_FLASH_ATTR
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

	if ( OPERATION_FAIL == ( adress = foundLine(oldLine) ) ){
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
operationres ICACHE_FLASH_ATTR
delete( uint8_t *removableLine ) {
	uint8_t   strAdrOfSecThatContRemLine;     // startAdressOfSectorThatContainsRemovableLine
	uint32_t  absAdrOfRemLineInFlash;         // absoluteAdressOfRemovableLineInFlash
	uint16_t  reltAdrOfRemLine;               // relativeAdressOfRemovableLine адрес удаляемой записи относительно
	                                          // начала сектора ( тоже самое что относительно начала tmp[] )
	uint16_t i;

	if ( strlen( removableLine ) != LINE_SIZE ){		// длинна обновляемой записи не больше
			return OPERATION_FAIL;   					// заданой длинны LINE_SIZE во время компиляции
	}

	if ( OPERATION_FAIL == ( absAdrOfRemLineInFlash = foundLine( removableLine ) ) ){
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
 * В пустые поля искомой строки должны быть записаны \r
 * Функция работает следующим образом:
 * - на вход приходит строка в которой заполнены не все поля
 *   и необходимо найти в куче запись в которой хотя бы одно
 *   поле будет такое же как в искомой записи
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
		spi_flash_write( currentSector*SPI_FLASH_SEC_SIZE, (uint32 *)&markerEnable, 1 ); // ставим метку что сектор пустой
	}

	return OPERATION_OK;
 }


/*
 * uint32_t ICACHE_FLASH_ATTR foundLine( uint8_t *line )
 * Функция возвращает 0 если не найдено ни одного совпадения
 * либо адресс записи если найдено совпадение
 * Description:
 * 	Читаем начальный сектор START_SECTOR в ОЗУ, проверяем что сектор не пустой
 * 	если пустой, то читаем следующий. Если же сектор не пустой то последовательно
 * 	сравниваем записи до тех пор:
 * 		- пока не будет найдена искомая строка ( strcmp вернет 0 )
 * 		- пока strcmp не вернет значение != 0, это свидетильствует
 * 		  о том что это последняя запись (она закнчивается на 0х03 а не на 0х00).
 * 		  В таком случае последнюю запись необходимо проверить другим образом:
 * 		   - просто в ОЗУ вместо 0х03 записать 0х00, если не строки не совпали
 * 		     то читаем след. сектор и так далее, пока не проверим все выделиненные
 * 		     сектора включая END_SECTOR, или не найдем запись
 *
 */
uint32_t ICACHE_FLASH_ATTR
findLine( uint8_t *line ) {
	uint8_t currentSector = START_SECTOR;
	uint16_t len = strlen(line) + 1; // функция возвращает длинну без учета \0
	uint8_t *p = &tmp[1];

	 // длинна входящей строки не должна быть больше установленой длинны LINE_SIZE
	if ( len > LINE_SIZE ) { return OPERATION_FAIL; }


	for ( ; currentSector <= END_SECTOR;  currentSector++) {                // пока не дошли до конца выделенной памяти
#ifdef  DB_DEBUG
		ets_uart_printf("Read iteration");
#endif
		if ( SPI_FLASH_RESULT_OK != spi_flash_read( SPI_FLASH_SEC_SIZE*currentSector , (uint32 *)tmp, SPI_FLASH_SEC_SIZE ) ){
				return OPERATION_FAIL;
#ifdef  DB_DEBUG
		ets_uart_printf("Read fail");
#endif
		}

		if ( MARKER_DISABLE == tmp[0] ) {   // сектор не пустой

		    for ( p = &tmp[1]; p < &tmp[SPI_FLASH_SEC_SIZE]; p += LINE_SIZE ) { // пока не дошли до конца сектора
			    if ( 0 == strcmp( line, p ) ) { return currentSector*SPI_FLASH_SEC_SIZE + (p - &tmp[0]);      // найдено совпадение
			    }
			    else if ( MARKER_ENABLE == p[LINE_SIZE - 1] ) {
				    p[LINE_SIZE - 1] = MARKER_DISABLE;
				    if ( 0 == strcmp( line, p ) ) { return currentSector*SPI_FLASH_SEC_SIZE + (p - &tmp[0]);  // найдено совпадение
				    } else { break; }
			    }

		   }
		}
	}
#ifdef  DB_DEBUG
		ets_uart_printf("Nothing founded");
#endif
	return OPERATION_FAIL;        // если дошли до этой строчки значит совпадений не было найдено
 }

operationres ICACHE_FLASH_ATTR
query() {


}





