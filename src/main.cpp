#include <Arduino.h>
#include <U8g2lib.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

const char *ssid     = "MARINA";
const char *password = "marina.br";
const String endpoint = "https://api.openweathermap.org/data/2.5/weather?lat=-23.53&units=metric&lon=-46.67&leng=fr&appid=";
const String key = "12afe2bff954a255506fd24c6b17425f";

struct tm  ts;
char buf[80];
int temp;

int UsrMode;

u_long modetimer = 10000;
u_long away = 0;
u_long desk = 0;

//IR VARIABLES
const int IRpin = 34;
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

//SETUP FUNCTION
void setup(void) {
  Serial.begin(115200);
  u8g2.begin();
  WiFi.begin(ssid, password);
  //Serial.println(payload);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  timeClient.begin();
  timeClient.setTimeOffset(-10800);
}

//LOOP FUNCTION
void loop(void) {

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
    http.begin(endpoint + key); //Specify the URL
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
  Serial.println(timeNeedle/1000);

//PACE
  delay (500);

}