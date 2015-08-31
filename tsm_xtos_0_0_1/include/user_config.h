#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define DEBUG

//----------------------------------------------------------------
#define OUT_1_GPIO 		4
#define OUT_1_MUX 		PERIPHS_IO_MUX_GPIO4_U
#define OUT_1_FUNC 		FUNC_GPIO4

#define OUT_2_GPIO		5
#define OUT_2_MUX 		PERIPHS_IO_MUX_GPIO5_U
#define OUT_2_FUNC 		FUNC_GPIO5
//-----------------------------------------------------------------
//status_led
#define LED_GPIO		15
#define LED_MUX 		PERIPHS_IO_MUX_MTDO_U
#define LED_FUNC 		FUNC_GPIO15
//-----------------------------------------------------------------
//-----------------------------------------------------------------
#define INP_1 			BIT12
#define INP_1_MUX 		PERIPHS_IO_MUX_MTDI_U
#define INP_1_FUNC 		FUNC_GPIO12

#define INP_2 			BIT13
#define INP_2_MUX 		PERIPHS_IO_MUX_MTCK_U
#define INP_2_FUNC 		FUNC_GPIO13

#define INP_3 			BIT14
#define INP_3_MUX 		PERIPHS_IO_MUX_GPIO2_U
#define INP_3_FUNC 		FUNC_GPIO14

#define INP_4 			BIT2
#define INP_4_MUX 		PERIPHS_IO_MUX_MTMS_U
#define INP_4_FUNC 		FUNC_GPIO2
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//STA
#define SSID_STA "TSM_Guest" //"DIR-320"
#define PWD_STA "tsmguest"
//----------------------------------------------------------------
//Udp
#define DELAY 	100 /* milliseconds */
#endif
//----------------------------------------------------------------
//brodcast strings
#define NAME "name: "
#define MAC " mac: "
#define IP " ip: "
#define SERVER_PORT " server port: "
#define RSSI " rssi: "

