
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
afAddrType_t LEMAPP_BROADCAST_DstAddr ;
char input_type[32] ;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void LEMAPP_MessageMSGCB( afIncomingMSGPacket_t *pckt );
/* 应答函数 */
static void LEMAPP_ACK_LINKQLT_MSG( afIncomingMSGPacket_t *pkt ) ;
static void LEMAPP_ACK_TOPO_MSG( void ) ;
static void LEMAPP_ACK_PERMIT_MSG( void )  ;
static void LEMAPP_SEND_REQ_LINKQLT( void ) ;
static void LEMAPP_SEND_REQ_TOPO( void ) ;
static void LEMAPP_SEND_REQ_CTR( char *addr ) ;
static void LEMAPP_SEND_REQ_PERMIT( void ) ;
static void LEMAPP_SEND_PKT_PC( afIncomingMSGPacket_t *pkt ) ;

/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

void LEMAPP_Init( uint8 task_id )
{
  /*  端点描述 */
  LEMAPP_TaskID = task_id ;
  LEMAPP_NwkState = DEV_INIT ;
  LEMAPP_TransID = 0 ;
  
  /*  UART 初始化内容 */
  halUARTCfg_t myUart ;
  myUart.configured                 = TRUE ;
  myUart.baudRate                   = HAL_UART_BR_115200 ;
  myUart.flowControl                = FALSE ;
  myUart.callBackFunc               = rxCB ;
  HalUARTOpen(0, &myUart) ;
  HalUARTWrite(0, "Uart Init\n", 10) ;

  // Fill out the endpoint description.
  LEMAPP_epDesc.endPoint = LEMAPP_ENDPOINT;
  LEMAPP_epDesc.task_id = &LEMAPP_TaskID;
  LEMAPP_epDesc.simpleDesc = (SimpleDescriptionFormat_t *)&LEMAPP_SimpleDesc;
  LEMAPP_epDesc.latencyReq = noLatencyReqs;
  
  /* 地址 */
  LEMAPP_DstAddr.addrMode = (afAddrMode_t)Addr16Bit ;
  LEMAPP_DstAddr.endPoint = LEMAPP_ENDPOINT ;
  LEMAPP_DstAddr.addr.shortAddr = 0x0000 ;
  
  /* 协调器节点 */
  LEMAPP_Coor_DstAddr.addrMode = (afAddrMode_t)Addr16Bit ;
  LEMAPP_Coor_DstAddr.endPoint = LEMAPP_ENDPOINT ;
  LEMAPP_Coor_DstAddr.addr.shortAddr = 0x0000 ; // 协调器地址

  /* 广播地址 */
  LEMAPP_BROADCAST_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast ;
  LEMAPP_BROADCAST_DstAddr.endPoint = LEMAPP_ENDPOINT ;
  LEMAPP_BROADCAST_DstAddr.addr.shortAddr = 0xFFFF ;

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
  if ( events & SYS_EVENT_MSG ) {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( LEMAPP_TaskID );
    while ( MSGpkt ) {
      switch ( MSGpkt->hdr.event ) {
        case AF_DATA_CONFIRM_CMD :
          break;

        case AF_INCOMING_MSG_CMD:
          LEMAPP_MessageMSGCB( MSGpkt );
          break;

        case ZDO_STATE_CHANGE:
          LEMAPP_NwkState = (devStates_t)(MSGpkt->hdr.status);
          if ( (LEMAPP_NwkState == DEV_ROUTER) ) {
              printINIT("RouterConnect:") ;
            //  osal_start_timerEx( LEMAPP_TaskID, LEMAPP_SEND_LINK_EVT, LEMAPP_SEND_LINK_TIMEOUT ) ; 
          }
          break;

        default:
          break;
      }
      osal_msg_deallocate( (uint8 *)MSGpkt );
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( LEMAPP_TaskID ) ;
    }
    return (events ^ SYS_EVENT_MSG);
  }
    /* 接收到串口数据 */
  if ( events & LEM_UART_RECV ) {
    HalUARTWrite( 0, recvBuf, strlen((char*)recvBuf) ) ;
    HalUARTWrite(0, "\n", 1) ;
    memset( input_type, 0, 3 )  ;
    input_type[0] = recvBuf[0] ;
    input_type[1] = recvBuf[1] ;
    // 查看网络拓扑结构
    if( strcmp((char*)input_type, "04") == 0 ) {
      LEMAPP_SEND_REQ_TOPO() ;
    }
    // 获取各结点的路由状况，链路质量
    if( strcmp((char*)input_type, "05") == 0 ) {
      LEMAPP_SEND_REQ_LINKQLT() ;
    }
    // 允许设备加入网络
    if( strcmp((char*)input_type, "06") == 0 ) {
      NLME_PermitJoiningRequest(0x1E) ;
      LEMAPP_SEND_REQ_PERMIT() ;
    }
    if( 1 ) ; // 更新密钥
    // 指定某设备重启/休眠/...
    if( strcmp((char*)input_type, "08") == 0 ) {
      LEMAPP_SEND_REQ_CTR( recvBuf ) ;
    }
    if( 1 ) ; // 控制某端点单片机
    return ( events ^ LEM_UART_RECV ) ;
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
    /* 响应功能 */
    case LEMAPP_REQ_LINKQLT: 
      LEMAPP_ACK_LINKQLT_MSG(pkt) ;
      break ;
    case LEMAPP_REQ_TOPO:
      LEMAPP_ACK_TOPO_MSG() ;
      break ;
    case LEMAPP_REQ_PERMIT:
      NLME_PermitJoiningRequest(0x1E) ;
      LEMAPP_ACK_PERMIT_MSG() ;
      break ;
   /*  请求功能 */ 
    case LEMAPP_ACK_LINKQLT :
      LEMAPP_SEND_PKT_PC(pkt) ;
      break ;
    case LEMAPP_ACK_TOPO :
      LEMAPP_SEND_PKT_PC(pkt) ;
      break ;
    case LEMAPP_ACK_PERMIT:
      LEMAPP_SEND_PKT_PC(pkt) ;
      break ;
    case LEMAPP_ACK_CTR:
      LEMAPP_SEND_PKT_PC(pkt) ;
      break ;
  }
}

/********************************************************************
 * 这是输出该节点到目标结点之间的链路状况
 * @ 响应事件：收到 LEMAPP_REQ_LINKQLT
 * @ 回应事件：发送 LEMAPP_ACK_LINKQLT
 * @ 帧格式为：&+05+IEEE+RSSI+LQI+&
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
  HalUARTWrite(0, (unsigned char*)sendPkt, strlen(sendPkt)) ;
  HalUARTWrite(0, "\n", 1) ;
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
  HalUARTWrite(0, (unsigned char*)sendPkt, strlen(sendPkt)) ;
  HalUARTWrite(0, "\n", 1) ;
  AF_DataRequest( &LEMAPP_DstAddr, &LEMAPP_epDesc, LEMAPP_ACK_TOPO,
                              strlen((char*)sendPkt), (byte*)(sendPkt), &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;  
}

/********************************************************************
 * 
 */
static void LEMAPP_ACK_PERMIT_MSG( void ) {
  // 初始化
  unsigned char tmp[3] ;
  unsigned char extmp[5] ;
  // char extAddr[17] ;
  uint16 shortAddr  ;
  memset(sendPkt, 0, 64 ) ;
  memset(tmp, 0, 3 ) ;
  // 数据包头
  sendPkt[0] = '&' ;
  strcat(sendPkt, "06\0" ) ; // 回传设备状态信息的包
  // own 16位地址
  memset(extmp, 0, 5) ;
  shortAddr = NLME_GetShortAddr() ;
  HexToStr( shortAddr, extmp ) ;
  strcat((char*)sendPkt, (char*)extmp) ;
  strcat((char*)sendPkt, "SUCCESS!") ;
  // 包尾
  strcat(sendPkt, "$") ;
  // send!
  HalUARTWrite(0, (unsigned char*)sendPkt, strlen(sendPkt)) ;
  HalUARTWrite(0, "\n", 1) ;
  AF_DataRequest( &LEMAPP_DstAddr, &LEMAPP_epDesc, LEMAPP_ACK_PERMIT,
                              strlen((char*)sendPkt), (byte*)(sendPkt), &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;  
}

static void LEMAPP_SEND_PKT_PC( afIncomingMSGPacket_t *pkt ) {
  HalUARTWrite( 0, pkt->cmd.Data, pkt->cmd.DataLength ) ;
  HalUARTWrite( 0, "\n", 1 ) ; 
}

/******************************************************************************
 *
 *
 */
static void LEMAPP_SEND_REQ_TOPO( void ) {
  HalLedBlink(HAL_LED_1, 5, 50, 50) ; 
  AF_DataRequest( &LEMAPP_BROADCAST_DstAddr, &LEMAPP_epDesc, LEMAPP_REQ_TOPO,
                              3, (byte*)("04\0"), &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;  
}

static void LEMAPP_SEND_REQ_LINKQLT( void ) {
  HalLedBlink(HAL_LED_1, 5, 50, 50) ; 
  AF_DataRequest( &LEMAPP_BROADCAST_DstAddr, &LEMAPP_epDesc, LEMAPP_REQ_LINKQLT,
                              3, (byte*)("05\0"), &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;  
}

static void LEMAPP_SEND_REQ_CTR( char *addr ) {
  HalLedBlink(HAL_LED_1, 5, 50, 50) ; 
  strcpy(&addr[0], &addr[2]) ;
  addr[16] = 0 ;
  HalUARTWrite( 0, addr, 16) ;
  AF_DataRequest( &LEMAPP_BROADCAST_DstAddr, &LEMAPP_epDesc, LEMAPP_REQ_CTR,
                              16, (byte*)(addr), &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;  
}

static void LEMAPP_SEND_REQ_PERMIT( void ) {
  HalLedBlink(HAL_LED_1, 5, 50, 50) ; 
  AF_DataRequest( &LEMAPP_BROADCAST_DstAddr, &LEMAPP_epDesc, LEMAPP_REQ_PERMIT,
                              3, (byte*)("06\0"), &LEMAPP_TransID,
                              AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) ;  
}