/**
 ************************************************************************************
  * @file    myDB.c
  * @author  Denys
  * @version V0.0.1
  * @date    7-Sept-2015
  * @brief
 ************************************************************************************
 * @info:
 * 		Сдесь реализованы четыре основные функции необходимые для работы с кучей
 * 		- найти запись по записи
 * 		- сделать запись
 * 		- удалить запись
 * 		- очистить выделенную область под хранение кучи
 * @note:
 * 	-	Для коректной работы с флешь памятью данные хранить в ASCII (windows - 1251)
 * 	-	В функциях spi_flash_write и spi_flash_read запись и чтение производится
 * 		по 4 байта, если указать в этих функциях значение которе не кратно четырем
 * 		то оно все равно округлится до ближайшего большего значения, которое кратно
 * 		четырем, для коректной работы это нужно учитывать.
 * 	-	О том что сектор пустой свидетельствует маркер записанный в первмом байте.
 * 		MARKER_ENABLE - если сектор пустой, и MARKER_DISABLE - если в секторе есть
 * 		запись/записи. Если сектор пустой то запись делать начиная со второго байта.
 * 		Если сектор не пустой 1-й байт = MARKER_DISABLE, то необходимо найти
 * 		последнюю запись. Для этого последний байт каждой записи зарезервирован для
 * 		маркера MARKER_ENABLE/MARKER_DISABLE
 ************************************************************************************
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
 *  - во-первых проверяем что запись меньше величины сектора,
 * 	- далее вычитываем один сектор флешь памяти в ОЗУ
 * 	- далее, ищем конец последней записи и если сектор полный,
 *    читаем следующий и так далее
*/
writeres ICACHE_FLASH_ATTR
writeLine( uint8_t *line ) {

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
	 // длинна входящей строки не должна быть больше установленой длинны LINE_SIZE
	if ( len > SPI_FLASH_SEC_SIZE || len > LINE_SIZE ) { return WRITE_FAIL; }

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

operationres ICACHE_FLASH_ATTR
cleanAllSectors( void ) {
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
operationres ICACHE_FLASH_ATTR
foundLine( uint8_t *line ) {
	uint8_t currentSector = START_SECTOR;
	uint16_t len = strlen(line) + 1; // функция возвращает длинну без учета \0
	uint8_t *p = &tmp[1];

	 // длинна записи не больше величины сектора  SPI_FLASH_SEC_SIZE
	 // длинна входящей строки не должна быть больше установленой длинны LINE_SIZE
	if ( len > SPI_FLASH_SEC_SIZE || len > LINE_SIZE ) { return WRITE_FAIL; }


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

		    for ( ; p <= &tmp[SPI_FLASH_SEC_SIZE - 1 - LINE_SIZE]; p += LINE_SIZE ) { // пока не дошли до конца сектора
			    if ( 0 == strcmp( line, p ) ) { return OPERATION_OK;      // найдено совпадение
			    }
			    else if ( MARKER_ENABLE == p[LINE_SIZE - 1] ) {
				    p[LINE_SIZE - 1] = MARKER_DISABLE;
				    if ( 0 == strcmp( line, p ) ) { return OPERATION_OK;  // найдено совпадение
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
delLine( uint8_t *line ) {

 }


