
/****************************************************************/
#ifndef COMMON_H
#define COMMON_H

#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "AF.h"
#include "ZGlobals.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "DebugTrace.h"

//#if !defined( WIN32 )
  #include "OnBoard.h"
//#endif

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_adc.h"
#include "hal_drivers.h"
#include "hal_assert.h"
#include "hal_flash.h"
#include "hal_uart.h"
#include "NLMEDE.h"
#include "nwk_util.h"

/* UART */
#include "MT_UART.h"
#include "MT_APP.h"
#include "MT.h"

#include "ZComDef.h"
#include "string.h"

/* 组播 */
#include "aps_groups.h"

/* Other */
#include <stdio.h>
#include <stdlib.h>

/****************************************************************/
#define LEMAPP_ENDPOINT           10

#define LEMAPP_PROFID 0x0F04
#define LEMAPP_DEVICEID 0x0001
#define LEMAPP_DEVICE_VERSION 0
#define LEMAPP_FLAGS 0

#define LEMAPP_MAX_CLUSTERS       10
#define LEMAPP_COOR                     1              // 发送数据包给协调器
#define LEMAPP_BROADCAST            2             // 全网广播
#define LEMAPP_ACK_LINKQLT          10             // 设备状态信息
#define LEMAPP_REQ_LINKQLT          11
#define LEMAPP_ACK_TOPO              12
#define LEMAPP_REQ_TOPO              13
#define LEMAPP_ACK_CTR                 14
#define LEMAPP_REQ_CTR                 15
#define LEMAPP_ACK_PERMIT            16
#define LEMAPP_REQ_PERMIT            17

#define LEMAPP_SEND_MSG_TIMEOUT 180000     //  终端设备上传传感数据的时间间隔
#define LEMAPP_SEND_LINK_TIMEOUT 300000     //  各个设备上传自己节点链路及设备状态信息的间隔

#define LEMAPP_SEND_PKT_EVT              0x0001
#define LEMAPP_SEND_LINK_EVT            0x0002

#define LEM_UART_RECV  0x0010

#define GROUP_ID 0x0001
/****************************************************************/

// 缓冲区
extern unsigned char recvBuf[128] ;

/****************************************************************/

// 串口接收函数回调
extern void rxCB( uint8 port, uint8 event ) ;

/*********************************************************************/

/******************************************************************
 * 电压信息
 */
#define ADC_REF_115V            0x00
#define ADC_DEC_256             0x20
#define ADC_CHN_TEMP         0x0e
#define ADC_DEC_064             0x00
#define ADC_CHN_VDD3         0x0f

extern unsigned char _u_status_info[2] ;
extern void THexToStr( unsigned char *str, uint8 src ) ;
extern void HexToStr( uint16 src, unsigned char *buf ) ;
extern void Addr64ToStr( char *str ) ;
extern void PAddr64ToStr( char *str ) ;
extern uint16 getVddvalue( void ) ;
extern int ifEqualIEEE( char *str1 ) ;
extern void printINIT( char *str ) ;

/*********************************************************************/

#define LED1 P1_0
#define LED2 P1_1
#define LED3 P1_4

void InitUart( void );   //初始化串口
void UartSendString(unsigned char *Data,int len);
int UartReceiveString( unsigned char *Recdata ) ;
void Delay( void ) ;
//__interrupt void UART0_ISR(void) ;

#endif
