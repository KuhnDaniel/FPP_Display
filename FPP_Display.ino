
/****************************************************************************************************************************
   FPP_Display.ino (fork of OH_View.ino by Heiko Hegewald)
   Steuerung des Falcon Players (Playlisten)
   ArduiTouch https://www.hwhardsoft.de/
   ---------------------------------------------------
   Daniel Kuhn 12.2019 V0.1
   Built by Daniel Kuhn https://dkNetwork.de
   Licensed under MIT license
   Version: 0.1

   Version Modified By   Date      Comments
   ------- -----------  ---------- -----------
    0.1.0   D Kuhn      29/12/2019  Initial coding
    0.1.1               30/12/2019  Touch implement
    0.1.2                           Touch and Beep ok
    0.1.3               30/12/2019  Play Touch Buttons MQTT publish working well
    0.1.4                           Playlist request from FPP
    0.1.5                           Update to ArduinoJson V.6
    0.1.6                           Add FastLED
    0.1.7                           Dynamic Playlist -> max 10
    0.1.8               31/12/2019  Main Screen Setup <- Info from FPP Status
    0.1.9                           Playliste and ArduinoJson works fine (smile)
    0.2.0                           loadPlaylistMenu
    0.2.1               02/01/2020  Menu Button and dynamic Load of Playlist / Send to MQTT working verry well
    0.2.2                           Touch Calibration read file
    0.2.3               03/01/2020  Rename Projektname : FPP-RD Control
    0.2.4                           Redesign MainScreen Menu Playlist, move Up
    0.2.5                           button playlist function add to mqtt send command
    0.2.6                           Player function ad to mqtt send command
    0.3.0                           WiFiManager add Config File config
    0.3.1               04/01/2020  Update basic Fild MQTT_
    0.3.2                           Prepair to all Settings in CONFIG_FILE
    0.3.3                           Alle Configs überarbeitet
    0.4.0                           New Configstruct create
    0.5.0                           New Configstruct create
    0.5.1                           Config minimal ok
    0.5.2               07/01/2020  change SPIFFS to SD (configfile handling)
    0.5.3                           Web Config ok, Config Button ok
    0.5.4                           triggerPIN / buzzerPIN Webconfig attached / Webinterface
    0.5.5                           BackGround remove
    0.5.6               10/01/2020  Repeat function add
    0.6.0               11/01/2020  Change (call Webconfig Mode at uper left corner on Touchscreen near CPU)
                                    add TOUCHMAP 0 or 1 to inverse TOP/BOTTEM coordinates
                                    configuration in Webinterface TOUCH MAP true / false
                                    some ILI9341 models have the touchscreen foil wrong/reverse

   ESP32 boards / TouchScreen

   // MQTT_MAX_PACKET_SIZE : Maximum packet size
   in -> libraries\PubSubClient\src
   #define MQTT_MAX_PACKET_SIZE 128 <---- 512

   Arduino IDE 1.8.10
   ArduinoJSON V.6.x Benoit Blanchon

*/
#include <Arduino.h>
String FPPDISPLAYVERSION = "FPP-RD Control V 0.6.0";

#include <SD.h>

#include <SPI.h>
#include "SPIFFS.h"                               // SPDIFF
#include <FS.h>
#include <PubSubClient.h>                           // by Nick O'Lery 2.7.0
#include <WiFi.h>


#include <XPT2046_Touchscreen.h>                  // by Paul Stoffregen 1.3.0
#include <ILI9341_SPI.h>
//#include "Adafruit_ILI9341.h"
#include <MiniGrafx.h>
#include <ArduinoJson.h>
// Playlists Configfile
#include <HTTPClient.h>
//#include <DHT.h>
#define FASTLED_INTERNAL
#include <FastLED.h>
/*--GL---------------*/
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebConfigFPP.h>
/*-------------------*/
#include "settings.h"
#include "ArialRounded.h"
#include "Roboto.h"

//********************************************
// Definitionen
//********************************************


int SCREEN_WIDTH = 240;
int SCREEN_HEIGHT = 320;
int BITS_PER_PIXEL = 3; // 2^2 =  8 colors
time_t dstOffset = 0;
long timerPress;


// Limited to 16 colors due to memory constraints
ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC, TFT_RST);
MiniGrafx gfx = MiniGrafx(&tft, BITS_PER_PIXEL, palette);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// Define FastLED to controlo CheckLED
#define NUM_LEDS 1
#define ledPIN 27
#define BRIGHTNESS  90
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
// Define the array of leds
CRGB leds[NUM_LEDS];
#define DELAYVAL 100    // Delay CheckLED/Blink

/*---WebserverConfig---*/
WebServer server;
WebConfigFPP conf;

int X, Y;
long Num1, Num2, Number;
char action;
boolean result = false;
bool Touch_pressed = false;
TS_Point p;
/*END Input codelock*/


simpleDSTadjust dstAdjusted(StartRule, EndRule);
//aktuelle Positionen
int tsx, tsy, tsxraw, tsyraw;
//aktueller Touch Zustand
bool tsdown = false;

WiFiClient espClient;
PubSubClient client(espClient);

int mqtt_count = 0;

long lastMsg = 0;
long lastChange = 0;
long lastScreen = 0;

//********************************************
// Funktionen
//********************************************
void callback(char* topic, byte *payload, unsigned int length);
void setMQTT();
void reconnectMQTT();
void connectWiFi();
void drawMainBG();
void DrawMainButton();
void drawTime();
String getTime(time_t *timestamp);
void drawMQTT();
int8_t getWifiQuality();
void drawWifiQuality();
void initDisplay();
void drawScreenPoints(int actual, int count);
void DetectButtons();
void Button_ACK_Tone();
void getplaylists();
void CheckStatusLED();
void fppsystemstatus();
void fppversion();
void loadTouchCalibration();
void led_mqttrx();                            // LED Json/MQTT  Status RX ok  1x Green short
void led_JError();                            // LED Json Error deserialize   1x RED longtime 100
void led_mqtterror();                         // LED MQTT Error disconnect    2x Red short





//********************************************
// WiFi / MQTT
//********************************************
void callback(char* topic, byte *payload, unsigned int length) {
  // Liste der topics durchsuchen
  int found = 0;
  int i = 0;
  int firstSlash = 0;
  int lastSlash = 0;
  mqtt_count = 1;

    if (strstr (topic, conf.getValue("PPD")) != NULL) {

    String inData;
    //Serial.print("payload: ");
    for (int i = 0; i < length; i++) {
      // Serial.print((char)payload[i]);
      inData += (char)payload[i];
    }
    //Serial.println(inData);
    const size_t capacity = 2 * JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(6) + 200;
    //DynamicJsonBuffer jsonBuffer(capacity); //V5
    DynamicJsonDocument doc(1024);

    //JsonObject &root = jsonBuffer.parseObject(inData);  V5
    DeserializationError error = deserializeJson(doc, inData);

    // Test if parsing succeeds.
    if (error) {
      Serial.print("deserializeJson() failed with code ");
      Serial.println(error.c_str());
      led_JError();
      return;
    }

    // Pull the value from key "VALUE" from the JSON message {"value": 1 , "someOtherValue" : 200}

    JsonObject activePlaylists_0 = doc["activePlaylists"][0];                                  //V5
    JsonObject activePlaylists_0_currentItems_0 = activePlaylists_0["currentItems"][0];
    int apl_PlayOnce = activePlaylists_0_currentItems_0["playOnce"]; // 0
    int apl_SecondsElapsed = activePlaylists_0_currentItems_0["secondsElapsed"]; // 24
    int apl_SecondsRemaining = activePlaylists_0_currentItems_0["secondsRemaining"]; // 6
    int apl_SecondsTotal = activePlaylists_0_currentItems_0["secondsTotal"]; // 30
    const char* apl_SequenceName = activePlaylists_0_currentItems_0["sequenceName"]; // "Daniel-Home.fseq"
    const char* apl_Type = activePlaylists_0_currentItems_0["type"]; // "sequence"
    const char* apl_Name = activePlaylists_0["name"]; // "dkNetwork"
    int apl_Repeat = activePlaylists_0["repeat"]; // 1
    const char* apl_Status = doc["status"]; // "playing"
    // DO SOMETHING WITH THE DATA THAT CAME IN!

    // Playbutton Status Farbe GREEN Play NAVY IDLE
    if (strstr (apl_Status, "playing") != NULL) {
      apl_Status_Button = 6;        //GREEN
      apl_Status_Button_Text = 0;   //BLACK
    } else {
      apl_Status_Button = 4;        //NAVY
      apl_Status_Button_Text = 1;   //WHITE
    }
    p_Name = apl_Name,
    p_SequenceName = apl_SequenceName;
    p_Status = apl_Status;
    p_Status.toUpperCase();
    p_SecondsTotal = apl_SecondsTotal;
    p_SecondsElapsed = apl_SecondsElapsed;
    p_SecondsRemaining = apl_SecondsRemaining;
    p_Repeat = apl_Repeat;
    if (apl_Repeat) {
      apl_Repeat_Button = 4;        //GREEN
      apl_Repeat_Button_Text = 5;   //Red
    } else {
      apl_Repeat_Button = 4;        //NAVY
      apl_Repeat_Button_Text = 9;   //ULTRA_DARKGREY
    }
    /*---RX Status to LED*/
    led_mqttrx();
  }
}
/* END MQTT CALLBACK */

void setMQTT() {
  const char* MQTT_Server = conf.getValue("MQTT_Server");
  String sMQTT_Port = (conf.getValue("MQTT_Port"));
  MQTT_Port = (sMQTT_Port.toInt());

  if (WiFi.status() == WL_CONNECTED){
  client.setServer(MQTT_Server, MQTT_Port);
  client.setCallback(callback);
  }
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = conf.getApName(); //"FPP_Display001";
    // Attempt to connect
    
    
    //if (client.connect(clientId.c_str(), MQTT_User, MQTT_Password)) {
    if (client.connect(clientId.c_str(), conf.getValue("MQTT_User"), conf.getValue("MQTT_Password"))) {
      Serial.println("connected");
      //Once connected, publish an announcement...
      client.publish(TOPIC_Presence, "ON");
      // ... and resubscribe
      client.subscribe(TOPIC_Subscription);
      ClientStatus = (client.state());
      Serial.println(ClientStatus);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      led_mqtterror();
      // Wait 5 seconds before retrying
      mqtt_count = 0;
      ClientStatus = (client.state());
      Serial.println(ClientStatus);
      // initDisplay();
      delay(2000);

    }
  }
}
/* END SET MQTT */



void drawMainBG() {
  /*---TOP Box---*/
  gfx.fillBuffer(MINI_BLACK);
  gfx.setColor(9);
  gfx.fillRect(0, 0, 240, 70);
  gfx.setColor(MINI_NAVY);
  gfx.fillRect(1, 1, 238, 68);

  /*----FPP-RD-Control-Version----*/
  gfx.setColor(MINI_RED);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(Roboto_Medium_16);
  gfx.drawString(120, 300, FPPDISPLAYVERSION);

  /*----FPP-RD-Control-Version----*/
}
/* END drawMainBG */

void drawScreenPoints(int actual, int count) {
  int i = 0;
  String temp0 = " -";
  String temp1 = " +";
  String result = "";

  gfx.setColor(MINI_YELLOW);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(Roboto_Medium_24);
  for (i = 1; i <= count; i++) {
    if (i == actual) {
      result = result + temp1;
    }
    else {
      result = result + temp0;
    }
  }
  gfx.drawString(120, 190, result + " ");
}
/* END drawScreenPoints */

/*---Draw Main Screen---*/
void DrawMainButton() {
  gfx.setColor(MINI_WHITE);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(5, 78,  "FPP");
  gfx.drawString(65, 78,  (p_Status));
  if (p_Status != "IDLE") {
    gfx.setFont(Roboto_Medium_16);
    gfx.drawString(5, 97,  "P-List");
    gfx.drawString(65, 97,  (p_Name));
    gfx.drawString(5, 112, "Seq.N");
    gfx.drawString(65, 112, (p_SequenceName));
    gfx.drawString(5, 127, "Repeat");
    gfx.drawString(65, 127, (p_Repeat));
    gfx.drawString(5, 142, "Sec.T");
    gfx.drawString(65, 142, (p_SecondsTotal));
    gfx.drawString(5, 157, "Sec.E");
    gfx.drawString(65, 157, (p_SecondsElapsed));
    gfx.drawString(5, 172, "Sec.R");
    gfx.drawString(65, 172,  (p_SecondsRemaining));
  }
  if (p_Status == "IDLE") {

    /*---BG Button Line1---*/
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setColor(9);
    gfx.fillRect(0, 100, 240, 30); //
    /*---Button1---*/
    gfx.setColor(MINI_NAVY);
    gfx.fillRect(1, 101, 79, 29);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(39, 104, (button1));
    /*---Button2----*/
    gfx.setColor(MINI_NAVY);
    gfx.fillRect(81, 101, 79, 29);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(120, 104, (button2));
    /*---Button3----*/
    gfx.setColor(MINI_NAVY);
    gfx.fillRect(161, 101, 78, 29);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(200, 104, (button3));
    /*---BG Button Line2---*/
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setColor(9);
    gfx.fillRect(0, 130, 240, 30);
    /*---Button4---*/
    gfx.setColor(MINI_NAVY);
    gfx.fillRect(1, 131, 79, 29);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(39, 134, (button4));
    /*---Button5----*/
    gfx.setColor(MINI_NAVY);
    gfx.fillRect(81, 131, 79, 29);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(120, 134, (button5));
    /*---Button6----*/
    gfx.setColor(MINI_NAVY);
    gfx.fillRect(161, 131, 78, 29);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(200, 134, (button6));

    /*---BG Button Line3---*/
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setColor(9);
    gfx.fillRect(0, 160, 240, 31);
    /*---Button7---*/
    gfx.setColor(MINI_NAVY);
    gfx.fillRect(1, 161, 79, 29);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(39, 164, (button7));
    /*---Button8----*/
    gfx.setColor(MINI_NAVY);
    gfx.fillRect(81, 161, 79, 29);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(120, 164, (button8));
    /*---Button9----*/
    gfx.setColor(MINI_NAVY);
    gfx.fillRect(161, 161, 78, 29);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(200, 164, (button9));
  }
  /*---Player Buttons---*/
  gfx.setFont(Roboto_Medium_16);

  gfx.fillRect(0, 220, 60, 30);  //next
  gfx.setColor(apl_Status_Button);
  gfx.fillRect(1, 221, 58, 28);
   gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(apl_Status_Button_Text);
  gfx.drawString(30, 225, "Play");
  
  gfx.setColor(9);
  gfx.fillRect(120, 220, 120, 30);

  gfx.fillRect(60, 220, 120, 30);
  gfx.setColor(apl_Repeat_Button);
  gfx.fillRect(61, 221, 118, 28);
   gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(apl_Repeat_Button_Text);
  gfx.drawString(90, 225, "Rep.");

  gfx.setColor(9);
  gfx.fillRect(120, 220, 120, 30);
  gfx.setColor(MINI_NAVY);
  gfx.fillRect(121, 221, 118, 28);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(180, 225, "Stop");
  gfx.setColor(9);
  gfx.fillRect(0, 250, 120, 30);
  gfx.setColor(MINI_NAVY);
  gfx.fillRect(1, 251, 180, 28);
  gfx.setColor(9);
  gfx.fillRect(120, 250, 120, 30);
  gfx.setColor(MINI_NAVY);
  gfx.fillRect(121, 251, 118, 28);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(60, 255, "Previous");
  gfx.setColor(MINI_WHITE);
  gfx.drawString(180, 255, "Next");
}
/* END Draw Main Screen */

/*---drawTime---*/
void drawTime() {

  char time_str[11];
  char *dstAbbrev;
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_WHITE);
  String date = WDAY_NAMES[timeinfo->tm_wday] + " " + String(timeinfo->tm_mday) + ". " + MONTH_NAMES[timeinfo->tm_mon] + " " + String(1900 + timeinfo->tm_year);
  gfx.drawString(120, 6, date);

  gfx.setFont(ArialRoundedMTBold_36);
  sprintf(time_str, "%02d:%02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  gfx.drawString(120, 20, time_str);

  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(MINI_BLUE);
  sprintf(time_str, "%s", dstAbbrev);
  gfx.drawString(198, 27, time_str);  // Known bug: Cuts off 4th character of timezone abbreviation
}
/*---END drawTime---*/

String getTime(time_t *timestamp) {
  struct tm *timeInfo = gmtime(timestamp);

  char buf[6];
  sprintf(buf, "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  return String(buf);
}
/*---drawMQTT---*/
void drawMQTT() {
  if (!client.connected()) {
    gfx.setColor(MINI_RED);
    mqtt_count = 0;               //MQTT Connect broken
  }
  else {
    gfx.setColor(MINI_GREEN);
    mqtt_count = 1;               //MQTT Connect ONLINE
  }
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(235, 50, "B");
}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}

void drawWifiQuality() {
  int8_t quality = getWifiQuality();
  gfx.setColor(MINI_YELLOW);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(223, 9, String(quality) + "%");
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        gfx.setPixel(225 + 2 * i, 18 - j);
      }
    }
  }
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(5, 5, String("FPP"));
  gfx.setColor(MINI_GREEN);
  gfx.drawString(5, 16, fppbranch);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(5, 27, String("CPU"));
  gfx.setColor(MINI_GREEN);
  gfx.drawString(5, 38, sensortemp);
  gfx.setColor(MINI_WHITE);
}

// Progress bar helper
void drawProgress(uint8_t percentage, String text) {
  gfx.fillBuffer(MINI_BLACK);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_WHITE);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(120, 146, text);
  gfx.setColor(MINI_WHITE);
  gfx.drawRect(10, 168, 240 - 20, 15);
  gfx.setColor(MINI_BLUE);
  gfx.fillRect(12, 170, 216 * percentage / 100, 11);
  gfx.commit();
}

void initDisplay() {
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, LOW);
  Serial.println("initialization TFT ...");
  gfx.init();
  gfx.fillBuffer(MINI_BLACK);
  Serial.println("TFT initialization done...");
  gfx.commit();
  /*---INIT Rotation Screen MUSS hier stehen---*/
  tft.setRotation(2);
  
  drawMainBG();
  gfx.commit();
  timerPress = millis();
  Serial.println("MAINCSCREEN activ!");
}
/********************************************************************//**
   @brief     detects a touch event and converts touch data
   @param[in] None
   @return    boolean (true = touch pressed, false = touch unpressed)
 *********************************************************************/
bool Touch_Event() {
  p = touch.getPoint();
  delay(2);
  if (conf.getBool("TOUCHMAP")){
  p.x = map(p.x, TS_MINY, TS_MAXY, tft.height(), 0);       // TOUCHMAP = 1
  p.y = map(p.y, TS_MINX, TS_MAXX, 0, tft.width());
  if (p.z > MINPRESSURE) return true;
  return false;
  } else {
    p.x = map(p.x, TS_MINY, TS_MAXY, 0, tft.height());       // TOUCHMAP = 0
  p.y = map(p.y, TS_MINX, TS_MAXX, 0, tft.width());
  if (p.z > MINPRESSURE) return true;
  return false;
    
  }
  
}
/********************************************************************//**
   @brief     detecting pressed buttons with the given touchscreen values
   @param[in] None
   @return    None
 *********************************************************************/
void DetectButtons()
{
  /*---Buttonleisten---*/
  /*---PLAYLIST TOUCH ONLY IF PLAY IN IDLE MODE---*/
if (p_Status == "IDLE") {
  /*---Playlist Line 1---*/
  if (Y > 106 && Y < 130) //Detecting Buttons on Playlist R1
  {
    if (X >= 1 && X <= 67)
    { Serial.println ("Playlist B1");
      Button_ACK_Tone();
      b_b1 = true;
    }
    if (X >= 80 && X <= 153)
    { Serial.println ("Playlist B2");
      Button_ACK_Tone();
      b_b2 = true;
    }
    if (X >= 173 && X <= 243)
    { Serial.println ("Playlist B3");
      Button_ACK_Tone();
      b_b3 = true;
    }
  }
  /*---Playlist Line 2---*/
  if (Y > 141 && Y < 164) //Detecting Buttons on Playlist R2
  {
    if (X >= 1 && X <= 67)
    { Serial.println ("Playlist B4");
      Button_ACK_Tone();
      b_b4 = true;
    }
    if (X >= 80 && X <= 153)
    { Serial.println ("Playlist B5");
      Button_ACK_Tone();
      b_b5 = true;
    }
    if (X >= 173 && X <= 243)
    { Serial.println ("Playlist B6");
      Button_ACK_Tone();
      b_b6 = true;
    }
  }
  /*---Playlist Line 3---*/
  if (Y > 177 && Y < 193) //Detecting Buttons on Playlist R3
  {
    if (X >= 1 && X <= 67)
    { Serial.println ("Playlist B7");
      Button_ACK_Tone();
      b_b7 = true;
    }
    if (X >= 80 && X <= 153)
    { Serial.println ("Playlist B8");
      Button_ACK_Tone();
      b_b8 = true;
    }
    if (X >= 173 && X <= 243)
    { Serial.println ("Playlist B9");
      Button_ACK_Tone();
      b_b9 = true;
    }
  }
}
  /*--- END Playlist---*/

  if (Y < 30) //Detecting Buttons on Systemstatus
  {
    if (X >= 0 && X <= 20)
    { Serial.println ("Systemstatus WEBCONFIG");
      Button_ACK_Tone();
      b_systemstatus = true;

    }
  }


  /*---Player Line C1---*/
  if (Y >= 244 && Y <= 270) //Detecting Buttons on Player Column 1
  {
    if (X >= 3 && X <= 56)
    { Serial.println ("Button Play");
      Button_ACK_Tone();
      b_play = true;
    }
    
    if (X >= 63 && X <= 116)
    { Serial.println ("Button Repeat");
      Button_ACK_Tone();
      b_repeat = true;
    }

    if (X >= 126 && X <= 238)
    { Serial.println ("Button Stop");
      Button_ACK_Tone();
      b_stop = true;
    }
  }
  /*---Player Line C2---*/
  if (Y > 280 && Y < 297) //Detecting Buttons on Player Column 2
  {
    if (X >= 1 && X <= 114)
    { Serial.println ("Button Previous");
      Button_ACK_Tone();
      b_previous = true;
    }

    if (X >= 130 && X <= 233)
    { Serial.println(F("Button Next"));
      Button_ACK_Tone();
      b_next = true;
    }
  }
}


/********************************************************************//**
   @brief     plays ack tone (beep) after button pressing
   @param[in] None
   @return    None
 *********************************************************************/
void Button_ACK_Tone() {
  //Serial.print("buzzerStatus: ");
  //Serial.println(conf.getBool("buzzerStatus"));
    if (conf.getBool("buzzerStatus")) {
    ledcWriteTone(0, 1000);
    delay(50);
    ledcWriteTone(0, 0);
  }
}

/*----------------------------------------LED Status---------------------------------------*/
void CheckStatusLED()
{
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(200);
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(200);
  leds[0] = CRGB::Blue;
  FastLED.show();
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show();
  //delay(200);
}
/*---WiFi ok---*/
void led_WiFi()
{
  //  WiFi OK Status LED
  leds[0] = CRGB::Blue;
  FastLED.show();
  delay(30);
  leds[0] = CRGB::Black;
  FastLED.show();
}
/*---RX MQTT ok---*/
void led_mqttrx()
{
  //Serial.print("STATUS-LED 0 off / 1 on:  ");
  //Serial.println(conf.getBool("ledStatus"));
    if (conf.getBool("ledStatus")) {
    //  WiFi OK Status LED
    leds[0] = CRGB::Green;
    FastLED.show();
    delay(30);
    leds[0] = CRGB::Black;
    FastLED.show();
  }
}
void led_mqtterror()
{
  //  MQTT Error 2x Status LED
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(50);
  leds[0] = CRGB::Black;
  FastLED.show();
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(50);
  leds[0] = CRGB::Black;
  FastLED.show();
}
/*---JSON Parser Error 1 time RED longtime 200*/
void led_JError()
{
  //  JError Status LED
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show();
}
/*---END LED Status------------------------------------------*/

/*---FPP-SYSTEMSTATUS----------------------------------------*/
void fppsystemstatus() {
  HTTPClient http;  //Object of class HTTPClient
  http.begin("http://10.0.1.24/api/fppd/status");
  int httpCode = http.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    String inData = http.getString();
    //Serial.println(inData);
    const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(17) + 1024;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, inData);
    // Test if parsing succeeds.
    if (error) {
      Serial.print("deserializeJson() failed with code ");
      Serial.println(error.c_str());
      led_JError();
      return;
    }
    JsonObject current_playlist = doc["current_playlist"];
    const char* current_playlist_count = current_playlist["count"]; // "0"
    const char* current_playlist_index = current_playlist["index"]; // "0"
    const char* current_playlist_playlist = current_playlist["playlist"]; // ""
    const char* current_playlist_type = current_playlist["type"]; // ""

    const char* current_sequence = doc["current_sequence"]; // ""
    const char* current_song = doc["current_song"]; // ""
    const char* fppd = doc["fppd"]; // "running"
    int mode = doc["mode"]; // 2
    const char* mode_name = doc["mode_name"]; // "player"

    const char* next_playlist_playlist = doc["next_playlist"]["playlist"]; // "Countdown"
    const char* next_playlist_start_time = doc["next_playlist"]["start_time"]; // "Wednesday @ 23:59:00 - (Everyday)"

    const char* repeat_mode = doc["repeat_mode"]; // "0"
    const char* seconds_played = doc["seconds_played"]; // "0"
    const char* seconds_remaining = doc["seconds_remaining"]; // "0"

    JsonObject sensors_0 = doc["sensors"][0];
    const char* sensors_0_formatted = sensors_0["formatted"]; // "44.0"
    const char* sensors_0_label = sensors_0["label"]; // "CPU: "
    const char* sensors_0_postfix = sensors_0["postfix"]; // ""
    const char* sensors_0_prefix = sensors_0["prefix"]; // ""
    float sensors_0_value = sensors_0["value"]; // 44.008
    sensortemp = String(sensors_0_value);
    const char* sensors_0_valueType = sensors_0["valueType"]; // "Temperature"

    int status = doc["status"]; // 0
    const char* status_name = doc["status_name"]; // "idle"
    const char* time = doc["time"]; // "Wed Jan 01 21:55:00 CET 2020"
    const char* time_elapsed = doc["time_elapsed"]; // "00:00"
    const char* time_remaining = doc["time_remaining"]; // "00:00"
    int volume = doc["volume"]; // 37
    serializeJson(doc, Serial);
    Serial.println();
    /*--END---*/
    http.end();
  }
}

void fppversion() {
  HTTPClient http;  //Object of class HTTPClient
  http.begin("http://10.0.1.24/api/fppd/version");
  int httpCode = http.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    String inData = http.getString();
    //Serial.println(inData);
    //const char* json = "{\"branch\":\"master\",\"fppdAPI\":\"v1\",\"message\":\"\",\"respCode\":200,\"status\":\"OK\",\"version\":\"2.x-292-g83816a39-dirty\"}";
    const size_t capacity = JSON_OBJECT_SIZE(6) + 128;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, inData);
    // Test if parsing succeeds.
    if (error) {
      Serial.print("deserializeJson() failed with code ");
      Serial.println(error.c_str());
      led_JError();
      return;
    }
    //deserializeJson(doc, inData);
    const char* branch = doc["branch"]; // "master"
    fppbranch = branch;
    const char* fppdAPI = doc["fppdAPI"]; // "v1"
    const char* message = doc["message"]; // ""
    int respCode = doc["respCode"]; // 200
    const char* status = doc["status"]; // "OK"
    const char* version = doc["version"]; // "2.x-292-g83816a39-dirty"
    serializeJson(doc, Serial);
    Serial.println();
    /*--END---*/
    http.end();
  }
}

/*------GET Playlist----*/
void getplaylists() {
  HTTPClient http;  //Object of class HTTPClient
  http.begin("http://10.0.1.24/api/playlists");
  int httpCode = http.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    String inData = http.getString();
    //Serial.println(inData);
    //const char* json = "[\"Countdown\",\"DRB\",\"HNY2020\",\"FPP-Disply\",\"Nachrichten\",\"dkNetwork\"]";
    const size_t capacity = JSON_ARRAY_SIZE(10) + 512;
    DynamicJsonDocument pldoc(capacity);
    DeserializationError error = deserializeJson(pldoc, inData);
    // Test if parsing succeeds.
    if (error) {
      Serial.print("deserializeJson() failed with code ");
      Serial.println(error.c_str());
      led_JError();
      return;
    }
    const char* root_0 = pldoc[0]; // "Countdown"
    button1 = root_0;
    const char* root_1 = pldoc[1]; // "DRB"
    button2 = root_1;
    const char* root_2 = pldoc[2]; // "HNY2020"
    button3 = root_2;
    const char* root_3 = pldoc[3]; // "FPP-Disply"
    button4 = root_3;
    const char* root_4 = pldoc[4]; // "Nachrichten"
    button5 = root_4;
    const char* root_5 = pldoc[5]; // "dkNetwork"
    button6 = root_5;
    const char* root_6 = pldoc[6]; // "DRB"
    button7 = root_6;
    const char* root_7 = pldoc[7]; // "HNY2020"
    button8 = root_7;
    const char* root_8 = pldoc[8]; // "FPP-Disply"
    button9 = root_8;
    const char* root_9 = pldoc[9]; // "Nachrichten"
    button10 = root_9;

    serializeJson(pldoc, Serial);
    Serial.println();
    /*--END---*/
    http.end();
  }
  led_mqttrx(); // LED Staus Json ok 1 time Green short
}

void sendMQTTplay(String b_selected) {
  String pl_button = conf.getValue("TOPIC_msg");
  pl_button += b_selected;
  pl_button += conf.getValue("TOPICmsp");
  Serial.println(pl_button);
  char topic[100];

  pl_button.toCharArray(topic, 100);
  client.publish(topic, "0");
  return;
}


void sendMQTTplayer( String b_selected) {
  if (b_play) {
    String pl_button = conf.getValue("TOPIC_msg");
    pl_button += b_selected;
    pl_button += conf.getValue("TOPICmsp");
    Serial.println(pl_button);
    char topic[100];
    pl_button.toCharArray(topic, 100);
    client.publish(topic, "0");
    return;
  }

  if (b_repeat) {
    String pl_button = conf.getValue("TOPIC_msg");
    pl_button += b_selected;
    pl_button += conf.getValue("TOPICmsr");
    Serial.println(pl_button);
    char topic[100];
    pl_button.toCharArray(topic, 100);
    if (p_Repeat == "1"){
      client.publish(topic, "0");
    } else {
      client.publish(topic, "1");
      }
    return;
  }

  if (b_stop) {
    String pl_button = conf.getValue("TOPIC_msg");
    pl_button += b_selected;
    pl_button += conf.getValue("TOPICmss");
    Serial.println(pl_button);
    char topic[100];
    pl_button.toCharArray(topic, 100);
    client.publish(topic, "0");
    return;
  }
  if (b_previous) {
    String pl_button = conf.getValue("TOPIC_msg");
    pl_button += b_selected;
    pl_button += conf.getValue("TOPICmspr");
    Serial.println(pl_button);
    char topic[100];
    pl_button.toCharArray(topic, 100);
    client.publish(topic, "0");
    return;
  }
  if (b_next) {
    String pl_button = conf.getValue("TOPIC_msg");
    pl_button += b_selected;
    pl_button += conf.getValue("TOPICmsn");
    Serial.println(pl_button);
    char topic[100];
    pl_button.toCharArray(topic, 100);
    client.publish(topic, "0");
    return;

  }
}
/*-- FILE HANDLING----------------------------------------*/
// Prints the content of a file to the Serial
void printFile(const char *filename) {
  // Open file for reading
  File file = SPIFFS.open(filename);
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
}

bool checkFile(fs::FS &fs,const char *path) {
      File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        
        file.close();
        return true;
    } else {
        Serial.println("Failed to open file for reading");
        return false;
    }
}
  



void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

void CallWebConfig(){
  
  leds[0] = CRGB::Red;
  FastLED.show();
  
}
/*---HANLE ROOT--------------------------------------------------------------------------------*/
void handleRoot() {
  conf.handleFormRequest(&server);
  if (server.hasArg("SAVE")) {
    uint8_t cnt = conf.getCount();
    Serial.println("*********** Konfiguration ************");
    for (uint8_t i = 0; i<cnt; i++) {
      Serial.print(conf.getName(i));
      Serial.print(" = ");
      Serial.println(conf.values[i]);
    }
    if (conf.getBool("switch")) Serial.printf("%s %s %s %s \n",
                                conf.getValue("ssid"),
                                conf.getString("NTP_SERVERS").c_str(), 
                                conf.getString("MQTT_User").c_str(), 
                                conf.getString("TcS").c_str());
  }
}
bool checkConfigFile(fs::FS &fs,const char *path) {
      File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
    }
} 
boolean initWiFi() {
    boolean connected = false;
    configStatus = "SYSTEMINIT";
    time_t now;
    WiFi.mode(WIFI_STA);
    Serial.print("Verbindung zu ");
    Serial.print(conf.values[0]);
    Serial.println(" herstellen");
    if (conf.values[0] != "") {
      WiFi.begin(conf.values[0].c_str(),conf.values[1].c_str());
      uint8_t cnt = 0;
      
      while ((WiFi.status() != WL_CONNECTED) && (cnt<20)){
       // delay(500);
        //Serial.print(".");
        delay(500);
        Serial.print(".");       
        cnt++;
        }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("IP-Adresse = ");
        Serial.println(WiFi.localIP());
        connected = true;
        #define UTC_OFFSET +1
        configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
        // calculate for time calculation how much the dst class adds.
        dstOffset = UTC_OFFSET * 3600 + dstAdjusted.time(nullptr) - now;
        Serial.printf("Time difference for DST: %d\n", dstOffset);
        configStatus = "RUN";
      }
    }
    if (!connected) {
          WiFi.mode(WIFI_AP);
          WiFi.softAP(conf.getApName(),"",1);
          configStatus = "CONFIGMODE"; 
          drawProgress(100, "Connected to '" + String(WIFI_SSID) + "'"); 
    }
    return connected;
}
/*---END initWiFi---*/
/*------------------------------------------SETUP------------------------------------------*/
void setup() {
  // System INIT Setup

  Serial.begin(74880);
  Serial.println("boote ...");
  /*---Initialize SPIFFS library---*/
  
  while (!SPIFFS.begin()) {
    Serial.println(F("Failed to initialize SPIFFS library"));
    delay(500);
  }
  
  SD.begin(17);
  
  /*---Start Webserver---*/
  Serial.println(params);
  conf.setDescription(params);
  conf.readConfig();
  initWiFi();
  char dns[30];
  sprintf(dns,"%s.local",conf.getApName());
  if (MDNS.begin(dns)) {
    Serial.println("MDNS responder gestartet");
  }
  server.on("/",handleRoot);
  server.begin(80);
  /*---                        ---*/
  Serial.println(FPPDISPLAYVERSION);

  /*--- SYSTEM INIT             ---*/

  /*---sound configuration---*/
  String sbuzzerPIN = (conf.getValue("buzzerPIN"));
  int buzzerPIN = (sbuzzerPIN.toInt());
  
  
  ledcSetup(0, 1E5, 12);
  //ledcAttachPin(conf.getValue("buzzerPIN").toInt(), 0);
  ledcAttachPin(buzzerPIN, 0);
  /*---END sound configuration---*/

  /*---FastLED---*/
  // Initialize the LED digital pin as an output.
  pinMode(ledPIN, OUTPUT);
  FastLED.addLeds<LED_TYPE, ledPIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  /*---END FastLED*/

  // Initialize trigger pins HTTP Server
  String striggerPIN = (conf.getValue("triggerPIN"));
  int triggerPIN = (striggerPIN.toInt());
  
  pinMode(triggerPIN, INPUT_PULLUP);

  //loadTouchCalibration();
  /*--- INIT DISPLAY---*/
  initDisplay();
  touch.begin();

  /*--- INIT WIFI SYSTEM---*/
  //connectWiFi();
  
  /*--- INIT MQTT SYSTEM---*/
  setMQTT();


  /*--- INIT SYSTEM LED & TON---*/
  CheckStatusLED();
  ledcWriteTone(0, 1000);
  delay(100);
  ledcWriteTone(0, 0);
}
/*------------------------------------------END SETUP------------------------------------------*/

/*------------------------------------------  LOOP   ------------------------------------------*/
void loop() {
  if (configStatus == "CONFIGMODE" || configStatus == "WEBCONFIGMODE") { 
  server.handleClient();
  }
  String striggerPIN = (conf.getValue("triggerPIN"));
  int triggerPIN = (striggerPIN.toInt());
  if (digitalRead(triggerPIN) == LOW)  {
      Serial.println("\nCONFIG-PIN PUSHED---Webportal requested.");
      configStatus = "WEBCONFIGMODE";
      }

  if (configStatus == "WEBCONFIGMODE") {
    drawMainBG();
    //gfx.drawBmpFromFile("/grill-bg.bmp", 20, 0);
    gfx.setFont(Roboto_Medium_24);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(120, 15, "!___WEB Config___!");
    gfx.setColor(MINI_RED);
    gfx.drawString(120, 135, "Connect to ");
    gfx.setColor(MINI_WHITE);
    gfx.drawString(120, 165, WiFi.localIP().toString());
    gfx.commit();
    led_mqtterror();
      }

      
  if (configStatus == "CONFIGMODE") {
    gfx.setFont(Roboto_Medium_24);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(120, 15, "!...Config Mode...!");
    gfx.setColor(MINI_RED);
    gfx.drawString(120, 135, "SSID: ");
    gfx.setColor(MINI_WHITE);
    gfx.drawString(120, 165, conf.getApName());
    gfx.drawString(120, 195, "http://192.168.4.1");
    gfx.commit();
    led_mqtterror();
      }

  
  if (configStatus == "RUN") {
    if (!client.connected()) {
    reconnectMQTT();
      }
  client.loop();

  now1 = millis();
  drawMainBG();
  drawTime();
  drawWifiQuality();
  drawMQTT();
  DrawMainButton();

  if (mqtt_count != 0) {
    lastScreen = 1;

  }
  if (mqtt_count == 0) {
    lastScreen = 0;
  }
  if (lastScreen == 0) {
    gfx.setFont(Roboto_Medium_24);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_RED);
    gfx.drawString(120, 135, "Broker disconnect...");
    led_mqtterror();

  }
  
  else {
    drawScreenPoints(lastScreen, mqtt_count);
  }
  gfx.commit();

  // check touch screen for new events
  if (Touch_Event() == true) {
    X = p.y; Y = p.x;
    Touch_pressed = true;   // false no action true action
    
    /*      uncheck to calibrate and find positions
    Serial.print("X = ");
    Serial.println(X);
    Serial.print("Y = ");
    Serial.println(Y);
    */
    
  } else {
    Touch_pressed = false;
  }

  // if touch is pressed detect pressed buttons
  if (Touch_pressed == true) {
    DetectButtons();

    if (b_b1 == true) {
      b_b1 = false;
      b_selected = button1;
      sendMQTTplay(b_selected);
      return;
    }
    if (b_b2 == true) {
      b_b2 = false;
      b_selected = button2;
      sendMQTTplay(b_selected);
      return;
    }
    if (b_b3 == true) {
      b_b3 = false;
      b_selected = button3;
      sendMQTTplay(b_selected);
      return;
    }
    if (b_b4 == true) {
      b_b4 = false;
      b_selected = button4;
      sendMQTTplay(b_selected);
      return;
    }
    if (b_b5 == true) {
      b_b5 = false;
      b_selected = button5;
      sendMQTTplay(b_selected);
      return;
    }
    if (b_b6 == true) {
      b_b6 = false;
      b_selected = button6;
      sendMQTTplay(b_selected);
      return;
    }
    if (b_b7 == true) {
      b_b7 = false;
      b_selected = button7;
      sendMQTTplay(b_selected);
      return;
    }
    if (b_b8 == true) {
      b_b8 = false;
      b_selected = button8;
      sendMQTTplay(b_selected);
      return;
    }
    if (b_b9 == true) {
      b_b9 = false;
      b_selected = button9;
      sendMQTTplay(b_selected);
      return;
    }
    
    /*---END Send Button PL MQTT---*/
    if (b_systemstatus == true) {
      b_systemstatus = false;
      //fppsystemstatus();
      //fppversion();
      configStatus = "WEBCONFIGMODE";
      return;
    }

    if (b_playlist == true) {
      Serial.println("PlayListe reload ...");
      b_playlist = false;
      getplaylists();
      return;
    }

    /*--Send Player Button---*/
    if (b_play == true) {
      sendMQTTplayer(b_selected);
      b_play = false;
      return;
    }
    if (b_stop == true) {
      sendMQTTplayer(b_selected);
      b_stop = false;
      return;
    }
    if (b_previous == true) {
      sendMQTTplayer(b_selected);
      b_previous = false;
      return;
    }
    if (b_next == true) {
      sendMQTTplayer(b_selected);
      b_next = false;
      return;
    }
    if (b_repeat == true) {
      sendMQTTplayer(b_selected);
      b_repeat = false;
      return;
    }
  }

  if (now1 - lastChange > 10000) {
    if (fppRequest == true) {
      Serial.println("Reload FPP/Status,Playlist");
      lastChange = now1;
      fppversion();
      delay(10);
      fppsystemstatus();
      delay(10);
      getplaylists();
      fppRequest = false;
    }

  }

  if (now1 - lastChange > 60000) {
    Serial.println("Reload/Update FPP/Status,Playlist");
    fppversion();
    delay(10);
    fppsystemstatus();
    delay(10);
    getplaylists();
    lastChange = now1;
  }
  }
  gfx.commit();
  

/*------------------------------------------END LOOP------------------------------------------*/
}
/*------------------------------------------END LOOP------------------------------------------*/ 

/*--- FILEHANDLING---*/
/*
    listDir(SD, "/", 0);
    createDir(SD, "/mydir");
    listDir(SD, "/", 0);
    removeDir(SD, "/mydir");
    listDir(SD, "/", 2);
    writeFile(SD, "/hello.txt", "Hello ");
    appendFile(SD, "/hello.txt", "World!\n");
    readFile(SD, "/hello.txt");
    deleteFile(SD, "/foo.txt");
    renameFile(SD, "/hello.txt", "/foo.txt");
    readFile(SD, "/foo.txt");
    testFileIO(SD, "/test.txt");
    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
*/
