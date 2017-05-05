
#include "common.h"

void printINIT( char *str ) {
  char strTemp[40] ;
  char extAddr[17] ;
  extern afAddrType_t LEMAPP_Coor_DstAddr ;
  extern endPointDesc_t LEMAPP_epDesc ;
  extern byte LEMAPP_TransID ;
  memset(strTemp, 0, 40) ;
  memset(extAddr, 0, 17) ;
  Addr64ToStr(extAddr) ;
  strcpy(strTemp, "&00") ;
  strcat(strTemp, str) ;
  strcat(strTemp, extAddr) ;
  strcat(strTemp, "$") ;
  HalUARTWrite(0, (unsigned char*)strTemp, strlen(strTemp)) ;
  HalUARTWrite(0, "\n", 1) ;
  AF_DataRequest( &LEMAPP_Coor_DstAddr, &LEMAPP_epDesc, LEMAPP_COOR,
		osal_strlen(strTemp), (byte*)(strTemp), &LEMAPP_TransID,
		AF_DISCV_ROUTE , AF_DEFAULT_RADIUS ) ;
}

unsigned char recvBuf[128] ;

void rxCB( uint8 port, uint8 event ) {
  extern uint8 LEMAPP_TaskID ;
  memset(recvBuf, 0, 128) ;
  int len = HalUARTRead(0, (unsigned char*)recvBuf, 128) ;
  if ( len ) osal_set_event(LEMAPP_TaskID, LEM_UART_RECV) ;  
}

/******************************************************************
 @ The Hex transfers to char[] 
 @ src : Hex number 
 @ buf : a char* pointer // Attention: the space should be pre-allocated 
 @ ATTENTION: ONLY SUIT FOR UINT16(0XFFFF) 
 */
  
unsigned char _u_status_info[2] ;
   
void THexToStr( unsigned char *str, uint8 src ) {
  memset(str, 0, 2) ;
  for ( int i = 0 ; i < 2 ; i++ ) {
    uint8 single = src & 0x0f ;
    if (single < 10) single += '0' ;
    else single = (single - 10) + 'A' ;
    str[1-i] = single ;
    src >>= 4 ;
  }  
}
   
void HexToStr( uint16 src, unsigned char *buf ) {
  uint8 single;
  for (int i = 0; i < 4; i++) {
    single = src & 0x000f ;
    if (single < 10) single += '0' ;
    else single = (single - 10) + 'A' ;
    buf[3-i] = single ;
    src >>= 4 ;
  }
}

void Addr64ToStr( char *addr ) {
  NLME_GetExtAddr() ;
  char str[16] ;
  for ( int i = 0 ; i < 16 ; i+=2 ) {
    str[i] = saveExtAddr[i/2] & 0x0f ;
    if (str[i]< 10) str[i] += '0' ;
    else str[i] = (str[i] - 10) + 'A' ;   
    
    char tmp = saveExtAddr[i/2] & 0xf0 ;
    tmp >>= 4 ;
    str[i+1] = tmp ;
    if (str[i+1]< 10) str[i+1] += '0' ;
    else str[i+1] = (str[i+1] - 10) + 'A' ;   
  }
  for ( int i= 0 ; i < 16 ; i++ )
    addr[i] = str[15-i] ;
}

void PAddr64ToStr( char *addr ) {
  NLME_GetCoordExtAddr(saveExtAddr) ;
  char str[16] ;
  for ( int i = 0 ; i < 16 ; i+=2 ) {
    str[i] = saveExtAddr[i/2] & 0x0f ;
    if (str[i]< 10) str[i] += '0' ;
    else str[i] = (str[i] - 10) + 'A' ;   
    
    char tmp = saveExtAddr[i/2] & 0xf0 ;
    tmp >>= 4 ;
    str[i+1] = tmp ;
    if (str[i+1]< 10) str[i+1] += '0' ;
    else str[i+1] = (str[i+1] - 10) + 'A' ;   
  }
  for ( int i= 0 ; i < 16 ; i++ )
    addr[i] = str[15-i] ;  
}

uint16 getVddvalue( void ) {
  unsigned int value ;
  unsigned char tmpADCCON3 =ADCCON3 ;
  ADCIF = 0 ;
  ADCCON3 = ( ADC_REF_115V | ADC_DEC_064 | ADC_CHN_VDD3 ) ;
  while ( !ADCIF ) ;
  value = ADCH ;
  ADCCON3 = tmpADCCON3 ;
  return value ;
}

int ifEqualIEEE( char *str ) {
  char extAddr[17] ;
  memset(extAddr,0,17) ;
  Addr64ToStr(extAddr) ;
  extAddr[16] = 0 ;
  char tmpAddr[17] ;
  memset(tmpAddr,0,17) ;
  strncpy(tmpAddr, str, 16) ;
  tmpAddr[16] = 0 ;
  if ( strcmp(extAddr, tmpAddr) == 0 ) return 1 ;
  else return 0 ;
}

typedef unsigned int uint ;

unsigned char RXTXflag = 1;
uint  datanumber = 0;
uint  stringlen;

/*
#pragma vector = URX0_VECTOR
__interrupt void UART0_ISR(void) {
 	URX0IF = 0;				//清中断标志
	temp = U0DBUF;                          
}*/

void Delay( void ) {
  for ( int i = 0 ; i < 500 ; i++ )
    for ( int i = 0 ; i < 10 ; i++ ) ;
}

// 串口初始化函数     
void InitUart( void )
{
//    CLKCONCMD &= ~0x40;      // 设置系统时钟源为 32MHZ晶振
//    while(CLKCONSTA & 0x40); // 等待晶振稳定 
//    CLKCONCMD &= ~0x47;      // 设置系统主时钟频率为 32MHZ
    
    PERCFG = 0x00;           //位置1 P0口 
    P0SEL = 0x3c;            //P0_2,P0_3,P0_4,P0_5用作串口,第二功能 
    P2DIR &= ~0xC0;          //P0 优先作为UART0 ，优先级
    
    U0CSR |= 0x80;           //UART 方式 
    U0GCR |= 11;             //U0GCR与U0BAUD配合     
    U0BAUD |= 216;           // 波特率设为115200 
    UTX0IF = 0;              //UART0 TX 中断标志初始置位0 
    U0CSR |= 0X40;				  //允许接收
}

//串口发送函数    
void UartSendString(unsigned char *Data, int len) 
{
    int j;
    for(j=0;j<len;j++) 
    {
      Delay() ;
        U0DBUF = *Data++ ; 
        while (UTX0IF == 0) ; 
        UTX0IF = 0 ; 
    } 
    
}

int UartReceiveString( unsigned char *Recdata ) {
    int i = 0 ;
    unsigned char temp = 0 ;
    while( i < 32 ) {
        // URX0IF 会存在一个中断
        // 如果有数据从串口进入CC2530 URX0IF 会等于 1
        while ( URX0IF == 0 ) ;
        LED2 = 0 ;	       //接收状态指示
        URX0IF = 0 ;				//清中断标志
	temp = U0DBUF ;                
        if((temp!='#'))     //’＃‘被定义为结束字符，最多能接收16个字符       
          Recdata[i++] = temp ;
        else
          break ;
        temp = 0 ;       
        LED2 = 1 ;				     //接收状态指示
      }
   return i ;
}