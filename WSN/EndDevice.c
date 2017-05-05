
/*********************************************************************
 * INCLUDES
 */
#include "LEMApp.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

char sendPkt[64] ;

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
// This list should be filled with Application specific Cluster IDs.
const cId_t LEMAPP_ClusterList[LEMAPP_MAX_CLUSTERS] = {
  LEMAPP_COOR,
  LEMAPP_BROADCAST,
  LEMAPP_ACK_LINKQLT,
  LEMAPP_REQ_LINKQLT,
  LEMAPP_ACK_TOPO,
  LEMAPP_REQ_TOPO,
  LEMAPP_ACK_CTR,
  LEMAPP_REQ_CTR,
  LEMAPP_ACK_PERMIT,
  LEMAPP_REQ_PERMIT
} ;

const SimpleDescriptionFormat_t LEMAPP_SimpleDesc =
{
  LEMAPP_ENDPOINT,              //  int Endpoint;
  LEMAPP_PROFID,                //  uint16 AppProfId[2];
  LEMAPP_DEVICEID,              //  uint16 AppDeviceId[2];
  LEMAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
  LEMAPP_FLAGS,                 //  int   AppFlags:4;
  LEMAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)LEMAPP_ClusterList,  //  byte *pAppInClusterList;
  LEMAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)LEMAPP_ClusterList   //  byte *pAppInClusterList;
};

endPointDesc_t LEMAPP_epDesc;

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
byte LEMAPP_TaskID; 
devStates_t LEMAPP_NwkState;
byte LEMAPP_TransID;  // This is the unique message ID (counter)
afAddrType_t LEMAPP_DstAddr;
// 发往协调器
afAddrType_t LEMAPP_Coor_DstAddr ;
static uint32 txMsgDelay = LEMAPP_SEND_MSG_TIMEOUT;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void LEMAPP_MessageMSGCB( afIncomingMSGPacket_t *pckt );
static void LEMAPP_Send_Coor_Message( void ) ;
void PktSenserData( char *sendPkt, char *value ) ;
/* 应答函数 */
static void LEMAPP_ACK_LINKQLT_MSG( afIncomingMSGPacket_t *pkt ) ;
static void LEMAPP_ACK_TOPO_MSG( void ) ;
static void LEMAPP_ACK_CTR_MSG( afIncomingMSGPacket_t *pkt )  ;

/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */

/*********************************************************************
 * PUBLIC FUNCTIONS
 */
int i = 0 ;

void LEMAPP_Init( uint8 task_id )
{
  /*  端点描述 */
  LEMAPP_TaskID = task_id;
  LEMAPP_NwkState = DEV_INIT;
  LEMAPP_TransID = 0;
  
  // Fill out the endpoint description.
  LEMAPP_epDesc.endPoint = LEMAPP_ENDPOINT;
  LEMAPP_epDesc.task_id = &LEMAPP_TaskID;
  LEMAPP_epDesc.simpleDesc
            = (SimpleDescriptionFormat_t *)&LEMAPP_SimpleDesc;
  LEMAPP_epDesc.latencyReq = noLatencyReqs;
  
  /* 地址 */
  LEMAPP_DstAddr.addrMode = (afAddrMode_t)Addr16Bit ;
  LEMAPP_DstAddr.endPoint = LEMAPP_ENDPOINT ;
  LEMAPP_DstAddr.addr.shortAddr = 0x0000 ;
  
  /* 协调器节点 */
  LEMAPP_Coor_DstAddr.addrMode = (afAddrMode_t)Addr16Bit ;
  LEMAPP_Coor_DstAddr.endPoint = LEMAPP_ENDPOINT ;
  LEMAPP_Coor_DstAddr.addr.shortAddr = 0x0000 ; // 协调器地址

  // Register the endpoint description with the AF
  afRegister( &LEMAPP_epDesc );

  // Register for all key events - This app will handle all key events
  RegisterForKeys( LEMAPP_TaskID );

  ZDO_RegisterForZDOMsg( LEMAPP_TaskID, End_Device_Bind_rsp );
  ZDO_RegisterForZDOMsg( LEMAPP_TaskID, Match_Desc_rsp );

}

uint16 LEMAPP_ProcessEvent( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( LEMAPP_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        case MT_SYS_APP_MSG:
          HalUARTWrite(0, "HHH\n", 4) ;
          break ;
        case AF_DATA_CONFIRM_CMD :
          break;

        case AF_INCOMING_MSG_CMD:
          LEMAPP_MessageMSGCB( MSGpkt );
          break;

        case ZDO_STATE_CHANGE:
          LEMAPP_NwkState = (devStates_t)(MSGpkt->hdr.status);
          if ( (LEMAPP_NwkState == DEV_END_DEVICE) ) {
            /* 读取本地IEEE地址 */
            printINIT("EndDeviceConnect:") ;
            Delay() ;
            LEMAPP_Send_Coor_Message() ;
            osal_start_timerEx( LEMAPP_TaskID, LEMAPP_SEND_PKT_EVT, LEMAPP_SEND_MSG_TIMEOUT ) ; 
          }
          break;

        default:
          break;
      }
      osal_msg_deallocate( (uint8 *)MSGpkt );
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( LEMAPP_TaskID );
    }
    return (events ^ SYS_EVENT_MSG);
  }
  
  if ( events & LEMAPP_SEND_PKT_EVT ) {
    HalLedBlink(HAL_LED_2, 5, 50, 50) ;  
    LEMAPP_Send_Coor_Message() ;
    osal_start_timerEx( LEMAPP_TaskID, LEMAPP_SEND_PKT_EVT, txMsgDelay ) ;
    return (events ^ LEMAPP_SEND_PKT_EVT) ;
  }
  return 0 ;
}

/********************************************************************
 * 子节点收到信息后，将包分类
 * @ LEMAPP_REQ_LINKQLT: 链路质量评定请求
 */
static void LEMAPP_MessageMSGCB( afIncomingMSGPacket_t *pkt ) {
  LEMAPP_DstAddr.addr.shortAddr = pkt->srcAddr.addr.shortAddr ;
  switch ( pkt->clusterId ) {
    case LEMAPP_REQ_LINKQLT: 
      LEMAPP_ACK_LINKQLT_MSG(pkt) ;
      break ;
    case LEMAPP_REQ_TOPO:
      LEMAPP_ACK_TOPO_MSG() ;
      break ;
    case LEMAPP_REQ_CTR:
      LEMAPP_ACK_CTR_MSG(pkt) ;
      break ;
  }
}


/********************************************************************
 * 子节点收到信息后，发送点播给协调器
 */
static void LEMAPP_Send_Coor_Message( void ) {
   LED1 = 0 ;
   LED2 = 0 ;
   LED3 = 0 ;
   /* 初始化BUF */
  char strTemp[32] ;
  osal_memset(strTemp, 0, 32) ;
  /* 发送数据给单片机 */
  InitUart() ;
  UartSendString("*****", 5) ;
  /* 从单片机接收数据 */
  while ( !UartReceiveString( (unsigned char*)strTemp ) ) ;
  int count = strTemp[0] - '0' ;
  for ( int i = 0 ; i < count ; i++ ) {
   osal_memset(strTemp,0,32) ;
   UartReceiveString( (unsigned char*)strTemp ) ;
   PktSenserData(sendPkt, strTemp) ;
   AF_DataRequest( &LEMAPP_Coor_DstAddr, &LEMAPP_epDesc, LEMAPP_COOR,
                              osal_strlen(sendPkt), (byte*)sendPkt, &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;
   Delay() ;
  }
  HalLedBlink(HAL_LED_3, 5, 50, 50) ;
   LED1 = 1 ;
   LED2 = 1 ;
   LED3 = 1 ;
}

/********************************************************************
 * 这是数据包合成函数
 * @ 帧格式为 : &+01+IEEE+type+value+$
 * @ sendPkt: 最终保存包的地方 
 * @ type: 两位表示--传感项目
 * @ value: 该传感器的值
 */
void PktSenserData( char *sendPkt, char *value ) {
  memset(sendPkt,0,64) ;
  sendPkt[0] = '&' ;
  strcat(sendPkt, "01\0") ;
  char extAddr[17] ;
  memset(extAddr,0,17) ;
  Addr64ToStr(extAddr) ;
  extAddr[16] = 0 ;
  strcat(sendPkt, extAddr) ;
  strcat(sendPkt, value) ; // tpye+value
  strcat(sendPkt, "$" ) ;
}

/********************************************************************
 * 这是输出该节点到目标结点之间的链路状况
 * @ 响应事件：收到 LEMAPP_REQ_LINKQLT
 * @ 回应事件：发送 LEMAPP_ACK_LINKQLT
 * @ 帧格式为：&+05+own16+ownIEEE+p16+pIEEE+RSSI+LQI+voltage+&
 */
static void LEMAPP_ACK_LINKQLT_MSG( afIncomingMSGPacket_t *pkt ) {
  // 初始化
  unsigned char tmp[3] ;
  unsigned char tmpV[5] ;
  unsigned char extmp[5] ;
  char extAddr[17] ;
  uint16 shortAddr  ;
  memset(sendPkt, 0, 64 ) ;
  memset(tmp, 0, 3 ) ;
  // 数据包头
  sendPkt[0] = '&' ;
  strcat(sendPkt, "05\0" ) ; // 回传设备状态信息的包
  // own 16位地址
  memset(extmp, 0, 5) ;
  shortAddr = NLME_GetShortAddr() ;
  HexToStr( shortAddr, extmp ) ;
  strcat((char*)sendPkt, (char*)extmp) ;
  // own IEEE地址
  memset(extAddr,0,17) ;
  Addr64ToStr(extAddr) ;
  strcat(sendPkt, extAddr) ;
  // parent 16位地址
  memset(extmp, 0, 5) ;
  shortAddr = NLME_GetCoordShortAddr() ;
  HexToStr( shortAddr, extmp ) ;
  strcat((char*)sendPkt, (char*)extmp) ;  
  // parent IEEE地址
  memset(extAddr,0,17) ;
  PAddr64ToStr(extAddr) ;
  strcat(sendPkt, extAddr) ;
  // RSSI
  THexToStr(tmp, pkt->rssi ) ;
  strcat((char*)sendPkt, (char*)tmp ) ; 
  // LQI
  THexToStr(tmp, pkt->LinkQuality ) ;
  strcat((char*)sendPkt, (char*)tmp) ;
  // 电压
  memset(tmpV, 0, 5) ;
  uint16 vddvalue =  69*getVddvalue()/256 ;
  HexToStr( vddvalue, tmpV ) ;
  strcat((char*)sendPkt, (char*)tmpV) ;
  // 包尾
  strcat(sendPkt, "$") ;
  // send!
  AF_DataRequest( &LEMAPP_DstAddr, &LEMAPP_epDesc, LEMAPP_ACK_LINKQLT,
                              strlen((char*)sendPkt), (byte*)(sendPkt), &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;  
}

/********************************************************************
 * 这是输出该节点及父节点的信息，用于生成网络拓扑结构
 * @ 响应事件：收到 LEMAPP_REQ_TOPO
 * @ 回应事件：发送 LEMAPP_ACK_TOPO
 * @ 帧格式为：&+03+自短地址+自长地址+父段地址+父长地址+&
 */
static void LEMAPP_ACK_TOPO_MSG( void ) {
  // 初始化
  unsigned char tmp[3] ;
  unsigned char extmp[5] ;
  char extAddr[17] ;
  uint16 shortAddr  ;
  memset(sendPkt, 0, 64 ) ;
  memset(tmp, 0, 3 ) ;
  // 数据包头
  sendPkt[0] = '&' ;
  strcat(sendPkt, "03\0" ) ; // 回传设备状态信息的包
  // own 16位地址
  memset(extmp, 0, 5) ;
  shortAddr = NLME_GetShortAddr() ;
  HexToStr( shortAddr, extmp ) ;
  strcat((char*)sendPkt, (char*)extmp) ;
  // own IEEE地址
  memset(extAddr,0,17) ;
  Addr64ToStr(extAddr) ;
  strcat(sendPkt, extAddr) ;
  // parent 16位地址
  memset(extmp, 0, 5) ;
  shortAddr = NLME_GetCoordShortAddr() ;
  HexToStr( shortAddr, extmp ) ;
  strcat((char*)sendPkt, (char*)extmp) ;  
  // parent IEEE地址
  memset(extAddr,0,17) ;
  PAddr64ToStr(extAddr) ;
  strcat(sendPkt, extAddr) ;
  // 包尾
  strcat(sendPkt, "$") ;
  // send!
//  HalUARTWrite(0, (unsigned char*)sendPkt, strlen(sendPkt)) ;
  AF_DataRequest( &LEMAPP_DstAddr, &LEMAPP_epDesc, LEMAPP_ACK_TOPO,
                              strlen((char*)sendPkt), (byte*)(sendPkt), &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;  
}

/********************************************************************
 * @ 响应事件：收到 LEMAPP_REQ_CTR
 * @ 回应事件：发送 LEMAPP_ACK_CTR
 * @ 帧格式为：
 */
static void LEMAPP_ACK_CTR_MSG( afIncomingMSGPacket_t *pkt ) {
  /* 判断是否为该设备的控制包 */
  if ( ifEqualIEEE((char*)pkt->cmd.Data) ) {
    HalLedBlink(HAL_LED_2, 5, 50, 50) ; 
    AF_DataRequest( &LEMAPP_DstAddr, &LEMAPP_epDesc, LEMAPP_ACK_CTR,
                              strlen("EndDevice ShutDown\n"), (byte*)("EndDevice ShutDown\n"), &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;
    /* 初始化BUF */
    char strTemp[32] ;
    memset(strTemp, 0, 32) ;
    /* 发送数据给单片机 */
    InitUart() ;
    UartSendString("!!!!!", 5) ;
    /* 从单片机接收数据 */
    Delay() ;
    while ( !UartReceiveString( (unsigned char*)strTemp ) ) ;
    AF_DataRequest( &LEMAPP_DstAddr, &LEMAPP_epDesc, LEMAPP_ACK_CTR,
                             osal_strlen(strTemp), (byte*)(strTemp), &LEMAPP_TransID,
                             AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;
  } 
}