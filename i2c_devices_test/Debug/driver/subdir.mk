################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../driver/i2c.c \
../driver/i2c_bmp180.c \
../driver/i2c_ina219.c \
../driver/i2c_oled.c \
../driver/i2c_sht21.c \
../driver/uart.c 

OBJS += \
./driver/i2c.o \
./driver/i2c_bmp180.o \
./driver/i2c_ina219.o \
./driver/i2c_oled.o \
./driver/i2c_sht21.o \
./driver/uart.o 

C_DEPS += \
./driver/i2c.d \
./driver/i2c_bmp180.d \
./driver/i2c_ina219.d \
./driver/i2c_oled.d \
./driver/i2c_sht21.d \
./driver/uart.d 


# Each subdirectory must supply rules for building sources it contributes
driver/%.o: ../driver/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"C:\Espressif\ESP8266_SDK\include\" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


