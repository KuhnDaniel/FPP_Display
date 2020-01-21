#include <simpleDSTadjust.h>
const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 10 minutes
const int SLEEP_INTERVAL_SECS = 0;        // Going to sleep after idle times, set 0 for insomnia
// Adjust Country
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600};
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};
#define NTP_MIN_VALID_EPOCH 1533081600
/*---TIME CODE-------------------------------------------------------------------------*/

/*---WiFi---*/
#define WIFI_SSID ""
#define WIFI_PASS ""

/*---Config File Setup Portal---*/
bool initialConfig = false;
bool readConfigFile();
bool writeConfigFile();

String configStatus = ("SYSTEMINIT");         // BOOT/CONFIG/WEBCONFIG/RUN              MUSS GEMACHT WERDEN
String ClientStatus = ("");             // Client Connect
long now1;                              //Systemweite laufZeit WICHTIG

// Adjust according to your language
const String WDAY_NAMES[] = {"SO", "MO", "DI", "MI", "DO", "FR", "SA"};
const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAI", "JUN", "JUL", "AUG", "SEP", "OKT", "NOV", "DEZ"};

#define TOPIC_Presence "xLights/FPP_Display/System/Online"  //CLIENT ANNOUNCMENT

//Messbereich muss eventuell kalibriert werden
/*____Calibrate Touchscreen_____ArduiTouch DisplayModul V.2 Yellowsockl !!!!*/
//  #define MINPRESSURE 10      // minimum required force for touch event
#define TS_MINX 370
#define TS_MINY 470
#define TS_MAXX 3700
#define TS_MAXY 3600

/*---Touch Calibration File---*/
float dx = 0;
float dy = 0;
int ax = 0;
int ay = 0;

// Pins for the ILI9341
  #define TFT_MOSI 23
  #define TFT_CLK 18
  #define TFT_MISO 19
  #define TFT_CS   5            //diplay chip select
  #define TFT_DC   4            //display d/c
  #define TFT_RST  22           //display reset
  #define TFT_LED  15           //display background LED
  #define ARDUITOUCH_VERSION 1    //0 for older 1 for new with PNP Transistor

  #define HAVE_TOUCHPAD
  #define TOUCH_CS 14
  #define TOUCH_IRQ 2
  //#define TOUCHMAP 1             //rotate the coordinates of touchscreen

/*______End of Calibration______*/





// defines the colors usable in the paletted 16 color frame buffer
#define ILI9341_ULTRA_DARKGREY 0x632C
#define MINI_BLACK 0
#define MINI_WHITE 1
#define MINI_YELLOW 2
#define MINI_BLUE 3
#define MINI_NAVY 4
#define MINI_RED 5
#define MINI_GREEN 6
#define MINI_GRAY 7

uint16_t palette[] = {ILI9341_BLACK,          // 0
                      ILI9341_WHITE,          // 1
                      ILI9341_YELLOW,         // 2
                      0x7E3C,                 // 3
                      ILI9341_NAVY,           // 4
                      ILI9341_RED,            // 5
                      ILI9341_GREEN,          // 6
                      0x8410,                 // 7
                      0x632C,                 // 8
                      ILI9341_ULTRA_DARKGREY, // 9
                      0x632C                  //10
                     };



/*--------------------------------------------------------------------------------------------------------------------*/

/*---SYSTEMBUTTONS---*/
char apl_Status_Button = 4;         //NAVY
char apl_Status_Button_Text = 1;    //WHITE
char apl_Repeat_Button = 4;         //NAVY
char apl_Repeat_Button_Text = 9;    //WHITE

boolean fppRequest = true;
boolean b_systemstatus = false;
boolean b_playlist = false;
boolean b_play = false;
boolean b_stop = false;
boolean b_previous = false;
boolean b_next = false;
boolean b_repeat = false;
/*---Buttons Playlist 1 bis 9---*/
boolean b_b1 = false;
boolean b_b2 = false;
boolean b_b3 = false;
boolean b_b4 = false;
boolean b_b5 = false;
boolean b_b6 = false;
boolean b_b7 = false;
boolean b_b8 = false;
boolean b_b9 = false;
/*---StatusScreen---*/
String p_Status;
String p_Name;
String p_SequenceName;
String p_SecondsTotal;
String p_SecondsElapsed;
String p_SecondsRemaining;
String p_Repeat = "0";
String sensortemp;
String fppbranch;
String b_selected = "MAIN";
String button1;
String button2;
String button3;
String button4;
String button5;
String button6;
String button7;
String button8;
String button9;
String button10;

/*---END-SYSTEM-BUTTON---*/

char* UTCOFFSET = "3600";
char* NTP_SERVERS = "de.pool.ntp.org";
char* MQTT_Server = "10.0.1.1";
int MQTT_Port = 1883;
bool MQTT_Secure = true;
char* MQTT_User = "user";
char* MQTT_Password = "password";
char* MQTT_Prefix = "FPP-RD-Control";
char* PAYLOAD_playlist_details = "rpi/falcon/player/FPP/playlist_details";
char* TOPIC_msg = "rpi/falcon/player/FPP/set/playlist/";
char* TOPIC_Subscription = "rpi/falcon/player/FPP/#";
char* TOPIC_msg_suffix_play = "/start";
char* TOPIC_msg_suffix_stop = "/stop/now";
char* TOPIC_msg_suffix_prev = "/prev";
char* TOPIC_msg_suffix_next = "/next";
char* TOPIC_msg_suffix_repeat = "/repeat";
int screen_rotation = 2;
int MINPRESSURE = 100;
int MAXPRESSURE = 2000;
bool ledStatus = true;
bool buzzerStatus = true;
bool TOUCHMAP = true;
//int buzzerPIN = 21;
//int TRIGGER_PIN = 25;

/*-----------------------------------------------------------------------------*/

String params = "["
  "{"
  "'name':'ssid',"
  "'label':'Name des WLAN',"
  "'type':"+String(INPUTTEXT)+","
  "'default':''"
  "},"
  "{"
  "'name':'pwd',"
  "'label':'WLAN Passwort',"
  "'type':"+String(INPUTPASSWORD)+","
  "'default':''"
  "},"
  "{"
  "'name':'MQTT_Server',"
  "'label':'MQTT_Server',"
  "'type':"+String(INPUTTEXT)+","
  "'default':''"
  "},"
  "{"
  "'name':'MQTT_Port',"
  "'label':'MQTT_Port',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':1800,'max':1900,"
  "'default':'1883'"
  "},"
  "{"
  "'name':'MQTT_Secure',"
  "'label':'MQTT_Secure',"
  "'type':"+String(INPUTCHECKBOX)+","
  "'default':'1'"
  "},"
  "{"
  "'name':'MQTT_User',"
  "'label':'MQTT_User',"
  "'type':"+String(INPUTTEXT)+","
  "'default':''"
  "},"
  "{"
  "'name':'MQTT_Password',"
  "'label':'MQTT_Password',"
  "'type':"+String(INPUTPASSWORD)+","
  "'default':''"
  "},"
  "{"
  "'name':'UTC_Offset',"
  "'label':'UTC_Offset',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'3600'"
  "},"
  "{"
  "'name':'NTP_SERVERS',"
  "'label':'NTP_SERVERS',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'de.pool.ntp.org'"
  "},"  
  "{"
  "'name':'PPD',"
  "'label':'PAYLOAD Playlist Details',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'xLights/falcon/player/FPP/playlist_details'"
  "},"
  "{"
  "'name':'TOPIC_msg',"
  "'label':'TOPIC MSG Playlist',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'xLights/falcon/player/FPP/set/playlist/'"
  "},"
  "{"
  "'name':'TOPIC_Subscr',"
  "'label':'TOPIC Subscription',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'xLights/falcon/player/FPP/#'"
  "}," 
  "{"
  "'name':'TOPICmsp',"
  "'label':'TOPIC_msg_suffix_play',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'/start'"
  "}," 
  "{"
  "'name':'TOPICmss',"
  "'label':'TOPIC_msg_suffix_stop',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'/stop/now'"
  "}," 
  "{"
  "'name':'TOPICmspr',"
  "'label':'TOPIC_msg_suffix_prev',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'/prev'"
  "}," 
  "{"
  "'name':'TOPICmsn',"
  "'label':'TOPIC_msg_suffix_next',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'/next'"
  "}," 
  "{"
  "'name':'TOPICmsr',"
  "'label':'TOPIC_msg_suffix_repeat',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'/repeat'"
  "}," 
  "{"
  "'name':'screen_rotation',"
  "'label':'Screen Rotation',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':0,'max':3,"
  "'default':'2'"
  "},"
  "{"
  "'name':'TOUCHMAP',"
  "'label':'TOUCH MAP',"
  "'type':"+String(INPUTCHECKBOX)+","
  "'default':'0'"
  "},"
  "{"
  "'name':'ledStatus',"
  "'label':'LED-Status',"
  "'type':"+String(INPUTCHECKBOX)+","
  "'default':'1'"
  "},"
  "{"
  "'name':'buzzerStatus',"
  "'label':'BUZZER-Status',"
  "'type':"+String(INPUTCHECKBOX)+","
  "'default':'1'"
  "},"
  "{"
  "'name':'buzzerPIN',"
  "'label':'BUZZER-PIN',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':21,'max':27,"
  "'default':'21'"
  "},"
  "{"
  "'name':'triggerPIN',"
  "'label':'CONFIG-PIN',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':21,'max':27,"
  "'default':'25'"
  "}"
  "]";

/*---Config File Setup END-----------------------------------------------------*/
