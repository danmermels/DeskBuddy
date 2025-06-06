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

float temp;
const int IRpin = 34;
int IRValue = 0;

String payload = "empty";


//TIMERS
u_long refreshTime = 30000;
u_long refreshWeather = 30000;
int timeline;

//CLIENTS
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//DISPLAY CONSTRUCTOR
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0); 

void setup(void) {
  Serial.begin(115200);
  u8g2.begin();
  WiFi.begin(ssid, password);
  Serial.println(payload);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  timeClient.begin();
  timeClient.setTimeOffset(-10800);
}
//MAIN LOOP
void loop(void) {

//IRValue = analogRead(IRpin);
Serial.println(IRValue);

int timer = millis()-timeline;

//CLOCK REFRESH
  if (millis()-refreshTime > 60000) {
    timeClient.update();
    Serial.println (timeClient.getEpochTime());
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

//PRINT TO SCREEN

  if (timer >= 000 && timer < 5000) {
    
    u8g2.clearBuffer();					// clear the internal memory
    u8g2.setFont(u8g2_font_chargen_92_mr);	// choose a suitable font
    u8g2.drawFrame(0,0,128,32);
    u8g2.setCursor(16,23);
    u8g2.print (timeClient.getFormattedTime()); //(10,24, buf);
    Serial.println (timeClient.getEpochTime());
    u8g2.sendBuffer();					// transfer internal memory to the display
    Serial.println("time");
    }
  if (timer >= 5000 && timer < 8000) {

    u8g2.clearBuffer();					// clear the internal memory
    u8g2.setFont(u8g2_font_chargen_92_mr);	// choose a suitable font
    u8g2.drawFrame(0,0,128,32);
    u8g2.drawStr (10,24, buf);
    u8g2.sendBuffer();					// transfer internal memory to the display
    Serial.println("date");
    }
  if (timer >= 8000 && timer < 10000) {
    u8g2.clearBuffer();					// clear the internal memory
    u8g2.setFont(u8g2_font_chargen_92_mr);	// choose a suitable font
    u8g2.drawFrame(0,0,128,32);
    u8g2.setCursor(36,24);
    u8g2.print (temp);	// write something to the internal memory
    u8g2.sendBuffer();					// transfer internal memory to the display
    Serial.println("temp");
    }
  if (timer >= 10000 && timer < 60000) {
    u8g2.clearBuffer();					// clear the internal memory
    u8g2.drawFrame(0,0,128,32);
    u8g2.setFont(u8g2_font_helvR14_tr);	// choose a suitable font
    u8g2.drawStr(16,30,"DeskBuddy");	// write something to the internal memory
    u8g2.sendBuffer();					// transfer internal memory to the display
    Serial.println("logo");    
    }    
  if (timer >= 12000) {
      timeline = millis();
    }

  Serial.println(timer);
  delay (1000); 
}