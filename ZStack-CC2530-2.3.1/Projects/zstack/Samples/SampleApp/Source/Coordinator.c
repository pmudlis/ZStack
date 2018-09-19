/**************************************************************************************************
  Filename:       SampleApp.c
  Revised:        $Date: 2009-03-18 15:56:27 -0700 (Wed, 18 Mar 2009) $
  Revision:       $Revision: 19453 $

  Description:    Sample Application (no Profile).


  Copyright 2007 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED ?AS IS? WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

/*********************************************************************
  This application isn't intended to do anything useful, it is
  intended to be a simple example of an application's structure.

  This application sends it's messages either as broadcast or
  broadcast filtered group messages.  The other (more normal)
  message addressing is unicast.  Most of the other sample
  applications are written to support the unicast message model.

  Key control:
    SW1:  Sends a flash command to all devices in Group 1.
    SW2:  Adds/Removes (toggles) this device in and out
          of Group 1.  This will enable and disable the
          reception of the flash command.
*********************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "AF.h"
#include "OSAL.h"
#include "OnBoard.h"
#include "ZGlobals.h"
#include "AF.h"
#include "aps_groups.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include "stdio.h"sprin
#include "string.h"

#include "SampleApp.h"
#include "SampleAppHw.h"

/* HAL */
#include "hal_drivers.h"
#if defined (LCD_SUPPORTED )
  #include "hal_lcd.h"
#endif
#include "hal_led.h"
#include "hal_key.h"
#include "hal_adc.h"
#include "hal_uart.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

#if !defined( SAMPLE_APP_PORT )
#define SAMPLE_APP_PORT  0
#endif

#if !defined( SAMPLE_APP_BAUD )
#define SAMPLE_APP_BAUD  HAL_UART_BR_38400
//#define SAMPLE_APP_BAUD  HAL_UART_BR_115200
#endif

// When the Rx buf space is less than this threshold, invoke the Rx callback.
#if !defined( SAMPLE_APP_THRESH )
#define SAMPLE_APP_THRESH  64
#endif

#if !defined( SAMPLE_APP_RX_SZ )
#define SAMPLE_APP_RX_SZ  128
#endif

#if !defined( SAMPLE_APP_TX_SZ )
#define SAMPLE_APP_TX_SZ  128
#endif

// Millisecs of idle time after a byte is received before invoking Rx callback.
#if !defined( SAMPLE_APP_IDLE )
#define SAMPLE_APP_IDLE  6
#endif

// Loopback Rx bytes to Tx for throughput testing.
#if !defined( SAMPLE_APP_LOOPBACK )
#define SAMPLE_APP_LOOPBACK  FALSE
#endif

// This is the max byte count per OTA message.
#if !defined( SAMPLE_APP_TX_MAX )
#define SAMPLE_APP_TX_MAX  128
#endif

#define SAMPLE_APP_RSP_CNT  4

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
uint8 SerialApp_TaskID;    // Task ID for internal task/event processing.

uint8 SampleApp_TaskID;    // Task ID for internal task/event processing.

// This list should be filled with Application specific Cluster IDs.
const cId_t SampleApp_ClusterList[SAMPLEAPP_MAX_CLUSTERS] =
{
  SAMPLEAPP_PERIODIC_CLUSTERID,
  SAMPLEAPP_FLASH_CLUSTERID,
  SAMPLEAPP_CLUSTERID1,
  SAMPLEAPP_CLUSTERID2
};

const SimpleDescriptionFormat_t SampleApp_SimpleDesc =
{
  SAMPLEAPP_ENDPOINT,              //  int Endpoint;
  SAMPLEAPP_PROFID,                //  uint16 AppProfId[2];
  SAMPLEAPP_DEVICEID,              //  uint16 AppDeviceId[2];
  SAMPLEAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
  SAMPLEAPP_FLAGS,                 //  int   AppFlags:4;
  SAMPLEAPP_MAX_CLUSTERS,          //  uint8  AppNumInClusters;
  (cId_t *)SampleApp_ClusterList,  //  uint8 *pAppInClusterList;
  SAMPLEAPP_MAX_CLUSTERS,          //  uint8  AppNumInClusters;
  (cId_t *)SampleApp_ClusterList   //  uint8 *pAppInClusterList;
};

// This is the Endpoint/Interface description.  It is defined here, but
// filled-in in SampleApp_Init().  Another way to go would be to fill
// in the structure here and make it a "const" (in code space).  The
// way it's defined in this sample app it is define in RAM.
endPointDesc_t SampleApp_epDesc;

const endPointDesc_t SerialApp_epDesc =
{
  SAMPLEAPP_ENDPOINT,
 &SerialApp_TaskID,
  (SimpleDescriptionFormat_t *)&SampleApp_SimpleDesc,
  noLatencyReqs
};

/*********************************************************************
 * EXTERNAL VARIABLES
 */

typedef struct
{
    int temperature;           // -100 degrees C to 200 degrees C
    int water_level;           // -50m to 50m
    int  flow_rate;   	 // -10000 L/min to 10000 L/min
    uint8  PH;   		 // 0 to 14
    uint32  salinity;   	 // 0 to 2000000 mg
    uint8  batt_level;        // 0 to 100 percent
    float GNSS_latitude;         // -85 to 85 decimal degrees
    float GNSS_longitude;        // -180 to 180 decimal degrees
    
    bool sensors_okay;   	 // 1=good, 0=bad
    bool node_okay;   		 // 1=good, 0=bad
    char error_state[25];    	 // char message, null terminated "\n"
 
} data_sensor_outgoing;


/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
static uint8 SampleApp_TaskID;   // Task ID for internal task/event processing
                          // This variable will be received when
                          // SampleApp_Init() is called.
devStates_t SampleApp_NwkState;

uint8 SampleApp_TransID;  // This is the unique message ID (counter)

afAddrType_t SampleApp_Periodic_DstAddr;
afAddrType_t SampleApp_Flash_DstAddr;
afAddrType_t SampleApp_Broadcast;

aps_Group_t SampleApp_Group;

uint8 SampleAppPeriodicCounter = 0;
uint8 SampleAppFlashCounter = 0;

static afAddrType_t SampleApp_TxAddr;
static uint8 SampleApp_TxSeq;
static uint8 SampleApp_TxBuf[SAMPLE_APP_TX_MAX+1];
static uint8 SampleApp_TxLen;

static afAddrType_t SampleApp_RxAddr;
static uint8 SampleApp_RxSeq;
static uint8 SampleApp_RspBuf[SAMPLE_APP_RSP_CNT];

/*********************************************************************
 * LOCAL FUNCTIONS
 */
void SampleApp_HandleKeys( uint8 shift, uint8 keys );
void SampleApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
void SampleApp_SendPeriodicMessage( void );
void SampleApp_SendFlashMessage( uint16 flashTime );

static void SampleApp_CallBack(uint8 port, uint8 event);

/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      SampleApp_Init
 *
 * @brief   Initialization function for the Generic App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void SampleApp_Init( uint8 task_id )
{
  SampleApp_TaskID = task_id;
  SampleApp_NwkState = DEV_INIT;
  SampleApp_TransID = 0;

  // Device hardware initialization can be added here or in main() (Zmain.c).
  // If the hardware is application specific - add it here.
  // If the hardware is other parts of the device add it in main().

 #if defined ( BUILD_ALL_DEVICES )
  // The "Demo" target is setup to have BUILD_ALL_DEVICES and HOLD_AUTO_START
  // We are looking at a jumper (defined in SampleAppHw.c) to be jumpered
  // together - if they are - we will start up a coordinator. Otherwise,
  // the device will start as a router.
  if ( readCoordinatorJumper() )
    zgDeviceLogicalType = ZG_DEVICETYPE_COORDINATOR;
  else
    zgDeviceLogicalType = ZG_DEVICETYPE_ROUTER;
#endif // BUILD_ALL_DEVICES

#if defined ( HOLD_AUTO_START )
  // HOLD_AUTO_START is a compile option that will surpress ZDApp
  //  from starting the device and wait for the application to
  //  start the device.
  ZDOInitDevice(0);
#endif

  // Setup for the periodic message's destination address
  // Broadcast to everyone
  SampleApp_Periodic_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast;
  SampleApp_Periodic_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
  SampleApp_Periodic_DstAddr.addr.shortAddr = 0xFFFF;

  // Setup for the flash command's destination address - Group 1
  SampleApp_Flash_DstAddr.addrMode = (afAddrMode_t)afAddrGroup;
  SampleApp_Flash_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
  SampleApp_Flash_DstAddr.addr.shortAddr = SAMPLEAPP_FLASH_GROUP;
  
  SampleApp_Broadcast.addrMode = (afAddrMode_t)AddrBroadcast;
  SampleApp_Broadcast.endPoint = SAMPLEAPP_ENDPOINT;
  SampleApp_Broadcast.addr.shortAddr = 0xFFFF;

  // Fill out the endpoint description.
  SampleApp_epDesc.endPoint = SAMPLEAPP_ENDPOINT;
  SampleApp_epDesc.task_id = &SampleApp_TaskID;
  SampleApp_epDesc.simpleDesc
            = (SimpleDescriptionFormat_t *)&SampleApp_SimpleDesc;
  SampleApp_epDesc.latencyReq = noLatencyReqs;

  // Register the endpoint description with the AF
  afRegister( &SampleApp_epDesc );

  // Register for all key events - This app will handle all key events
  RegisterForKeys( SampleApp_TaskID );

  // By default, all devices start out in Group 1
  SampleApp_Group.ID = 0x0001;
  osal_memcpy( SampleApp_Group.name, "Group 1", 7  );
  aps_AddGroup( SAMPLEAPP_ENDPOINT, &SampleApp_Group );
  
  // Serial initialization start 
  halUARTCfg_t uartConfig;

  SampleApp_TaskID = task_id;
  SampleApp_RxSeq = 0xC3;

  afRegister( (endPointDesc_t *)&SampleApp_epDesc );

  RegisterForKeys( task_id );

  uartConfig.configured           = TRUE;              // 2x30 don't care - see uart driver.
  uartConfig.baudRate             = SAMPLE_APP_BAUD;
  uartConfig.flowControl          = TRUE;
  uartConfig.flowControlThreshold = SAMPLE_APP_THRESH; // 2x30 don't care - see uart driver.
  uartConfig.rx.maxBufSize        = SAMPLE_APP_RX_SZ;  // 2x30 don't care - see uart driver.
  uartConfig.tx.maxBufSize        = SAMPLE_APP_TX_SZ;  // 2x30 don't care - see uart driver.
  uartConfig.idleTimeout          = SAMPLE_APP_IDLE;   // 2x30 don't care - see uart driver.
  uartConfig.intEnable            = TRUE;              // 2x30 don't care - see uart driver.
  uartConfig.callBackFunc         = SampleApp_CallBack;
  HalUARTOpen (SAMPLE_APP_PORT, &uartConfig);
  // Serial Initialization end

#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "SerialApp+Labs", HAL_LCD_LINE_1 );
#endif
}

/*********************************************************************
 * @fn      SampleApp_ProcessEvent
 *
 * @brief   Generic Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  none
 */
uint16 SampleApp_ProcessEvent( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( SampleApp_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        // Received when a key is pressed
        case KEY_CHANGE:
          SampleApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        // Received when a messages is received (OTA) for this endpoint
        case AF_INCOMING_MSG_CMD:
          SampleApp_MessageMSGCB( MSGpkt );
          break;

        // Received whenever the device changes state in the network
        case ZDO_STATE_CHANGE:
          SampleApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
          if ( (SampleApp_NwkState == DEV_ZB_COORD)
              || (SampleApp_NwkState == DEV_ROUTER)
              || (SampleApp_NwkState == DEV_END_DEVICE) )
          {
            // Start sending the periodic message in a regular interval.
            osal_start_timerEx( SampleApp_TaskID,
                              SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
                              SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT );
          }
          else
          {
            // Device is no longer in the network
          }
          break;

        default:
          break;
      }

      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );

      // Next - if one is available
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( SampleApp_TaskID );
    }
    
    

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  // Send a message out - This event is generated by a timer
  //  (setup in SampleApp_Init()).
  if ( events & SAMPLEAPP_SEND_PERIODIC_MSG_EVT )
  {
    // Send the periodic message
    SampleApp_SendPeriodicMessage();

    // Setup to send message again in normal period (+ a little jitter)
    osal_start_timerEx( SampleApp_TaskID, SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
        (SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT + (osal_rand() & 0x00FF)) );

    // return unprocessed events
    return (events ^ SAMPLEAPP_SEND_PERIODIC_MSG_EVT);
  }

  // Discard unknown events
  return 0;
}

/*********************************************************************
 * Event Generation Functions
 */
/*********************************************************************
 * @fn      SampleApp_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */

typedef struct
{
  int16 temperature;
  int16 water_level;
  int32 flow_rate;
  uint8 PH;
  uint8 batt_level;
  int16 GNSS_latitude;
  int16 GNSS_longitude;
  
  bool sensors_okay;
  bool node_okay;
  uint8 error_state;
  char text[10];
} packet_measurements;
packet_measurements receiveData = {0,0,0,0,0,0,0,0,0,0,"nothing"};

char serialOut[150] = "";
char serialBuffer[150] = "";
char serialBuffer2[40] = "";

void SampleApp_HandleKeys( uint8 shift, uint8 keys )
{

  if ( keys & HAL_KEY_SW_1 )
  {
    HalLcdWriteStringValue( "Temperature:", receiveData.temperature, 10, HAL_LCD_LINE_1);
    HalLcdWriteStringValue( "Water Level:", receiveData.temperature, 10, HAL_LCD_LINE_2);
    HalLcdWriteStringValue( "Flow Rate:", receiveData.temperature, 10, HAL_LCD_LINE_3);        
  }

  if ( keys & HAL_KEY_SW_2 )
  {
    HalLcdWriteStringValue( "pH:", receiveData.PH, 10, HAL_LCD_LINE_1);
    HalLcdWriteStringValue( "Battery Level:", receiveData.batt_level, 10, HAL_LCD_LINE_2);
    HalLcdWriteStringValue( "Latitude:", receiveData.GNSS_latitude, 10, HAL_LCD_LINE_3);
  }
  
  if ( keys & HAL_KEY_SW_3 )
  {
    HalLcdWriteStringValue( "Longitude:", receiveData.GNSS_longitude, 10, HAL_LCD_LINE_1);
    HalLcdWriteStringValue( "Battery Level:", receiveData.batt_level, 10, HAL_LCD_LINE_2);
    HalLcdWriteString( receiveData.text, HAL_LCD_LINE_3);
  }
  
  if ( keys & HAL_KEY_SW_4 )
  {
 //   HalLcdWriteStringValue( "SensorStat:", receiveData.sensors_okay, 10, HAL_LCD_LINE_1);
 //   HalLcdWriteStringValue( "NodeStat:", receiveData.node_okay, 10, HAL_LCD_LINE_2);
      
      //sprintf(serialOut,"temperature:%d\n",receiveData.temperature);
        int serialSize;
        /*
        sprintf(serialBuffer,
   ",temperature:%d:\n,water_level:%d:\n,flow_rate:%d:\n,PH:%d:\n,batt_level:%d:\n,GNSS_latitude:%d:\n,GNSS_longitude:%d:\n,sensors_okay:%d:\n,error_state:%d:\n,node_okay:1:\n",
                  receiveData.temperature, //1
                  receiveData.water_level, //2
                  receiveData.flow_rate, //3
                  receiveData.PH, //4
                  receiveData.batt_level, //5
                  receiveData.GNSS_latitude, //6
                  receiveData.GNSS_longitude, //7
                  receiveData.sensors_okay, //8
                  receiveData.error_state); //9
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize); 
        */

        sprintf(serialBuffer,",temperature:%d:\n",receiveData.temperature);
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);
 
        sprintf(serialBuffer,",water_level:%d:\n",receiveData.water_level);
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);
        
        sprintf(serialBuffer,",flow_rate:%d:\n",receiveData.flow_rate);
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);

 
        /*
        sprintf(serialBuffer,",PH:%d:\n",receiveData.PH);
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);
       
        // HalLedBlink(HAL_LED_2,4,50,1000);
        
        sprintf(serialBuffer,",batt_level:%d:\n",receiveData.batt_level);
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);
 
        sprintf(serialBuffer,",GNSS_latitude:%d:\n",receiveData.GNSS_latitude);
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);
 
        sprintf(serialBuffer,",GNSS_longitude:%d:\n",receiveData.GNSS_longitude);
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize); 
        
*/
        
        /*
        
        sprintf(serialBuffer,",sensors_okay:%d:\n",receiveData.sensors_okay);
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);
 
        sprintf(serialBuffer,",error_state:%d:\n",receiveData.error_state);
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);
        
        strcpy(serialBuffer,",node_okay:1:\n");
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);
        
        */
        
        /*
        sprintf(serialBuffer,",sensors_okay:%d,",receiveData.sensors_okay);
        strcat(serialOut,serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialOut, serialSize);
        */
        /*
        sprintf(serialBuffer,",error_state:%d,",receiveData.error_state);
        strcat(serialOut,serialBuffer2);
        HalUARTWrite(SAMPLE_APP_PORT, serialOut, serialSize);
        
        strcat(serialOut,",node_okay:1:\n,");
        HalUARTWrite(SAMPLE_APP_PORT, serialOut, serialSize);
        
        serialSize = sizeof(serialOut);
        HalUARTWrite(SAMPLE_APP_PORT, serialOut, serialSize);
*/
        
        
        strcpy(serialBuffer,",node_okay:1:\n");
        serialSize = sizeof(serialBuffer);
        HalUARTWrite(SAMPLE_APP_PORT, serialBuffer, serialSize);
        
  }
}

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      SampleApp_MessageMSGCB
 *
 * @brief   Data message processor callback.  This function processes
 *          any incoming data - probably from other devices.  So, based
 *          on cluster ID, perform the intended action.
 *
 * @param   none
 *
 * @return  none
  */
/*
typedef struct
{
  int16 temperature;
  int16 water_level;
  int32 flow_rate;
  uint8 PH;
  uint8 batt_level;
  int16 GNSS_latitude;
  int16 GNSS_longitude;
  
  bool sensors_okay;
  bool node_okay;
  uint8 error_state;
} packet_measurements;

packet_measurements receiveData;
*/
void SampleApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
  int dataSize; //Data size of received packet
  
  switch ( pkt->clusterId )
  {
    case SAMPLEAPP_PERIODIC_CLUSTERID:
      break;
      
    case SAMPLEAPP_CLUSTERID1:
      dataSize = sizeof(packet_measurements);
      HalLcdWriteStringValue("Data Size:", dataSize, 10,HAL_LCD_LINE_1);
      memcpy(&receiveData,pkt->cmd.Data,dataSize);
      //HalLcdWriteStringValue("Data:", receiveData.testData2, 10, HAL_LCD_LINE_2);
      //HalLcdWriteStringValue("Byte: ", pkt->cmd.Data[4], 10,HAL_LCD_LINE_3);
      
      //char ptr[5] = {pkt->cmd.Data[4], pkt->cmd.Data[5], pkt->cmd.Data[6], pkt->cmd.Data[7], pkt->cmd.Data[8]};
      
      /*
      HalLcdWriteString((char*)pkt->cmd.Data[4],HAL_LCD_LINE_1);
 
      HalLcdWriteString((char*)pkt->cmd.Data[5],HAL_LCD_LINE_2);
      
      HalLcdWriteString((char*)pkt->cmd.Data[7],HAL_LCD_LINE_3);
      */ 
      
      HalLcdWriteStringValue("Byte: ", pkt->cmd.DataLength, 10,HAL_LCD_LINE_3);
      
      break;

    case SAMPLEAPP_FLASH_CLUSTERID:
      break;
  }
}

/*********************************************************************
 * @fn      SampleApp_SendPeriodicMessage
 *
 * @brief   Send the periodic message.
 *
 * @param   none
 *
 * @return  none
 */
void SampleApp_SendPeriodicMessage( void )
{
  if ( AF_DataRequest( &SampleApp_Periodic_DstAddr, &SampleApp_epDesc,
                       SAMPLEAPP_PERIODIC_CLUSTERID,
                       1,
                       (uint8*)&SampleAppPeriodicCounter,
                       &SampleApp_TransID,
                       AF_DISCV_ROUTE,
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
}

/*********************************************************************
 * @fn      SampleApp_SendFlashMessage
 *
 * @brief   Send the flash message to group 1.
 *
 * @param   flashTime - in milliseconds
 *
 * @return  none
 */
void SampleApp_SendFlashMessage( uint16 flashTime )
{
  uint8 buffer[3];
  buffer[0] = (uint8)(SampleAppFlashCounter++);
  buffer[1] = LO_UINT16( flashTime );
  buffer[2] = HI_UINT16( flashTime );

  if ( AF_DataRequest( &SampleApp_Flash_DstAddr, &SampleApp_epDesc,
                       SAMPLEAPP_FLASH_CLUSTERID,
                       3,
                       buffer,
                       &SampleApp_TransID,
                       AF_DISCV_ROUTE,
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
}

/*********************************************************************
*********************************************************************/

/*********************************************************************
 * @fn      SampleApp_CallBack
 *
 * @brief   Send data OTA.
 *
 * @param   port - UART port.
 * @param   event - the UART port event flag.
 *
 * @return  none
 */

static void SampleApp_CallBack(uint8 port, uint8 event)
{
  (void)port;
  uint8 localBuf[81];
  uint16 receivedUARTLen;

  if ((event & (HAL_UART_RX_FULL | HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_TIMEOUT)) &&
#if SERIAL_APP_LOOPBACK
      (SampleApp_TxLen < SERIAL_APP_TX_MAX))
#else
      !SampleApp_TxLen)
#endif
  {
    // buffer needed to be emptied for the new liner
    for(int i = 0; i < 81; i++){
      localBuf[i] = 0;
    }   
    receivedUARTLen= HalUARTRead(SAMPLE_APP_PORT, localBuf, SAMPLE_APP_TX_MAX);
    HalLcdWriteStringValue( localBuf,  receivedUARTLen, 16, HAL_LCD_LINE_3 ); 
    
    
    //HalUARTWrite(SAMPLE_APP_PORT, *localBuf, 81);
    //SampleApp_Send();
  }
}


/*********************************************************************
*********************************************************************/
