//*****************************************************************************
//
// Application Name     -   Asteroids Game
// Application Overview -   Dodge asteroids for as long as possible! Get more points as
//                          asteroids pass through the screen. Control spaceship with the
//                          board's accelerometer. Navigate menu and enter text using an
//                          infrared TV remote. Save and load game data to and from an AWS
//                          "Thing" shadow to keep data persistent after program shutdown.
// Author               -   Nima Bayati
//
//*****************************************************************************

//*****************************************************************************
//
//! \addtogroup lab6
//! @{
//
//*****************************************************************************

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

// Simplelink includes
#include "simplelink.h"

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "hw_nvic.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "interrupt.h"
#include "systick.h"
#include "gpio.h"
#include "timer.h"

//Common interface includes
#include "pinmux.h"
#include "uart_if.h"
#include "gpio_if.h"
#include "timer_if.h"
#include "i2c_if.h"
#include "common.h"
#include "Adafruit_SSD1351.h"
#include "Adafruit_GFX.h"

#define SPI_IF_BIT_RATE  10000000
#define TR_BUFF_SIZE     100

#define MAX_URI_SIZE 128
#define URI_SIZE MAX_URI_SIZE + 1


#define APPLICATION_NAME        "LAB6"
#define APPLICATION_VERSION     "1.1.1.EEC.Spring2023"
#define SERVER_NAME             "ab1yvxurt3f74-ats.iot.us-east-1.amazonaws.com"
#define GOOGLE_DST_PORT         8443

#define SL_SSL_CA_CERT "/cert/rootca.der"
#define SL_SSL_PRIVATE "/cert/private.der"
#define SL_SSL_CLIENT  "/cert/client.der"

//NEED TO UPDATE THIS FOR IT TO WORK!
#define DAY                 7       /* Current Day */
#define MONTH               6       /* Month 1-12 */
#define YEAR                2023    /* Current year */
#define HOUR                0       /* Time - hours */
#define MINUTE              0       /* Time - minutes */
#define SECOND              0       /* Time - seconds */

#define GETHEADER   "GET /things/CC3200_Thing/shadow HTTP/1.1\n\r"
#define POSTHEADER  "POST /things/CC3200_Thing/shadow HTTP/1.1\n\r"
#define HOSTHEADER  "Host: nbayati.iot.us-east-1.amazonaws.com\r\n"
#define CHEADER     "Connection: Keep-Alive\r\n"
#define CTHEADER    "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1   "Content-Length: "
#define CLHEADER2   "\r\n\r\n"

#define DATAPREFIX "{\"state\": {\n\r\"desired\": {\n\r\""
// Insert data identifier here
#define DATAMID "\": {\n\r\"message\": \""
// Insert data here
#define DATASUFFIX "\"\n\r}}}}\n\r\n\r"

// Application specific status/error codes
typedef enum{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

typedef struct
{
   /* time */
   unsigned long tm_sec;
   unsigned long tm_min;
   unsigned long tm_hour;
   /* date */
   unsigned long tm_day;
   unsigned long tm_mon;
   unsigned long tm_year;
   unsigned long tm_week_day; //not required
   unsigned long tm_year_day; //not required
   unsigned long reserved[3];
}SlDateTime;


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
volatile unsigned long  g_ulStatus = 0;//SimpleLink Status
unsigned long  g_ulPingPacketsRecv = 0; //Number of Ping Packets received
unsigned long  g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
signed char    *g_Host = SERVER_NAME;
SlDateTime g_time;
#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

// some helpful macros for systick

// the cc3200's fixed clock frequency of 80 MHz
// note the use of ULL to indicate an unsigned long long constant
#define SYSCLKFREQ 80000000ULL

// macro to convert ticks to microseconds
#define TICKS_TO_US(ticks) \
    ((((ticks) / SYSCLKFREQ) * 1000000ULL) + \
    ((((ticks) % SYSCLKFREQ) * 1000000ULL) / SYSCLKFREQ))\

// macro to convert microseconds to ticks
#define US_TO_TICKS(us) ((SYSCLKFREQ / 1000000ULL) * (us))

// systick reload value set to 200ms period //40ms period
// (PERIOD_SEC) * (SYSCLKFREQ) = PERIOD_TICKS
#define SYSTICK_RELOAD_VAL 16000000UL //3200000UL

// track systick counter periods elapsed
// if it is not 0, we know the transmission ended
volatile int systick_cnt = 0;

// Convert ordinary 24 bit rgb color code to 16 bit code for the Adafruit display
uint16_t color24to16(uint8_t red, uint8_t green, uint8_t blue) {
  return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
}

// Color definitions
#define BLACK           0x0000
#define BLUE            0x001F
#define GREEN           0x07E0
#define CYAN            0x07FF
#define RED             0xF800
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF
#define GRAY            color24to16(0xA1, 0xA1, 0xA1)

// Button definitions
#define ZERO            "00000100101110110001100011100111"
#define ONE             "00000100101110111000000001111111"
#define TWO             "00000100101110110100000010111111"
#define THREE           "00000100101110111100000000111111"
#define FOUR            "00000100101110111010000001011111"
#define FIVE            "00000100101110110110000010011111"
#define SIX             "00000100101110111110000000011111"
#define SEVEN           "00000100101110111001000001101111"
#define EIGHT           "00000100101110110101000010101111"
#define NINE            "00000100101110111101000000101111"
#define ENTER           "00000100101110111111000000001111"
// delete button is disabled for given tv code (1202)
#define LAST            "00000100101110111001001001101101"
#define MUTE            "00000100101110110010001011011101"
#define LEFT            "00000100101110110100100010110111"
#define UP              "00000100101110110000100011110111"
#define RIGHT           "00000100101110111000100001110111"
#define DOWN            "00000100101110111100100000110111"
#define OK_BUTTON       "00000100101110111111000000001111"
#define INFO            "00000100101110111010001001011101"

volatile unsigned long IR_intcount;

static volatile unsigned long g_ulSysTickValue;
static volatile unsigned long g_ulBase;
static volatile unsigned long g_ulMultitapBase;
static volatile unsigned long g_ulIntClearVector;

#define TEXTSIZE 1
#define BUFFERSIZE  12  // limit message length to prevent going out of bounds on OLED display

int x;
int y;
int PrevButton;
int ButtonPressed;
char DisplayBuffer[BUFFERSIZE];
int BufferPos;
int text_x = 0;
int text_y = 0;

// Game states: Menu, LoadSaveEntry, SaveStateEntry, Instructions, GameMission};
int game_state = 0;

struct Player {
    int pos_x;
    int pos_y;

    int pri_color;
    int sec_color;
    int shape;

    int health;
    int cur_health;
    int firing_spd;
    int num_bullets;
    int bullet_strength;

    int money;
    int high_score;
};
struct Player p1;

struct Asteroid {
    float pos_x;
    float pos_y;
    float speed;

    int color;
    int crack_color;
    int radius;

    int health; // start at 2
    int cur_health;
    int value; // start at 1
};
struct Asteroid asteroids[100];

//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************


//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
static long WlanConnect();
static int set_time();
static void BoardInit(void);
static long InitializeAppVariables();
static int tls_connect();
static int connectToAccessPoint();
static int http_post(int, char*);
static int http_get(int, char*);

//  function delays 3*ulCount cycles
void delay(unsigned long ulCount) {
    int i;

  do{
    ulCount--;
        for (i=0; i< 65535; i++);
    } while(ulCount);
}

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************


//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent) {
    if(!pWlanEvent) {
        return;
    }

    switch(pWlanEvent->Event) {
        case SL_WLAN_CONNECT_EVENT: {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'slWlanConnectAsyncResponse_t'.
            // Applications can use it if required
            //
            //  slWlanConnectAsyncResponse_t *pEventData = NULL;
            // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
            //

            // Copy new connection SSID and BSSID to global parameters
            memcpy(g_ucConnectionSSID,pWlanEvent->EventData.
                   STAandP2PModeWlanConnected.ssid_name,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
            memcpy(g_ucConnectionBSSID,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
                   SL_BSSID_LENGTH);

            UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s , "
                       "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                       g_ucConnectionSSID,g_ucConnectionBSSID[0],
                       g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                       g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                       g_ucConnectionBSSID[5]);
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT: {
            slWlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            // If the user has initiated 'Disconnect' request,
            //'reason_code' is SL_USER_INITIATED_DISCONNECTION
            if(SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code) {
                UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s,"
                    "BSSID: %x:%x:%x:%x:%x:%x on application's request \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            else {
                UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s, "
                           "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
            memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
        }
        break;

        default: {
            UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                       pWlanEvent->Event);
        }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent) {
    if(!pNetAppEvent) {
        return;
    }

    switch(pNetAppEvent->Event) {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT: {
            SlIpV4AcquiredAsync_t *pEventData = NULL;

            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            //Ip Acquired Event Data
            pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

            //Gateway IP address
            g_ulGatewayIP = pEventData->gateway;

            UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                       "Gateway=%d.%d.%d.%d\n\r",
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,0),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,0));
        }
        break;

        default: {
            UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                       pNetAppEvent->Event);
        }
        break;
    }
}


//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent, SlHttpServerResponse_t *pHttpResponse) {
    // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent) {
    if(!pDevEvent) {
        return;
    }

    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status,
               pDevEvent->EventData.deviceEvent.sender);
}


//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock) {
    if(!pSock) {
        return;
    }

    switch( pSock->Event ) {
        case SL_SOCKET_TX_FAILED_EVENT:
            switch( pSock->socketAsyncEvent.SockTxFailData.status) {
                case SL_ECLOSE: 
                    UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                                "failed to transmit all queued packets\n\n", 
                                    pSock->socketAsyncEvent.SockTxFailData.sd);
                    break;
                default: 
                    UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                                "(%d) \n\n",
                                pSock->socketAsyncEvent.SockTxFailData.sd, pSock->socketAsyncEvent.SockTxFailData.status);
                  break;
            }
            break;

        default:
            UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
          break;
    }
}


//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End
//*****************************************************************************


//*****************************************************************************
//
//! \brief This function initializes the application variables
//!
//! \param    0 on success else error code
//!
//! \return None
//!
//*****************************************************************************
static long InitializeAppVariables() {
    g_ulStatus = 0;
    g_ulGatewayIP = 0;
    g_Host = SERVER_NAME;
    memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
    return SUCCESS;
}


//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState() {
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);

    // If the device is not in station-mode, try configuring it in station-mode 
    if (ROLE_STA != lMode) {
        if (ROLE_AP == lMode) {
            // If the device is in AP mode, we need to wait for this event 
            // before doing anything 
            while(!IS_IP_ACQUIRED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask(); 
#endif
            }
        }

        // Switch to STA role and restart 
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(0xFF);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Check if the device is in station again 
        if (ROLE_STA != lRetVal) {
            // We don't want to proceed if the device is not coming up in STA-mode 
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }
    
    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt, 
                                &ucConfigLen, (unsigned char *)(&ver));
    ASSERT_ON_ERROR(lRetVal);
    
    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
    ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
    ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
    ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
    ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
    ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig 
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, 
                                SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(lRetVal);

    

    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore 
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal) {
        // Wait
        while(IS_CONNECTED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask(); 
#endif
        }
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, 
            WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);

    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(lRetVal);

    InitializeAppVariables();
    
    return lRetVal; // Success
}


//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void BoardInit(void) {
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}


//****************************************************************************
//
//! \brief Connecting to a WLAN Accesspoint
//!
//!  This function connects to the required AP (SSID_NAME) with Security
//!  parameters specified in te form of macros at the top of this file
//!
//! \param  None
//!
//! \return  0 on success else error code
//!
//! \warning    If the WLAN connection fails or we don't aquire an IP
//!            address, It will be stuck in this function forever.
//
//****************************************************************************
static long WlanConnect() {
    SlSecParams_t secParams = {0};
    long lRetVal = 0;

    secParams.Key = SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;

    UART_PRINT("Attempting connection to access point: ");
    UART_PRINT(SSID_NAME);
    UART_PRINT("... ...");
    lRetVal = sl_WlanConnect(SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT(" Connected!!!\n\r");


    // Wait for WLAN Event
    while((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus))) {
        // Connection in Progress
        _SlNonOsMainLoopTask();
        delay(100);
        _SlNonOsMainLoopTask();
        delay(100);
    }

    return SUCCESS;

}

//*****************************************************************************
//
//! This function updates the date and time of CC3200.
//!
//! \param None
//!
//! \return
//!     0 for success, negative otherwise
//!
//*****************************************************************************

static int set_time() {
    long retVal;

    g_time.tm_day = DAY;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = HOUR;
    g_time.tm_hour = MINUTE;
    g_time.tm_min = SECOND;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                          SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                          sizeof(SlDateTime),(unsigned char *)(&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}

long printErrConvenience(char * msg, long retVal) {
    UART_PRINT(msg);
    //GPIO_IF_LedOn(MCU_RED_LED_GPIO);
    return retVal;
}

//*****************************************************************************
//
//! This function demonstrates how certificate can be used with SSL.
//! The procedure includes the following steps:
//! 1) connect to an open AP
//! 2) get the server name via a DNS request
//! 3) define all socket options and point to the CA certificate
//! 4) connect to the server via TCP
//!
//! \param None
//!
//! \return  0 on success else error code
//! \return  LED1 is turned solid in case of success
//!    LED2 is turned solid in case of failure
//!
//*****************************************************************************
static int tls_connect() {
    SlSockAddrIn_t    Addr;
    int    iAddrSize;
    unsigned char    ucMethod = SL_SO_SEC_METHOD_TLSV1_2;
    unsigned int uiIP,uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
    long lRetVal = -1;
    int iSockID;

    lRetVal = sl_NetAppDnsGetHostByName(g_Host, strlen((const char *)g_Host),
                                    (unsigned long*)&uiIP, SL_AF_INET);

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't retrieve the host name \n\r", lRetVal);
    }

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(GOOGLE_DST_PORT);
    Addr.sin_addr.s_addr = sl_Htonl(uiIP);
    iAddrSize = sizeof(SlSockAddrIn_t);
    //
    // opens a secure socket 
    //
    iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, SL_SEC_SOCKET);
    if( iSockID < 0 ) {
        return printErrConvenience("Device unable to create secure socket \n\r", lRetVal);
    }

    //
    // configure the socket as TLS1.2
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECMETHOD, &ucMethod,\
                               sizeof(ucMethod));
    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }
    //
    //configure the socket as ECDHE RSA WITH AES256 CBC SHA
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &uiCipher,\
                           sizeof(uiCipher));
    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //
    //configure the socket with CA certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
                           SL_SO_SECURE_FILES_CA_FILE_NAME, \
                           SL_SSL_CA_CERT, \
                           strlen(SL_SSL_CA_CERT));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //configure the socket with Client Certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
                SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME, \
                                    SL_SSL_CLIENT, \
                           strlen(SL_SSL_CLIENT));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //configure the socket with Private Key - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
            SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME, \
            SL_SSL_PRIVATE, \
                           strlen(SL_SSL_PRIVATE));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }


    /* connect to the peer device - Google server */
    lRetVal = sl_Connect(iSockID, ( SlSockAddr_t *)&Addr, iAddrSize);

    if(lRetVal < 0) {
        UART_PRINT("Device couldn't connect to server:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
        return printErrConvenience("Device couldn't connect to server \n\r", lRetVal);
    }
    else {
        UART_PRINT("Device has connected to the website:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
    }

    //GPIO_IF_LedOff(MCU_RED_LED_GPIO);
    //GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);
    return iSockID;
}

int connectToAccessPoint() {
    long lRetVal = -1;

    lRetVal = InitializeAppVariables();
    ASSERT_ON_ERROR(lRetVal);

    //
    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its default state at start of applicaton
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    lRetVal = ConfigureSimpleLinkToDefaultState();
    if(lRetVal < 0) {
      if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
          UART_PRINT("Failed to configure the device in its default state \n\r");

      return lRetVal;
    }

    UART_PRINT("Device is configured in default state \n\r");

    CLR_STATUS_BIT_ALL(g_ulStatus);

    ///
    // Assumption is that the device is configured in station mode already
    // and it is in its default state
    //
    lRetVal = sl_Start(0, 0, 0);
    if (lRetVal < 0 || ROLE_STA != lRetVal) {
        UART_PRINT("Failed to start the device \n\r");
        return lRetVal;
    }

    UART_PRINT("Device started as STATION \n\r");

    //
    //Connecting to WLAN AP
    //
    lRetVal = WlanConnect();
    if(lRetVal < 0) {
        UART_PRINT("Failed to establish connection w/ an AP \n\r");
        return lRetVal;
    }

    UART_PRINT("Connection established w/ AP and IP is aquired \n\r");
    return 0;
}

void SPIInit()
{
    // Reset SPI
    MAP_SPIReset(GSPI_BASE);

    // Configure SPI interface
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVEHIGH |
                     SPI_WL_8));

    // Enable SPI for communication
    MAP_SPIEnable(GSPI_BASE);
}

/**
 * Reset SysTick Counter
 */
static inline void SysTickReset(void) {
    // any write to the ST_CURRENT register clears it
    // after clearing it automatically gets reset without
    // triggering exception logic
    // see reference manual section 3.2.1
    HWREG(NVIC_ST_CURRENT) = 1;

    // clear the global count variable
    systick_cnt = 0;
}

/**
 * SysTick Interrupt Handler
 *
 * Keep track of whether the systick counter wrapped
 */
static void SysTickHandler(void) {
    // increment every time the systick handler fires
    systick_cnt++;
}

/**
 * Initializes SysTick Module
 */
static void SysTickInit(void) {

    // configure the reset value for the systick countdown register
    MAP_SysTickPeriodSet(SYSTICK_RELOAD_VAL);

    // register interrupts on the systick module
    MAP_SysTickIntRegister(SysTickHandler);

    // enable interrupts on systick
    // (trigger SysTickHandler when countdown reaches 0)
    MAP_SysTickIntEnable();

    // enable the systick module itself
    MAP_SysTickEnable();
}

// Interrupt handler for GPIOA0
uint64_t pulse_times[50];
char waveform[33];
int pulses = 0;
static void GPIOA0IntHandler(void) {
    unsigned long ulStatus;

    ulStatus = GPIOIntStatus (GPIOA0_BASE, true);
    MAP_GPIOIntClear(GPIOA0_BASE, ulStatus);        // clear interrupts on GPIOA0

    // read the countdown register and compute elapsed cycles
    uint64_t delta = SYSTICK_RELOAD_VAL - SysTickValueGet();

    // convert elapsed cycles to microseconds
    uint64_t delta_us = TICKS_TO_US(delta);

    // store the time in us for each pulse
    pulse_times[IR_intcount] = delta_us;

    int i = IR_intcount;
    uint64_t dif;
    if (i > 0 && pulses < 32) {
        if (pulse_times[i]<pulse_times[i-1]) {
            dif = (200000 - pulse_times[i-1]) + pulse_times[i];
        } else {
            dif = pulse_times[i]-pulse_times[i-1];
        }

        if (dif >= 1000 && dif < 1300) {
            // set rightmost bit as 0
            waveform[pulses++] = '0';
        } else if (dif >= 2100 && dif < 2400) {
            // set rightmost bit as 1
            waveform[pulses++] = '1';
        } else {
            if (i > 1) {
                // waveform has broken, begin decoding new waveform
                pulses = 0;
                IR_intcount = 0;
            }
        }
    }

    IR_intcount++;
}

// Interrupt handler for multi-tap timer
int started = false;
void TimerMultitapIntHandler(void) {
    // Clear the timer interrupt.
    Timer_IF_InterruptClear(g_ulMultitapBase);

    if (started == true) {
        // Stop the timer
        MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
        started = false;

        // Finalize currently queued character
        drawChar(text_x+BufferPos*6, text_y, DisplayBuffer[BufferPos], WHITE, BLACK, TEXTSIZE);
        BufferPos++;
        PrevButton = -1;
        ButtonPressed = -1;
    }
}

void InterruptsInit() {
    unsigned long ulStatus;

    // Enable SysTick
    SysTickInit();
    // reset the countdown register
    SysTickReset();

    // Register the interrupt handlers
    GPIOIntRegister(GPIOA0_BASE, GPIOA0IntHandler);
    // Configure rising edge interrupts
    GPIOIntTypeSet(GPIOA0_BASE, 0x1, GPIO_RISING_EDGE);    // pin 50
    ulStatus = GPIOIntStatus(GPIOA0_BASE, false);
    GPIOIntClear(GPIOA0_BASE, ulStatus);            // clear interrupts on GPIOA0
    // clear global variables
    IR_intcount=0;
    // Enable interrupts
    GPIOIntEnable(GPIOA0_BASE, 0x1);

    // Base address for timers
    g_ulMultitapBase = TIMERA1_BASE;

    // Configuring the timers
    Timer_IF_Init(PRCM_TIMERA1, g_ulMultitapBase, TIMER_CFG_PERIODIC, TIMER_A, 0);

    // Setup the interrupts for the timer timeouts.
    Timer_IF_IntSetup(g_ulMultitapBase, TIMER_A, TimerMultitapIntHandler);

    // Load timer values
    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));  // 1.1 seconds (multi-tap)
}

// Draw a full string to display starting from given x and y coords with specified color and size
void draw_string(char* str, int str_len, int x, int y, int color, int bg_color, int text_size) {
    int x_start = x;
    int i;
    for (i = 0; i < str_len; i++) {
        drawChar(x, y, str[i], color, bg_color, text_size);
        x += 6*text_size; // each character is 5x7 at text_size=1
        if (x > 124) {
            x = x_start;
            y += 8*text_size;
        }
    }
}

// Draw menu screen
int selected_button = 0;
void menu_screen() {
    fillScreen(BLACK); // clear screen

    // Display title
    draw_string("-Asteroids-", strlen("-Asteroids-"), 0, 10, CYAN, BLACK, 2);

    //
    // Draw buttons with selected one being slightly larger and colored
    //
    int highlight_color = BLUE; // was 0x0003

    // play button
    if (selected_button == 0) {
        drawRect(20, 31, 88, 21, WHITE);
        fillRect(21, 32, 86, 19, highlight_color);
        draw_string("Play", strlen("Play"), 52, 38, WHITE, highlight_color, 1);
    } else {
        drawRect(22, 33, 84, 17, WHITE);
        draw_string("Play", strlen("Play"), 52, 38, WHITE, BLACK, 1);
    }

    // load save button
    if (selected_button == 1) {
        drawRect(20, 56, 88, 21, WHITE);
        fillRect(21, 57, 86, 19, highlight_color);
        draw_string("Load Save", strlen("Load Save"), 38, 63, WHITE, highlight_color, 1);
    } else {
        drawRect(22, 58, 84, 17, WHITE);
        draw_string("Load Save", strlen("Load Save"), 38, 63, WHITE, BLACK, 1);
    }

    // save state button
    if (selected_button == 2) {
        drawRect(20, 81, 88, 21, WHITE);
        fillRect(21, 82, 86, 19, highlight_color);
        draw_string("Save State", strlen("Save State"), 35, 88, WHITE, highlight_color, 1);
    } else {
        drawRect(22, 83, 84, 17, WHITE);
        draw_string("Save State", strlen("Save State"), 35, 88, WHITE, BLACK, 1);
    }

    // instructions button
    if (selected_button == 3) {
        drawRect(20, 106, 88, 21, WHITE);
        fillRect(21, 107, 86, 19, highlight_color);
        draw_string("Instructions", strlen("Instructions"), 28, 113, WHITE, highlight_color, 1);
    } else {
        drawRect(22, 108, 84, 17, WHITE);
        draw_string("Instructions", strlen("Instructions"), 28, 113, WHITE, BLACK, 1);
    }

}

// Save text entry
void save_text_entry(char* label, char* info) {
    fillScreen(BLACK); // clear screen
    draw_string(label, strlen(label), 0, 30, WHITE, BLACK, 2);
    drawRect(19, 60, 90, 20, WHITE);
    draw_string(info, strlen(info), 0, 90, WHITE, BLACK, 1);

    text_x = 28;
    text_y = 66;
    draw_string(DisplayBuffer, BufferPos, text_x, text_y, WHITE, BLACK, 1);
}

// Draw player
void draw_player(int x, int y, int shape, int pri_color, int sec_color) {
    if (shape == 0) {
        drawFastVLine(x-3, y-2, 3, pri_color);
        drawPixel(x-3, y+3, pri_color);
        drawPixel(x-2, y-3, pri_color);
        drawFastVLine(x-2, y+1, 3, pri_color);
        drawFastVLine(x-1, y-1, 6, pri_color);
        drawFastVLine(x, y-3, 9, pri_color);
        drawFastVLine(x+1, y-1, 6, pri_color);
        drawFastVLine(x+2, y+1, 3, pri_color);
        drawPixel(x+2, y-3, pri_color);
        drawPixel(x+3, y+3, pri_color);
        drawFastVLine(x+3, y-2, 3, pri_color);
    } else if (shape == 1) {

    }
}

void draw_health_bar() {
    int health_color;
    if (p1.cur_health >= 4) {
        health_color = GREEN;
    } else if (p1.cur_health >= 2) {
        health_color = YELLOW;
    } else {
        health_color = RED;
    }

    fillRect(0, 120, 40, 7, BLACK);

    int i;
    for (i=0; i<p1.cur_health; i++) {
        fillRect(1+5*i, 120, 4, 6, health_color);
    }
}

int score;
void draw_score() {
    char str[5];
    sprintf(str, "%d", score);
    draw_string(str, strlen(str), 100, 120, WHITE, BLACK, 1);
}

// Spawn n number of asteroids from top of screen with random position and speed
void spawn_asteroids(int start_index, int n) {
    int i;
    for (i=start_index; i<(start_index+n); i++) {
        asteroids[i].pos_y = asteroids[i].radius;
        asteroids[i].pos_x = (float)(rand() % 120) + 5;
        asteroids[i].speed = ((((float)(rand() % 100))+1) / 50) + 0.5;
    }
}

void update_asteroids(int n) {
    int i;
    for (i=0; i<n; i++) {
        if (asteroids[i].pos_y < 0) { // don't draw asteroid if it's above the visible screen
            // update position
            asteroids[i].pos_y += asteroids[i].speed;
        } else {
            // remove previous position
            fillCircle((int)asteroids[i].pos_x, (int)asteroids[i].pos_y, asteroids[i].radius, BLACK);

            // update position
            asteroids[i].pos_y += asteroids[i].speed;

            // draw at new position
            fillCircle((int)asteroids[i].pos_x, (int)asteroids[i].pos_y, asteroids[i].radius, asteroids[i].color);
        }

        // Remove asteroid if it's fallen to bottom of screen and update score
        if (asteroids[i].pos_y >= 109) {
            // remove asteroid
            fillCircle((int)asteroids[i].pos_x, (int)asteroids[i].pos_y, asteroids[i].radius, BLACK);

            score++; // increment score
            draw_score();

            // re-spawn the asteroid on top with new values
            spawn_asteroids(i, 1);
        }
    }
}

// Game loop
unsigned char aucRdDataBuf[256]; // buffer for I2C data
void start_game() {
    // draw UI elements
    score = 0;
    draw_score();
    p1.cur_health = p1.health;
    draw_health_bar();
    drawFastHLine(0, 116, 128, WHITE);

    int asteroid_count = 5;
    spawn_asteroids(0, asteroid_count);

    int count = 0;
    int game_over = 0;
    while (1) {
        // When a full 32 bit signal is read
        if (pulses >= 32) {

            // begin decoding new waveform
            pulses = 0;
            IR_intcount = 0;
        }

        // Move player character using the board's accelerometer
        if (count >= 50000 && game_over == 0) {
            float x_spd, y_spd;
            // read x acceleration
            unsigned char regOffsetX = '\x03';
            I2C_IF_Write('\x18',&regOffsetX,1,0);
            I2C_IF_Read('\x18', &aucRdDataBuf[0], '\x01');
            x_spd = (float)((signed char)aucRdDataBuf[0]);

            // read y acceleration
            unsigned char regOffsetY = '\x05';
            I2C_IF_Write('\x18',&regOffsetY,1,0);
            I2C_IF_Read('\x18', &aucRdDataBuf[0], '\x01');
            y_spd = (float)((signed char)aucRdDataBuf[0]);

            draw_player((int)p1.pos_x, (int)p1.pos_y, p1.shape, BLACK, BLACK); // remove previous player position

            // update ball's position
            p1.pos_x += x_spd / 20;
            p1.pos_y += (-y_spd / 20)+0.5;

            // check for out-of-bounds
            int radius = 4;
            if(p1.pos_x >= (127 - radius))
                p1.pos_x = 127 - radius;
            else if(p1.pos_x <= radius)
                p1.pos_x = radius;
            if(p1.pos_y >= (115 - radius))
                p1.pos_y = 115 - radius;
            else if(p1.pos_y <= radius)
                p1.pos_y = radius;

            draw_player((int)p1.pos_x, (int)p1.pos_y, p1.shape, p1.pri_color, p1.sec_color); // draw player at new position

            // Update asteroids
            update_asteroids(asteroid_count);

            // Detect collisions and update health
            int i;
            for (i=0; i<asteroid_count; i++) {
                if ((abs(asteroids[i].pos_x - p1.pos_x) < 4) && (abs(asteroids[i].pos_y - p1.pos_y) < 4)) {
                    // asteroid gets destroyed from the collsion; re-spawn it
                    fillCircle((int)asteroids[i].pos_x, (int)asteroids[i].pos_y, asteroids[i].radius, BLACK);
                    spawn_asteroids(i, 1);

                    p1.cur_health--;
                    draw_health_bar();
                }
            }

            // Check for game over
            if (p1.cur_health <= 0) {
                game_over = 1;
                draw_string("Game Over!", strlen("Game Over!"), 0, 64, RED, BLACK, 2);
                // update player's high score
                if (score > p1.high_score) {
                    p1.high_score = score;
                }

                delay(400);
                // Go back to menu
                menu_screen();
                game_state = 0;
                break;
            }

            count = 0;
        } else {
            count++;
        }
    }
}

// Set player values
struct Player InitPlayer(int pos_x, int pos_y, int pri_color, int sec_color, int shape, int health, int cur_health, int firing_spd, int num_bullets, int bullet_strength, int money, int high_score) {
    struct Player p = {pos_x, pos_y, pri_color, sec_color, shape, health, cur_health, firing_spd, num_bullets, bullet_strength, money, high_score};
    return p;
}

// Create Asteroids to use
void InitAsteroids() {
    int i;
    for (i=0; i<100; i++) {
        struct Asteroid a = {0, 0, 1, GRAY, 0, 4, 2, 2, 1};
        asteroids[i] = a;
    }
}

//*****************************************************************************
//
//! Main 
//!
//! \param  none
//!
//! \return None
//!
//*****************************************************************************
void main() {
    long lRetVal = -1;

    // Initialize Board configurations
    BoardInit();

    // Muxing UART and SPI lines
    PinMuxConfig();

    // Enable the SPI module clock
    PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);
    // Reset the peripheral
    PRCMPeripheralReset(PRCM_GSPI);

    InitTerm();
    ClearTerm();

    // Initialize SPI connection
    SPIInit();

    // I2C Init
    I2C_IF_Open(I2C_MASTER_MODE_FST);

    // Initialize display
    Adafruit_Init();
    fillScreen(BLACK); // clear screen

    // Show loading bar while connecting to the wifi
    draw_string("Loading", strlen("Loading"), 20, 40, WHITE, BLACK, 2);
    drawRect(15, 60, 90, 10, WHITE); // 0%

    //Connect the CC3200 to the local access point
    lRetVal = connectToAccessPoint();
    fillRect(15, 60, 30, 10, WHITE); // 33.3%
    //Set time so that encryption can be used
    lRetVal = set_time();
    if(lRetVal < 0) {
        //UART_PRINT("Unable to set time in the device");
        LOOP_FOREVER();
    }
    fillRect(45, 60, 30, 10, WHITE); // 66.6%
    //Connect to the website with TLS encryption
    lRetVal = tls_connect();
    if(lRetVal < 0) {
        //ERR_PRINT(lRetVal);
    }
    fillRect(75, 60, 30, 10, WHITE); // 100%

    // Draw menu screen
    menu_screen();

    // Configure Interrupts
    InterruptsInit();

    // Initialize player with default values
    p1 = InitPlayer(64, 110, WHITE, 0, 0, 5, 5, 1, 1, 1, 0, 0);

    // Initialize asteroids
    InitAsteroids();

    x = 6;
    y = 112;
    PrevButton = -1;
    ButtonPressed = -1;
    BufferPos = 0;

    char recvbuff[1460];

    char chars[10][10] = {{'-', '_', 0,0,0,0,0,0,0,0}, // 0
                          {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'}, // 1
                          {'A', 'B', 'C', 0,0,0,0,0,0,0}, // 2
                          {'D', 'E', 'F', 0,0,0,0,0,0,0}, // 3
                          {'G', 'H', 'I', 0,0,0,0,0,0,0}, // 4
                          {'J', 'K', 'L', 0,0,0,0,0,0,0}, // 5
                          {'M', 'N', 'O', 0,0,0,0,0,0,0}, // 6
                          {'P', 'Q', 'R', 'S', 0,0,0,0,0,0}, // 7
                          {'T', 'U', 'V', 0,0,0,0,0,0,0}, // 8
                          {'W', 'X', 'Y', 'Z', 0,0,0,0,0,0}}; // 9

    while (1) {

        // When a full 32 bit signal is read
        if (pulses >= 32) {

            // Perform button navigation when in menu
            if (game_state == 0) {

                // Decode the key press
                if (strcmp(waveform, UP) == 0) {
                    if (selected_button > 0) {
                        selected_button--;
                        menu_screen();
                    }

                } else if (strcmp(waveform, DOWN) == 0) {
                    if (selected_button < 3) {
                        selected_button++;
                        menu_screen();
                    }

                } else if (strcmp(waveform, OK_BUTTON) == 0) {
                    switch(selected_button) {
                        case 0:
                            // play
                            fillScreen(BLACK); // clear screen
                            start_game();
                            break;
                        case 1:
                            // load save
                            save_text_entry("Load Save", "Press ENTER to submitsave name. Press MUTEto return to the menu");
                            game_state = 1;
                            break;
                        case 2:
                            // save state
                            save_text_entry("Save State", "Press ENTER to submitsave name. Press MUTEto return to the menu");
                            game_state = 2;
                            break;
                        case 3:
                            // instructions
                            game_state = 3;
                            break;
                        default:
                            continue; // do nothing
                    }
                }

            }

            // Use multi-tap text entry when typing name to save/load the current state to/from
            if (game_state == 1 || game_state == 2) {

                // Decode the key press
                if (strcmp(waveform, ZERO) == 0) {
                    if (PrevButton != 0){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[0][ButtonPressed-1];
                    if (ButtonPressed == 2) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 0;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, ONE) == 0) {
                    if (PrevButton != 1){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[1][ButtonPressed-1];
                    if (ButtonPressed == 10) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 1;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, TWO) == 0) {
                    if (PrevButton != 2){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[2][ButtonPressed-1];
                    if (ButtonPressed == 3) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 2;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, THREE) == 0) {
                    if (PrevButton != 3){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[3][ButtonPressed-1];
                    if (ButtonPressed == 3) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 3;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, FOUR) == 0) {
                    if (PrevButton != 4){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[4][ButtonPressed-1];
                    if (ButtonPressed == 3) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 4;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, FIVE) == 0) {
                    if (PrevButton != 5){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[5][ButtonPressed-1];
                    if (ButtonPressed == 3) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 5;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, SIX) == 0) {
                    if (PrevButton != 6){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[6][ButtonPressed-1];
                    if (ButtonPressed == 3) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 6;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, SEVEN) == 0) {
                    if (PrevButton != 7){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[7][ButtonPressed-1];
                    if (ButtonPressed == 4) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 7;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, EIGHT) == 0) {
                    if (PrevButton != 8){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[8][ButtonPressed-1];
                    if (ButtonPressed == 3) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 8;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, NINE) == 0) {
                    if (PrevButton != 9){
                        TimerMultitapIntHandler();
                        ButtonPressed = 1;
                    }
                    MAP_TimerDisable(g_ulMultitapBase, TIMER_A);
                    DisplayBuffer[BufferPos] = chars[9][ButtonPressed-1];
                    if (ButtonPressed == 4) {
                        ButtonPressed = 1;
                    } else {
                        ButtonPressed++;
                    }
                    PrevButton = 9;
                    started = true;
                    MAP_TimerLoadSet(g_ulMultitapBase, TIMER_A, ((SYS_CLK/1000) * (1100)));
                    MAP_TimerEnable(g_ulMultitapBase, TIMER_A);

                } else if (strcmp(waveform, ENTER) == 0 && BufferPos > 0) {
                    TimerMultitapIntHandler();

                    if (game_state == 1) {  // loading save
                        http_get(lRetVal, recvbuff);
                        char *result = strstr(recvbuff, DisplayBuffer); // find location of save data
                        if (result) {
                            char *token = strtok(result, "\"");
                            int i;
                            for (i=0; i<4; i++) {   // grab message from save data
                                token = strtok(NULL, "\"");
                            }
                            printf("High Score for '%s': %s\n", DisplayBuffer, token);

                            // print success message
                            fillScreen(BLACK); // clear screen
                            draw_string("SUCCESS", strlen("SUCCESS"), 22, 30, GREEN, BLACK, 2);
                            delay(200);

                            // Go back to menu
                            menu_screen();
                            game_state = 0;
                        } else {
                            // give an error, no data matching given save name
                            fillScreen(BLACK); // clear screen
                            draw_string("ERROR", strlen("ERROR"), 34, 30, RED, BLACK, 2);
                            char* str = "No save data is conn-ected to the given   name.";
                            draw_string(str, strlen(str), 0, 60, RED, BLACK, 1);
                            delay(400);
                            // return to save state name entry
                            save_text_entry("Save State", "Press ENTER to submitsave name. Press MUTEto return to the menu");
                        }

                    } else {                // saving current state
                        // Send message to CC3200 Thing Shadow
                        char str1[500] = DATAPREFIX;
                        strcat(str1, DisplayBuffer);
                        strcat(str1, DATAMID);
                        char str_score[5];
                        sprintf(str_score, "%d", p1.high_score);
                        strcat(str1, str_score);
                        strcat(str1, DATASUFFIX);
                        http_post(lRetVal, str1);

                        // Clear buffer
                        int i;
                        for (i=0; i<BUFFERSIZE; i++) {
                            DisplayBuffer[i] = '\0';
                        }
                        BufferPos = 0;

                        // print success message
                        fillScreen(BLACK); // clear screen
                        draw_string("SUCCESS", strlen("SUCCESS"), 22, 30, GREEN, BLACK, 2);
                        delay(200);

                        // Go back to menu
                        menu_screen();
                        game_state = 0;
                    }

                    PrevButton = 10;

                } else if (strcmp(waveform, LAST) == 0) {
                    TimerMultitapIntHandler();
                    if (BufferPos > 0) {
                        fillRect(text_x+BufferPos*6, text_y, 5, 7, BLACK);
                        BufferPos--;
                        DisplayBuffer[BufferPos] = '\0';
                    }
                    PrevButton = 11;

                } else if (strcmp(waveform, MUTE) == 0) {
                    TimerMultitapIntHandler();

                    // Clear buffer
                    int i;
                    for (i=0; i<BUFFERSIZE; i++) {
                        DisplayBuffer[i] = '\0';
                    }
                    BufferPos = 0;

                    // Go back to menu
                    menu_screen();
                    game_state = 0;
                }

                // display latest character
                if (game_state == 1 || game_state == 2) {
                    drawChar(text_x+BufferPos*6, text_y, DisplayBuffer[BufferPos], WHITE, BLACK, TEXTSIZE);
                }
            }

            // begin decoding new waveform
            pulses = 0;
            IR_intcount = 0;
        }
    }
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

static int http_post(int iTLSSockID, char* str) {
    char acSendBuff[512];
    char acRecvbuff[1460];
    char cCLLength[200];
    char* pcBufHeaders;
    int lRetVal = 0;

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, POSTHEADER);
    pcBufHeaders += strlen(POSTHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    int dataLength = strlen(str);

    strcpy(pcBufHeaders, CTHEADER);
    pcBufHeaders += strlen(CTHEADER);
    strcpy(pcBufHeaders, CLHEADER1);

    pcBufHeaders += strlen(CLHEADER1);
    sprintf(cCLLength, "%d", dataLength);

    strcpy(pcBufHeaders, cCLLength);
    pcBufHeaders += strlen(cCLLength);
    strcpy(pcBufHeaders, CLHEADER2);
    pcBufHeaders += strlen(CLHEADER2);

    strcpy(pcBufHeaders, str);
    pcBufHeaders += strlen(str);

    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);


    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
        //GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
        //GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    } else {
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
    }

    return 0;
}

static int http_get(int iTLSSockID, char *recvbuff_p) {
    char acSendBuff[512];
    char acRecvbuff[1460];
    char* pcBufHeaders;
    int lRetVal = 0;

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, GETHEADER);
    pcBufHeaders += strlen(GETHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    UART_PRINT(acSendBuff);

    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("GET failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
        //GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
        //GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    } else {
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT("\n\r\n\r");
        strcpy(recvbuff_p, acRecvbuff); // pass the receive buffer to the pointer
    }

    return 0;
}



