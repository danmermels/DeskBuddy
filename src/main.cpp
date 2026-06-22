#include <Arduino.h>
#include <U8g2lib.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "../Credentials.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

struct tm  ts;
char buf[80];
int temp;

int UsrMode;

u_long modetimer = 10000;
u_long away = 0;
u_long desk = 0;

// Pin definitions for ESP32-C3
// Default I2C pins for ESP32-C3 are SDA=8, SCL=9.
// IR pin must be an ADC pin. GPIO 3 is a good choice (ADC1_CH3).
const int SDA_pin = 8;
const int SCL_pin = 9;
const int IRpin = 3;

// Static IP Configuration
IPAddress local_IP(192, 168, 15, 160);  // Set static IP to 192.168.15.160
IPAddress gateway(192, 168, 15, 1);    // Gateway for 192.168.15.x network
IPAddress subnet(255, 255, 255, 0);     // Subnet mask
IPAddress primaryDNS(1, 1, 1, 1);       // Primary DNS (optional, default to 1.1.1.1)
IPAddress secondaryDNS(8, 8, 8, 8);     // Secondary DNS (optional, default to 8.8.8.8)
int IRValue;
int IRValOLD;
u_long IRValConstTime;
int IRCount = 0;

String payload = ""; //weather

//TIMERS
int timeline = 0;
int timeNeedle = 0;
u_long refreshTime = 30000;
u_long refreshWeather = 30000;

//CLIENTS
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//DISPLAY CONSTRUCTOR
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0); 

const char* getWiFiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

//SETUP FUNCTION
void setup(void) {
  Serial.begin(115200);
  
  // Initialize I2C with specified pins for ESP32-C3
  Wire.begin(SDA_pin, SCL_pin);
  u8g2.begin();

  // Set Hostname
  WiFi.setHostname("DeskBuddy");
  
  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure static IP");
  } else {
    Serial.println("Static IP configured successfully");
  }
  
  WiFi.begin(SSID, PASS);
  //Serial.println(payload);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  timeClient.begin();
  timeClient.setTimeOffset(-10800);

  #pragma region OTA
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if      (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  #pragma endregion
}

//LOOP FUNCTION
void loop(void) {
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
  }

//GO TO
  timeNeedle = millis()-timeline;

//READ IR
  float volts = analogRead(IRpin)*0.0048828125;  // value from sensor * (5/1024)
  IRValue = 13*pow(volts, -1); // worked out from datasheet graph
  if (IRValue > 22) {
  IRValue = 22;
  }

//Average last 20 reads
  //IRValSum[IRCount]=IRValue;
  //IRValAvg=0;
  //for(int i = 0; i < 20; i++) {
  //  IRValAvg = IRValAvg + IRValSum[i];
  //  }
 // IRValAvg=IRValAvg/20;
 // IRCount = IRCount + 1;
//  if (IRCount>19){
 //   IRCount = 0;
 //   }
  if (IRValue != IRValOLD) {
    IRValConstTime = millis();
    }

//TRIGGER - LEAVE DESK
  if (millis() - IRValConstTime >= 10000 && UsrMode != 1 && millis()-modetimer > 5000) {
    Serial.println("FUI");
    timeline = millis()-300000;
    //IRdelta = ((IRValue + IRValueOld)/2);
    //IRValueOld = IRValue ;
    desk = desk + ((millis()-modetimer)/1000);
    modetimer = millis();
    UsrMode = 1;
    IRValConstTime = millis();
    }

//TRIGGER - RETURN TO DESK
  if (IRValue != IRValOLD && UsrMode != 0 && millis()-modetimer > 5000) {
    UsrMode = 0;
    int seconds, hours, minutes;
    away = away + ((millis()-modetimer)/1000);
    modetimer = millis();
    Serial.print("BEMVINDO depois de "); Serial.print(away); Serial.println("s");
    seconds = away;
    minutes = int((seconds / 60)%60);
    hours = minutes / 60;
    minutes = int(minutes%60);
    seconds = int(seconds%60);
    Serial.print(hours); Serial.print("h, ");
    Serial.print(minutes); Serial.print("m e ");
    Serial.print(seconds); Serial.print("s.");
    Serial.println("");
    timeline = millis()-310000;
    }
//CLOCK REFRESH
  if (millis()-refreshTime > 60000) {
    timeClient.update();
    //Serial.println (timeClient.getEpochTime());
    refreshTime= millis();
    }

//WEATHER REFRESH
  if (millis()-refreshWeather > 600000) {
    HTTPClient http;
    http.begin(String(OpenWeatherCall) + OpenWeatherKey); //Specify the URL
    int httpCode = http.GET();  //Make the request
    if (httpCode > 0) { //Check for the returning code
      payload = http.getString();
      Serial.println(httpCode);
      Serial.println(payload);
      }
    else {
      Serial.println("Error on HTTP request");
      }
    
    http.end(); //Free the resources
    
    DynamicJsonDocument jsonBuffer(1024);
    DeserializationError error = deserializeJson(jsonBuffer, payload);
    if (error) {
      Serial.print("Deserialization failed with code: ");
      Serial.println(error.c_str());
      return;
      }
    JsonArray array = jsonBuffer["weather"].as<JsonArray>();
    temp = (float)(jsonBuffer["main"]["temp"]);
    time_t     rawtime = (jsonBuffer["dt"]);

    rawtime = rawtime -10800;
    ts = *localtime(&rawtime);
    strftime(buf, sizeof(buf), "%a %d-%m", &ts);
    printf("%s\n", buf);
    refreshWeather = millis();
    }

//TIMELINE - PRINT TO SCREEN
  if (timeNeedle >= 000 && timeNeedle < 7000) { //Print Time
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_chargen_92_mr);
    u8g2.drawFrame(0,0,128,32);
    u8g2.setCursor(16,24);
    u8g2.print (timeClient.getFormattedTime());
    //Serial.println (timeClient.getEpochTime());
    u8g2.sendBuffer();
    //Serial.println("time");
    }
  if (timeNeedle >= 7000 && timeNeedle < 9000) { //Print Date
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_chargen_92_mr);
    u8g2.drawFrame(0,0,128,32);
    u8g2.drawStr (10,24, buf);
    u8g2.sendBuffer();
    //Serial.println("date");
    }
  if (timeNeedle >= 9000 && timeNeedle < 10500) { //Print Temp
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_chargen_92_mr);
    u8g2.drawFrame(0,0,128,32);
    u8g2.setCursor(44,24);
    u8g2.print (temp);
    u8g2.setCursor(71,19);
    u8g2.print ("c");
    u8g2.sendBuffer();
    //Serial.println("temp");
    }
  if (timeNeedle >= 10500 && timeNeedle < 60000) { //Print Logo
    u8g2.clearBuffer();
    u8g2.drawFrame(0,0,128,32);
    u8g2.setFont(u8g2_font_helvR14_tr);
    u8g2.drawStr(16,30,"DeskBuddy");
    u8g2.sendBuffer();
    //Serial.println("logo");    
    }
  if (timeNeedle >= 300000 && timeNeedle <= 300500) { //Leave desk routine,Print Bye Bye
    u8g2.clearBuffer();
    u8g2.drawFrame(0,0,128,32);
    u8g2.setFont(u8g2_font_helvR14_tr);
    u8g2.drawStr(17,28,"Bye Buddy");
    u8g2.sendBuffer();
    timeline = millis();
    //Serial.println("Bye Bye");   
    //if (timeNeedle >= 300000 && timeNeedle <= 300500) { 
    //  timeline = millis();
    //  }  
    }
  if (timeNeedle >= 310000 && timeNeedle <= 310500) { //Return to desk Routine,Print Heya
    u8g2.clearBuffer();
    u8g2.drawFrame(0,0,128,32);
    u8g2.setFont(u8g2_font_helvR14_tr);
    u8g2.drawStr(13,28,"Heya Buddy");
    u8g2.sendBuffer();  
    }
  if (timeNeedle >= 311000 && timeNeedle <= 316000) { //Return to desk Routine cont.
    //away = ((millis()-away)/1000);
    int seconds, hours, minutes;
    seconds = away;
    minutes = int((seconds / 60)%60);
    hours = minutes / 60;
    minutes = int(minutes%60);
    seconds = int(seconds%60);
    u8g2.clearBuffer();
    u8g2.drawFrame(0,0,128,32);
    u8g2.setFont(u8g2_font_chargen_92_mr);
    //u8g2.drawStr(39,23,seconds);
    u8g2.drawStr(39,23,":  :");
    //u8g2.setCursor(36,24);    u8g2.print(away);
    u8g2.setCursor(36,24);    u8g2.print(hours);
    u8g2.setCursor(59,24);    u8g2.print(minutes);
    u8g2.setCursor(92,24);    u8g2.print(seconds);
    u8g2.sendBuffer();
    //Serial.println("Chegae Dan");   
    if (timeNeedle >= 315500 && timeNeedle <= 316000) {
      timeline = millis();
      }  
    }

  if (timeNeedle >= 10500 && timeNeedle <= 14000) { //End of Base Timeline - Return to start
      timeline = millis();
    }
  
  IRValOLD=IRValue;

//DEBUGGER
  Serial.print("IR:");
  Serial.print(IRValue);
  Serial.print(" - IRValConstTime :");
  Serial.print(IRValConstTime);
  Serial.print(" - IRcount:");
  Serial.print(IRCount);
  Serial.print(" - Modetimr:");
  Serial.print((millis()-modetimer)/1000);
  Serial.print(" - Away:");
  Serial.print(away);
  Serial.print(" - Desk:");
  Serial.print(desk);
  Serial.print(" - UsrMode:");
  Serial.print(UsrMode);
  Serial.print(" - Timeline:");
  Serial.print(timeNeedle/1000);
  Serial.print(" - WiFi:");
  wl_status_t wifiStatus = WiFi.status();
  Serial.print(getWiFiStatusName(wifiStatus));
  if (wifiStatus == WL_CONNECTED) {
    Serial.print("(");
    Serial.print(WiFi.RSSI());
    Serial.print("dBm, ");
    Serial.print(WiFi.localIP());
    Serial.print(")");
  }
  Serial.println();

//PACE
  delay (500);

}