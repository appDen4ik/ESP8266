#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"

#include "driver/uart_register.h"
#include "driver/uart.h"

#include "driver/i2c.h"
#include "driver/i2c_oled.h"
#include "driver/i2c_bmp180.h"
#include "driver/i2c_sht21.h"
#include "driver/i2c_ina219.h"

#define sleepms(x) os_delay_us(x*1000);
#define MAINLOOPSLEEP  2500

extern void ets_wdt_disable(void);

static volatile bool OLED, BMP, SHT, INA;

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void loop(os_event_t *events);

void user_rf_pre_init(void)
{
}





//Main code function
static void ICACHE_FLASH_ATTR
loop(os_event_t *events)
{
  char oledbuffer[21+1];
  
  OLED_Fill(0xf0);

  os_printf("MainLoop\r\n");

  if (BMP) {
      uint32_t t1 = BMP180_GetVal(GET_BMP_TEMPERATURE);
      uint32_t p1 = BMP180_GetVal(GET_BMP_REAL_PRESSURE);
      os_printf("BMP T=%3d.%1d*C P=%4dhPa\r\n",t1/10,t1%10,p1/100);
      if (OLED) {
        os_sprintf(oledbuffer, "%3d.%1dC  %4dhPa",t1/10,t1%10,p1/100);
        OLED_Line(1, oledbuffer, 2);      
      }
  }
   
  if (SHT) {
      int16_t t2 = SHT21_GetVal(GET_SHT_TEMPERATURE);
      int16_t h1 = SHT21_GetVal(GET_SHT_HUMIDITY);
      os_printf("SHT T=%3d.%1d*C H=%3d.%1d%%RH\r\n",t2/10,t2%10,h1/10,h1%10);
      if (OLED) {
        os_sprintf(oledbuffer, "%3d.%1dC  %3d.%1d%%H",t2/10,t2%10,h1/10,h1%10);
        OLED_Line(2, oledbuffer, 2);       
      }  
  }
  
  if (INA) {
    if (INA219_GetVal(CONFIGURE_READ_POWERDOWN)) {  
      uint32_t voltage = INA219_GetVal(GET_VOLTAGE);
      uint32_t current = INA219_GetVal(GET_CURRENT);
      uint32_t shuntvoltage = INA219_GetVal(GET_SHUNT_VOLTAGE);
      uint32_t power = INA219_GetVal(GET_POWER);
      os_printf("INA V=%dmV A=%dmA SV=%dmV P=%dmW\r\n",voltage,current,shuntvoltage,power);
      if (OLED) {
        os_sprintf(oledbuffer, "%4dmV   %4dmA",voltage,current);
        OLED_Line(4, oledbuffer, 2);
      }
    }  
  }

  sleepms(MAINLOOPSLEEP);
  system_os_post(user_procTaskPrio, 0, 0 );
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
  extern void ets_wdt_disable(void);

  uart_init(BIT_RATE_115200, BIT_RATE_115200);

  i2c_init();
  OLED = OLED_Init();
  INA = INA219_Init();  
  BMP = BMP180_Init();
  SHT = SHT21_Init();

  //Start os task
  system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
  system_os_post(user_procTaskPrio, 0, 0 );
}
