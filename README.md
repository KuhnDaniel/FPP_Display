# FPP_Display
 Falcon Player Remote TouchDisplay

Steuerung des F-Players (Playliste und Status) auf dem Raspberry (MQTT) Control of the F-Player (playlist and status) on the Raspberry (MQTT) First DemoVideo: https://youtu.be/uxZQKN3vWPk

![Bild](https://github.com/KuhnDaniel/FPP-RD-Control/blob/master/FPP-RD-Control-Bild.png)

    Based on the ArduiTouch ESP from zihatec (https://www.hwhardsoft.de)
    Falcon Player FPP on the Raspberry / MQTT Server https://github.com/FalconChristmas

Version history:

    0.5.2 07/01/2020 change SPIFFS to SD (configfile handling)
    0.5.3 Web Config ok, Config Button ok
    0.5.4 triggerPIN / buzzerPIN Webconfig attached / Webinterface
    0.5.5 BackGround remove
    0.5.6 10/01/2020 Repeat function add
    0.6.0 11/01/2020 Change (call Webconfig Mode at uper left corner on Touchscreen near CPU) add TOUCHMAP 0 or 1 to inverse TOP/BOTTEM coordinates configuration in Webinterface TOUCH MAP true / false some ILI9341 models have the touchscreen foil wrong/reverse

What is it doing: Play the playlist and show/control the status of the FPP player Playlist is automatically sent and updated by the master (FPP) to the Display. Playlist overview with sequence and playing time. The temperature status of the Raspberry is monitored Control (play Stop next prev select) via touch screen change control the repeatstatus/option

!Based on ArduiTouch Modul! // Pins for the ILI9341

    #define TFT_MOSI 23
    #define TFT_CLK 18
    #define TFT_MISO 19
    #define TFT_CS 5 //diplay chip select
    #define TFT_DC 4 //display d/c
    #define TFT_RST 22 //display reset
    #define TFT_LED 15 //display background LED
    #define HAVE_TOUCHPAD
    #define TOUCH_CS 14
    #define TOUCH_IRQ 2
    int buzzerPIN = 21; // ArduiTouch integrated

// Extra Hardware

    int ledPIN = 27; // 1x WS2812 (Status LED) WiFi / MQTT / Json WebConfig / Error

    <WebConfigFPP.h> // fork of WebConfig https://github.com/GerLech/WebConfig SD Cardreader support added bigger filesitze ArduinoJson V.6
