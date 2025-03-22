#include <GxEPD2_BW.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
//#include <AsyncTCP.h>
#include <WebServer.h>
//#include <AsyncElegantOTA.h>
  #include <WiFiClient.h>

#include <ArduinoOTA.h>
#include <HTTPClient.h>

#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp32.h>
#include <SensirionI2CScd4x.h>
#include "kirby.h"
#include <Preferences.h>

Preferences preferences;

SensirionI2CScd4x scd4x;

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
sensors_event_t humidity, temp;
#include<Wire.h>
#define I2C_ADDRESS 0x48
#include "driver/periph_ctrl.h"
int GPIO_reason;
#include "esp_sleep.h"
#include <ElegantOTA.h>
#include <WiFiManager.h>      
WiFiManager wm;
 WebServer server2(8080);

const char* ssid = "mikesnet";
const char* password = "springchicken";
// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 1
#define TEMP_OFFSET 0.8
#define sleeptimeSecs 300
#define maxArray 501
#define controlpin 10
#define MENU_MAX 8
#define TIME_TIMEOUT 20000
#define ELEGANTOTA_USE_ASYNC_WEBSERVER 0
RTC_DATA_ATTR float array1[maxArray];
RTC_DATA_ATTR float array2[maxArray];
RTC_DATA_ATTR float array3[maxArray];
RTC_DATA_ATTR float array4[maxArray];
RTC_DATA_ATTR float windspeed, windgust, fridgetemp, outtemp;
 int  timetosleep = 5;
 bool isSetNtp = false;  
RTC_DATA_ATTR int  failcount = 0;
bool wifisaved = false;
bool rotateDisplay = false;
bool editrotate = false; 
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;  //Replace with your GMT offset (secs)
const int daylightOffset_sec = 0;   //Replace with your daylight offset (secs)
  float t, h, pres, barx;
  float v41_value, v42_value, v62_value;
char timeString[10]; // "12:34 PM" is 8 chars + null terminator
 RTC_DATA_ATTR   int firstrun = 100;
 RTC_DATA_ATTR   int page = 2;
 int calibTarget = 430;
float abshum;
 float minVal = 3.9;
 float maxVal = 4.2;
RTC_DATA_ATTR int readingCount = 0; // Counter for the number of readings
int readingTime;
int menusel = 1;
uint16_t co2;
float temp2, hum;
bool editinterval = false;
bool editcalib = false;
bool calibrated = false;
bool facreset = false;
bool wifireset = false;


#include <Fonts/Open_Sans_Condensed_Bold_36.h> 
#include <Fonts/FreeSans9pt7b.h> 

#define FONT1  //default
#define FONT2 &FreeSans9pt7b
#define FONT3 &Open_Sans_Condensed_Bold_36

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ SS, /*DC=*/ 21, /*RES=*/ 20, /*BUSY=*/ 3)); // DEPG0213BN 122x250, SSD1680

#define every(interval) \
    static uint32_t __every__##interval = millis(); \
    if (millis() - __every__##interval >= interval && (__every__##interval = millis()))


const char* blynkserver = "192.168.50.197:9443";
const char* bedroomauth = "8_-CN2rm4ki9P3i_NkPhxIbCiKd5RXhK";  //hubert
//const char* fridgeauth = "VnFlJdW3V0uZQaqslqPJi6WPA9LaG1Pk";

// Virtual Pins
const char* v41_pin = "V41";
const char* v62_pin = "V62";

float vBat;





WidgetTerminal terminal(V20);


BLYNK_WRITE(V20) {
  if (String("recal") == param.asStr()) {
    uint16_t error;
    scd4x.stopPeriodicMeasurement();
    terminal.println("Recalibrating to 400ppm...");
    scd4x.performForcedRecalibration(400, error);
    terminal.print("Adjusted by: ");
    terminal.println(error);
    terminal.flush();
    scd4x.startPeriodicMeasurement();
  }
  if (String("sleep") == param.asStr()) {
    terminal.println("");
    terminal.println("Going to sleep...");
    terminal.flush();
    gotosleep();
  }
  if (String("scd") == param.asStr()) {
    uint16_t error;
    char errorMessage[256];
    bool isDataReady = false;
    error = scd4x.getDataReadyFlag(isDataReady);
    if (error) {
      terminal.print("Error trying to execute getDataReadyFlag(): ");
      errorToString(error, errorMessage, 256);
      terminal.println(errorMessage);
      terminal.flush();
      return;
    }
    if (isDataReady) {
      error = scd4x.readMeasurement(co2, temp2, hum);
      if (error) {
        terminal.print("Error trying to execute readMeasurement(): ");
        errorToString(error, errorMessage, 256);
        terminal.println(errorMessage);
        terminal.flush();
      } else if (co2 == 0) {
        terminal.println("Invalid sample detected, skipping.");
        terminal.flush();
      } else {
        terminal.print("CO2: ");
        terminal.println(co2);

        terminal.print("Temp: ");
        terminal.println(temp2);

      }
    }
    terminal.flush();
  }
  if (param.asInt() > 300) {
    uint16_t error;
    int newppm = param.asInt();
    scd4x.stopPeriodicMeasurement();
    terminal.println("");
    terminal.print("Recalibrating to ");
    terminal.print(newppm);
    terminal.println("ppm.");
    
    scd4x.performForcedRecalibration(newppm, error);
    terminal.print("Adjusted by: ");
    terminal.println(32767 - error);
    terminal.flush();
    scd4x.startPeriodicMeasurement();
  }
    if (String("facreset") == param.asStr()) {
    scd4x.stopPeriodicMeasurement();
    scd4x.performFactoryReset();
    //scd4x.performForcedRecalibration(980, error);

    scd4x.startPeriodicMeasurement();
    }
  terminal.flush();
}




void gotosleep() {
      scd4x.stopPeriodicMeasurement();
      delay(10);
      scd4x.powerDown();
      delay(10);
      WiFi.disconnect();
      display.hibernate();
      //SPI.end();
      Wire.end();
      pinMode(SS, INPUT );
      pinMode(6, INPUT );
      pinMode(4, INPUT );
      pinMode(8, INPUT );
      pinMode(9, INPUT );
      pinMode(1, INPUT_PULLUP );
      pinMode(2, INPUT_PULLUP );
      pinMode(3, INPUT_PULLUP );
      pinMode(0, INPUT_PULLUP );
      pinMode(5, INPUT_PULLUP );
      digitalWrite(controlpin, LOW);
      pinMode(controlpin, INPUT);


      uint64_t bitmask = BUTTON_PIN_BITMASK(GPIO_NUM_0) | BUTTON_PIN_BITMASK(GPIO_NUM_1) | BUTTON_PIN_BITMASK(GPIO_NUM_2) | BUTTON_PIN_BITMASK(GPIO_NUM_3) |  BUTTON_PIN_BITMASK(GPIO_NUM_5);

      esp_deep_sleep_enable_gpio_wakeup(bitmask, ESP_GPIO_WAKEUP_GPIO_LOW);
      esp_sleep_enable_timer_wakeup((timetosleep * 60) * 1000000ULL);
      delay(1);
      esp_deep_sleep_start();
      //esp_light_sleep_start();
      delay(1000);
}

#include "esp_sntp.h"
void cbSyncTime(struct timeval *tv) { // callback function to show when NTP was synchronized
  Serial.println("NTP time synched");
  isSetNtp = true;
}

void initTime(String timezone){
  configTzTime(timezone.c_str(), "time.cloudflare.com", "pool.ntp.org", "time.nist.gov");

  /*while ((!isSetNtp) && (millis() < TIME_TIMEOUT)) {
        delay(1000);
        display.print("@");
        display.display(true);
        }*/

}

void drawBusy() {
  // Position just left of battery icon
  
  int xOffset = 5;
  int yOffset = 3;
  int x = 250 - 38 - 2 - xOffset - 15; // 15 pixels left of battery
  int y = 122 - 10 - yOffset;
  display.fillRect(x-1, y-1, 9, 11, GxEPD_WHITE);
  // Draw hourglass shape (8x10 pixels)
  display.drawLine(x, y, x+6, y, GxEPD_BLACK);       // Top line
  display.drawLine(x, y+8, x+6, y+8, GxEPD_BLACK);   // Bottom line
  display.drawLine(x, y, x+3, y+4, GxEPD_BLACK);     // Top left diagonal
  display.drawLine(x+6, y, x+3, y+4, GxEPD_BLACK);   // Top right diagonal
  display.drawLine(x, y+8, x+3, y+4, GxEPD_BLACK);   // Bottom left diagonal
  display.drawLine(x+6, y+8, x+3, y+4, GxEPD_BLACK); // Bottom right diagonal
  
  // Update only the hourglass area
  display.displayWindow(x-1, y-1, 9, 11);
}

void startWifi(){
  drawBusy();
   bool isDataReady = false;
   WiFi.mode(WIFI_STA);
  if (wifisaved && (failcount < 4)){

      WiFi.begin(wm.getWiFiSSID(), wm.getWiFiPass());
      WiFi.setTxPower (WIFI_POWER_8_5dBm);
      // Wait for connection
      while (WiFi.status() != WL_CONNECTED) {
        if (millis() > 10000) { 
          //display.print("!");
        }
        if (millis() > 20000) {
            failcount++;
            break;
          }
          //display.print(".");
          //display.display(true);
          delay(1000);
      }
      if (WiFi.status() == WL_CONNECTED) {
        //display.print("Connected. Getting time...");
        initTime("EST5EDT,M3.2.0,M11.1.0");
        Blynk.config(bedroomauth, IPAddress(216,110,224,105), 8080);
        Blynk.connect();
        while ((!Blynk.connected()) && (millis() < 20000)){
            delay(500);}
      }
  else
  {
    //display.print("Connection timed out. :(");
  }
  //display.display(true);
  
           
            scd4x.getDataReadyFlag(isDataReady);
  while(!isDataReady){delay(250);
  scd4x.getDataReadyFlag(isDataReady);}
  scd4x.readMeasurement(co2, temp2, hum);

      if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
                Blynk.virtualWrite(V91, t);
              if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
              Blynk.virtualWrite(V92, h);
              if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
              Blynk.virtualWrite(V93, pres);
              if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
              Blynk.virtualWrite(V94, abshum);
              if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
              Blynk.virtualWrite(V95, vBat);
              if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
              Blynk.virtualWrite(V96, co2);
              if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
              Blynk.virtualWrite(V97, temp2);
              if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
              Blynk.virtualWrite(V97, temp2);
              if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
        struct tm timeinfo;
        setenv("TZ","EST5EDT,M3.2.0,M11.1.0",1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
        tzset();
        getLocalTime(&timeinfo);
        time_t now = time(NULL);
      localtime_r(&now, &timeinfo);

      // Allocate a char array for the time string
      

      // Format the time string
      if (timeinfo.tm_min < 10) {
        snprintf(timeString, sizeof(timeString), "%d:0%d %s", timeinfo.tm_hour % 12 == 0 ? 12 : timeinfo.tm_hour % 12, timeinfo.tm_min, timeinfo.tm_hour < 12 ? "AM" : "PM");
      } else {
        snprintf(timeString, sizeof(timeString), "%d:%d %s", timeinfo.tm_hour % 12 == 0 ? 12 : timeinfo.tm_hour % 12, timeinfo.tm_min, timeinfo.tm_hour < 12 ? "AM" : "PM");
      }
  }
  else {
           
            scd4x.getDataReadyFlag(isDataReady);
  while(!isDataReady){delay(250);
  scd4x.getDataReadyFlag(isDataReady);}
  scd4x.readMeasurement(co2, temp2, hum);
  }


}

void startWebserver(){

  wipeScreen();

  if (wifisaved){
  display.setPartialWindow(0, 0, display.width(), display.height());
  
  display.setCursor(0, 0);

  
  display.print("Connecting...");
  display.display(true);
  WiFi.mode(WIFI_STA);
      
      WiFi.begin(wm.getWiFiSSID(), wm.getWiFiPass());

  //WiFi.mode(WIFI_STA);
  //WiFi.begin(ssid, password);  
  WiFi.setTxPower (WIFI_POWER_8_5dBm);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() > 20000) { break;}
    delay(1000);
  }
   if (WiFi.status() == WL_CONNECTED) {
    Blynk.config(bedroomauth, IPAddress(192, 168, 50, 197), 8080);
    Blynk.connect();
    while ((!Blynk.connected()) && (millis() < 20000)){delay(100);}
    
    wipeScreen();
    display.setCursor(0, 0);
    display.firstPage();
    do {
      display.print("Connected! to: ");
      display.println(WiFi.localIP());
    } while (display.nextPage());
    ArduinoOTA.setHostname("katieco2");
    ArduinoOTA.begin();

    server2.on("/", []() {
      server2.send(200, "text/plain", "Hi! This is ElegantOTA Demo.");
    });

    ElegantOTA.begin(&server2);    // Start ElegantOTA
  
    server2.begin();
   }
  }
  pinMode(0, INPUT);
  vBat = analogReadMilliVolts(0) / 500.0;
  pinMode(0, INPUT_PULLUP);
  bmp.takeForcedMeasurement();
  aht.getEvent(&humidity, &temp);
  t = temp.temperature;
  h = humidity.relative_humidity;
  pres = bmp.readPressure() / 100.0;
  /*abshum = (6.112 * pow(2.71828, ((17.67 * temp.temperature)/(temp.temperature + 243.5))) * humidity.relative_humidity * 2.1674)/(273.15 + temp.temperature);
  bool isDataReady = false;
  scd4x.getDataReadyFlag(isDataReady);
  if (isDataReady) {
  scd4x.readMeasurement(co2, temp2, hum);}*/
  displayMenu();
}

void displayMenu(){
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.setTextSize(1);
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 0);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
   if (WiFi.status() == WL_CONNECTED) {
  display.print("Connected! to: "); //http://moeburn.mooo.com/public-dashboards/e3bb4dccb20d426ba449c2842d306c2b
  display.println(WiFi.localIP());
  display.print("RSSI: ");
  display.println(WiFi.RSSI());
  display.println("");
   }
   else {display.setCursor(0, 8*3);}
    if (menusel == 1) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.println("Start WifiManager");
    if (menusel == 2) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.println("Change Interval");
    if (menusel == 3) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.println("Set Calibration Value");
    if (menusel == 4) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.println("Calibrate");
    if (menusel == 5) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.println("Factory Reset SCD41");
    if (menusel == 6) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.println("Forget Wifi");
    if (menusel == 7) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.println("Rotate?");
    if (menusel == 8) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.println("Exit");

    display.setCursor(200, 8*4); 
    if (editinterval) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.print(timetosleep);
    display.println(" mins");
    display.setCursor(200, 8*5); 
    if (editcalib) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.print(calibTarget);
    display.println(" ppm");
    
    display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
    display.setCursor(200, 8*6); 
    if (calibrated) {display.println("DONE!");}
    display.setCursor(200, 8*7); 
    if (facreset) {display.println("Reset!");}
    display.setCursor(200, 8*8); 
    if (wifireset) {display.println("Reset!");}
    display.setCursor(200, 8*9); 
    if (editrotate) {display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);} else {display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);}
    display.print(rotateDisplay ? "Yes" : "No");
    display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.setCursor(0, 106); 
  display.print("CO2: ");
  display.print(co2);
  display.print("ppm | Temp: ");
  display.print(t);
  display.print((char)247);
  display.print("c | Hum: ");
  display.print(h);
  display.print("%");
  display.setCursor(0, 114); 
  display.print("Pres: ");
  display.print(pres);
  display.print("hPa | vBat: ");
  display.print(vBat);
  display.print("v / ");
  int batPct = mapf(vBat, 3.3, 4.05, 0, 100);
  display.print(batPct);
  display.print("% [10s]");
   display.display(true);
}



void wipeScreen(){

    display.setPartialWindow(0, 0, display.width(), display.height());

    display.firstPage();
    do {
      display.fillRect(0,0,display.width(),display.height(),GxEPD_BLACK);
    } while (display.nextPage());
    delay(10);
    display.firstPage();
    do {
      display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
    } while (display.nextPage());
    display.firstPage();

}

void batCheck() {
  if (vBat < 3.4){
  display.fillRect(0, 0, display.width(), display.height(), GxEPD_BLACK);
  display.setTextColor(GxEPD_WHITE, GxEPD_BLACK);
  display.setCursor(10,60);
  display.setFont(FONT3);
display.setTextSize(1);
  display.print("LOW BATTERY");
  display.display(true);
  gotosleep();
 }
}

void setupChart(){
        
        display.setCursor(0, 0);
        display.print("<");
        display.print(maxVal, 3);
        display.setCursor(0, 114);
        display.print("<");
        display.print(minVal, 3);
        display.setCursor(110, 114);
        display.print("<--");
        display.print(readingCount - 1, 0);
        display.print("*");
        display.print(timetosleep * 60, 0);
        display.print("s-->");
        vBat = analogReadMilliVolts(0) / 500.0;
        barx = mapf (vBat, 3.3, 4.05, 0, 19);
        display.drawRect(229,114,19,7,GxEPD_BLACK);
        display.fillRect(229,114,barx,7,GxEPD_BLACK); 
        display.drawLine(248,115,248,119,GxEPD_BLACK);
        display.drawLine(249,115,249,119,GxEPD_BLACK);
        display.setCursor(125, 0);
}

void setupChart2(){
        
        display.setCursor(0, 0);
        display.print("<");
        display.print(maxVal, 0);
        display.setCursor(0, 114);
        display.print("<");
        display.print(minVal, 0);
        display.setCursor(110, 114);
        display.print("<--");
        display.print(readingCount - 1, 0);
        display.print("*");
        display.print(timetosleep * 60, 0);
        display.print("s-->");
        vBat = analogReadMilliVolts(0) / 500.0;
        barx = mapf (vBat, 3.3, 4.05, 0, 19);
        display.drawRect(229,114,19,7,GxEPD_BLACK);
        display.fillRect(229,114,barx,7,GxEPD_BLACK); 
        display.drawLine(248,115,248,119,GxEPD_BLACK);
        display.drawLine(249,115,249,119,GxEPD_BLACK);
        display.setCursor(125, 0);
}

double mapf(float x, float in_min, float in_max, float out_min, float out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void doTempChart() {
    // Recalculate min and max values
     minVal = array1[maxArray - readingCount];
     maxVal = array1[maxArray - readingCount];

    for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
        if (array1[i] != 0) {  // Only consider non-zero values
            if (array1[i] < minVal) {
                minVal = array1[i];
            }
            if (array1[i] > maxVal) {
                maxVal = array1[i];
            }
        }
    }

    // Calculate scaling factors
    float yScale = 121.0 / (maxVal - minVal);
    float xStep = 250.0 / (readingCount - 1);

    wipeScreen();
    
    do {
        display.fillRect(0, 0, display.width(), display.height(), GxEPD_WHITE);
        
        for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 121 - ((array1[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 121 - ((array1[i + 1] - minVal) * yScale);

            // Only draw a line for valid (non-zero) values
            if (array1[i] != 0) {
                display.drawLine(x0, y0, x1, y1, GxEPD_BLACK);
            }
        }
        setupChart();
        display.print("[");
        display.print("Temp: ");
        display.print(array1[(maxArray - 1)], 3);
        display.print((char)247);
        display.print("c");
        display.print("]");
    } while (display.nextPage());

    display.setFullWindow();

}

void doCO2Chart() {
    // Recalculate min and max values
     minVal = array2[maxArray - readingCount];
     maxVal = array2[maxArray - readingCount];

    for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
        if ((array2[i] < minVal) && (array2[i] > 0)) {
            minVal = array2[i];
        }
        if (array2[i] > maxVal) {
            maxVal = array2[i];
        }
    }

    // Calculate scaling factors
    float yScale = 121.0 / (maxVal - minVal);
    float xStep = 250.0 / (readingCount - 1);

    wipeScreen();
    
    
    do {
        display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
        
        for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 121 - ((array2[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 121 - ((array2[i + 1] - minVal) * yScale);
            if (array2[i] > 0) {
                display.drawLine(x0, y0, x1, y1, GxEPD_BLACK);
            }
        }
        setupChart2();
        display.print("[");
        display.print("CO2: ");
        display.print(array2[(maxArray - 1)], 0);
        display.print("ppm");
        display.print("]");
    } while (display.nextPage());

    display.setFullWindow();

}

void doMainDisplay() {        
  wipeScreen();
  updateMain();

}

void doPresDisplay() {
    // Recalculate min and max values
     minVal = array3[maxArray - readingCount];
     maxVal = array3[maxArray - readingCount];

    for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
        if ((array3[i] < minVal) && (array3[i] > 0)) {
            minVal = array3[i];
        }
        if (array3[i] > maxVal) {
            maxVal = array3[i];
        }
    }

    // Calculate scaling factors
    float yScale = 121.0 / (maxVal - minVal);
    float xStep = 250.0 / (readingCount - 1);

    wipeScreen();
    
    
    do {
        display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
        
        for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 121 - ((array3[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 121 - ((array3[i + 1] - minVal) * yScale);
            if (array3[i] > 0) {
                display.drawLine(x0, y0, x1, y1, GxEPD_BLACK);
            }
        }
        setupChart();
        display.print("[");
        display.print("Pres: ");
        display.print(array3[(maxArray - 1)], 2);
        display.print("mb");
        display.print("]");
    } while (display.nextPage());

    display.setFullWindow();

}

void doBatChart() {
    // Recalculate min and max values
     minVal = array4[maxArray - readingCount];
     maxVal = array4[maxArray - readingCount];

    for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
        if ((array4[i] < minVal) && (array4[i] > 0)) {
            minVal = array4[i];
        }
        if (array4[i] > maxVal) {
            maxVal = array4[i];
        }
    }

    // Calculate scaling factors
    float yScale = 121.0 / (maxVal - minVal);
    float xStep = 250.0 / (readingCount - 1);

    wipeScreen();
    
    
    do {
        display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);

        
        for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 121 - ((array4[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 121 - ((array4[i + 1] - minVal) * yScale);
            if (array4[i] > 0) {
                display.drawLine(x0, y0, x1, y1, GxEPD_BLACK);
            }
        }
        display.setCursor(0, 0);
        display.print("<");
        display.print(maxVal, 3);
        display.setCursor(0, 114);
        display.print("<");
        display.print(minVal, 3);
        display.setCursor(120, 114);
        display.print("<#");
        display.print(readingCount - 1, 0);
        display.print("*");
        display.print(timetosleep * 60, 0);
        display.print("s>");
        display.setCursor(175, 114);
        vBat = analogReadMilliVolts(0) / 500.0;
        int batPct = mapf(vBat, 3.3, 4.05, 0, 100);
        display.setCursor(125, 0);
        display.print("[vBat: ");
        display.print(vBat, 3);
        display.print("v/");
        display.print(batPct, 1);
        display.print("%]");
    } while (display.nextPage());

    display.setFullWindow();

}


void takeSamples(){
        for (int i = 0; i < (maxArray - 1); i++) {
            array3[i] = array3[i + 1];
        }
        array3[(maxArray - 1)] = pres;

        if (readingCount < maxArray) {
            readingCount++;
        }

        for (int i = 0; i < (maxArray - 1); i++) {
            array1[i] = array1[i + 1];
        }
        array1[(maxArray - 1)] = t;

        for (int i = 0; i < (maxArray - 1); i++) {
            array2[i] = array2[i + 1];
        }
        array2[(maxArray - 1)] = co2;

        for (int i = 0; i < (maxArray - 1); i++) {
            array4[i] = array4[i + 1];
        }
        array4[(maxArray - 1)] = vBat;
}

void updateMain(){

    int yadj = 25;
    int yadj2 = 12;
    int fontsize1 = 1;
    display.setPartialWindow(0, 0, display.width(), display.height());
    display.fillScreen(GxEPD_WHITE);
        float co2todraw = array2[(maxArray - 1)];
        float temptodraw = array1[(maxArray - 1)];
        
        display.drawLine(122, 0, 122, 122, GxEPD_BLACK);
        display.drawLine(123, 0, 123, 122, GxEPD_BLACK);
        display.drawLine(0, 60, 250, 60, GxEPD_BLACK);
        display.drawLine(0, 61, 250, 61, GxEPD_BLACK);

        display.setFont(FONT2);
        display.setCursor(32,2+yadj2);
        display.print(" CO2");
        display.setCursor(150,2+yadj2);
        display.print(" Temp");
        display.setCursor(8,64+yadj2);
        display.print("  Humidity");
        display.setCursor(136,64+yadj2);
        display.print("  Pressure");

        display.setFont(FONT3);
        display.setTextSize(1);
        display.setCursor(18,26+yadj);
        if ((co2todraw > 0) && (co2todraw < 1000)) {display.print(" ");}
        display.print(co2todraw, 0);
        display.setFont(FONT1);
        display.setTextSize(fontsize1);
        display.print("ppm");


        
        display.setFont(FONT3);
        display.setTextSize(1);
        display.setCursor(155,26+yadj);

        display.print(temptodraw, 1);
        display.setFont(FONT1);
        display.setTextSize(fontsize1);
        display.print((char)247);
        display.print("c");


        display.setFont(FONT3);
        display.setTextSize(1);
        display.setCursor(20,87+yadj);
        display.print(h, 1);
        display.setFont(FONT1);
        display.setTextSize(fontsize1);
        display.print("%");
        display.setTextSize(1);
        display.setCursor(0, 115);
        display.setFont(FONT1);
        
        if (WiFi.status() == WL_CONNECTED) {display.print(timeString); display.print(", ");}
        display.print(timetosleep);
        display.print("mins");

        display.setFont(FONT3);
        display.setTextSize(1);

        display.setCursor(155,87+yadj);
        display.print(pres, 0);
        display.setFont(FONT1);
        display.setTextSize(fontsize1);
        display.print("hPa");
        display.setFont(FONT3);
        display.setTextSize(1);
        barx = mapf (vBat, 3.3, 4.05, 0, 19);
        if (barx > 19) {barx = 19;}
        display.drawRect(229,114-0,19,7,GxEPD_BLACK);
        display.fillRect(229,114-0,barx,7,GxEPD_BLACK); 
        display.drawLine(248,115-0,248,119-0,GxEPD_BLACK);
        display.drawLine(249,115-0,249,119-0,GxEPD_BLACK);

        //display.drawRect(0,0,display.width(), display.height(), GxEPD_BLACK);
        display.setFont(FONT1);
        display.display(true);
}

void setup(){
  vBat = analogReadMilliVolts(0) / 500.0;
  batCheck();
  barx = mapf (vBat, 3.3, 4.05, 0, 19);
  if (barx > 19) {barx = 19;}
  GPIO_reason = log(esp_sleep_get_gpio_wakeup_status())/log(2);
  preferences.begin("my-app", false);
    timetosleep = preferences.getUInt("timetosleep", 5);
    wifisaved = preferences.getBool("wifisaved", false);
    rotateDisplay = preferences.getBool("rotate", false);
  preferences.end();
  Wire.begin();  
  scd4x.begin(Wire);
  scd4x.wakeUp();
  scd4x.stopPeriodicMeasurement();
  uint16_t error;
  //scd4x.performFactoryReset();
  float toff;
  if (scd4x.getTemperatureOffset(toff) != (4 - TEMP_OFFSET))
    {scd4x.setTemperatureOffset(4 - TEMP_OFFSET);}
  scd4x.startPeriodicMeasurement();
  aht.begin();
  bmp.begin();
  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500);
  bmp.takeForcedMeasurement();
  
  aht.getEvent(&humidity, &temp);
   t = temp.temperature;
   h = humidity.relative_humidity;
    pres = bmp.readPressure() / 100.0;
   uint16_t pres1 = pres;
   scd4x.setAmbientPressure(pres1);
   
    abshum = (6.112 * pow(2.71828, ((17.67 * temp.temperature)/(temp.temperature + 243.5))) * humidity.relative_humidity * 2.1674)/(273.15 + temp.temperature);

  //configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  pinMode(controlpin, OUTPUT);
  digitalWrite(controlpin, HIGH);
  display.init(115200, false, 10, false); // void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 10, bool pulldown_rst_mode = false)
  display.setRotation(rotateDisplay ? 3 : 1);
  display.setTextSize(1);
  pinMode(0, INPUT_PULLUP );
  pinMode(1, INPUT_PULLUP );
  pinMode(2, INPUT_PULLUP );
  //pinMode(3, INPUT_PULLUP );
  //pinMode(4, INPUT_PULLUP );
  pinMode(5, INPUT_PULLUP );


   



            
  if (firstrun >= 1000) {display.clearScreen();
  firstrun = 0;}
  firstrun++;

  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  



  if (GPIO_reason < 0) {

    startWifi();
    takeSamples();
      switch (page){
        case 0: 
          doTempChart();
          break;
        case 1: //down
          doTempChart();  
          break;
        case 2: //towards
          doMainDisplay(); 
          break;
        case 3:  //away
          doCO2Chart(); 
          break;
        case 4: //up
          doBatChart(); 
          break;
      }    
      gotosleep();  
    }
  switch (GPIO_reason) {
    case 1: 
      page = 1;
      doTempChart();
      break;
    case 2: 
      page = 2;
      doMainDisplay();
      break;
    case 3: 
      page = 3;
      doCO2Chart();
      break;
    case 0: 
      page = 4;
      doBatChart();
      break;
    case 5: 
    delay(50);
      while (!digitalRead(5))
        {
          delay(10);
          if (millis() > 2000) {
            while (!digitalRead(5)){delay(1);}
            startWebserver();
          return;}
        }
      wipeScreen();
      display.setPartialWindow(0, 0, display.width(), display.height());
      startWifi();
      takeSamples();
      //display.clearScreen();
      switch (page){
        case 0: 
          doTempChart();
          break;
        case 1: 
          doTempChart();
          break;
        case 2: 
          doMainDisplay();
          break;
        case 3: 
          doCO2Chart();
          break;
        case 4: 
          doBatChart();
          break;
      }
  }
  gotosleep();  
  

}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    server2.handleClient();
    ElegantOTA.loop();
  }
  if (Blynk.connected()) {Blynk.run();}


  if (!digitalRead(5)) {
      switch (menusel) {
        case 1:
          { 
                display.setPartialWindow(0, 0, display.width(), display.height());
                wipeScreen();
                display.setCursor(0, 0);                                  //
                display.println("Use your mobile phone to connect to ");
                display.println("[CO2 Setup] then browse to");
                display.println("http://192.168.4.1 to connect to WiFi");
                display.display(true);
                failcount = 0;
                WiFi.mode(WIFI_STA);
                WiFi.setTxPower (WIFI_POWER_8_5dBm);
                wm.setConfigPortalTimeout(300);
                  bool res;
                  res = wm.autoConnect("CO2 Setup");
                  if (res) {
                      wifisaved = true;
                      preferences.begin("my-app", false);
                      preferences.putBool("wifisaved", wifisaved);
                      preferences.end();
                    }
                  
                displayMenu();
                break; 
          }
        case 2: 
            editinterval = !editinterval;
            preferences.begin("my-app", false);
            preferences.putUInt("timetosleep", timetosleep);
            preferences.end();
            displayMenu();
            break; 
        case 3: 
            editcalib = !editcalib;
            displayMenu();
            break;
        case 4: 
            scd4x.stopPeriodicMeasurement();
            uint16_t error;
            scd4x.performForcedRecalibration(calibTarget, error);
            scd4x.startPeriodicMeasurement();
            calibrated = true;
            displayMenu();
            break; 
        case 5:    
            scd4x.stopPeriodicMeasurement();
            scd4x.performFactoryReset();
            scd4x.startPeriodicMeasurement();
            facreset = true;
            break;  
        case 6:    
            wm.resetSettings();
            wifireset = true;
            wifisaved = false;
            preferences.begin("my-app", false);
            preferences.putBool("wifisaved", wifisaved);
            preferences.end();
            break;  
        case 7:
            editrotate = !editrotate;
            preferences.begin("my-app", false);
            preferences.putBool("rotate", rotateDisplay);
            preferences.end();
            displayMenu();
            break;
        case 8: 
            ESP.restart();
            break; 
      }
    }
  every (100){

    if (!digitalRead(0)) {
      calibrated = false;
      facreset = false;
      wifireset = false;
      if (editrotate) {
          rotateDisplay = !rotateDisplay;
      } else if (editinterval) {
          timetosleep += rotateDisplay ? 1 : -1;
      } else if (editcalib) {
          calibTarget += rotateDisplay ? 5 : -5;
      } else {
          menusel += rotateDisplay ? -1 : 1;
      }
      if (menusel > MENU_MAX) {menusel = 1;}
      if (menusel < 1) {menusel = MENU_MAX;}
      if (timetosleep < 1) {timetosleep = 1;}
      if (timetosleep > 999) {timetosleep = 999;}
      displayMenu();
    }
    if (!digitalRead(1)) {
      calibrated = false;
      facreset = false;
      wifireset = false;
      if (editrotate) {
          rotateDisplay = !rotateDisplay;
      } else if (editinterval) {
          timetosleep += rotateDisplay ? -1 : 1;
      } else if (editcalib) {
          calibTarget += rotateDisplay ? -5 : 5;
      } else {
          menusel += rotateDisplay ? 1 : -1;
      }
      if (menusel > MENU_MAX) {menusel = 1;}
      if (menusel < 1) {menusel = MENU_MAX;}
      if (timetosleep < 1) {timetosleep = 1;}
      if (timetosleep > 999) {timetosleep = 999;}
      displayMenu();
    }
    if (!digitalRead(2)) {
      if (menusel == 7) {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, kirby, 250, 122, GxEPD_BLACK);
        display.display(true);
        delay(1000);
        gotosleep();
      }
    }
  }

  every (10000) {
    pinMode(0, INPUT);
    vBat = analogReadMilliVolts(0) / 500.0;
    pinMode(0, INPUT_PULLUP);
    bmp.takeForcedMeasurement();
    aht.getEvent(&humidity, &temp);
    t = temp.temperature;
    h = humidity.relative_humidity;
    pres = bmp.readPressure() / 100.0;
    abshum = (6.112 * pow(2.71828, ((17.67 * temp.temperature)/(temp.temperature + 243.5))) * humidity.relative_humidity * 2.1674)/(273.15 + temp.temperature);
    bool isDataReady = false;
    scd4x.getDataReadyFlag(isDataReady);
    if (isDataReady) {
    scd4x.readMeasurement(co2, temp2, hum);}
    if (Blynk.connected()) {
              Blynk.virtualWrite(V91, t);
              Blynk.virtualWrite(V92, h);
              Blynk.virtualWrite(V93, pres);
              Blynk.virtualWrite(V94, abshum);
              Blynk.virtualWrite(V95, vBat);
              Blynk.virtualWrite(V96, co2);
              Blynk.virtualWrite(V97, temp2);
    }
    displayMenu();
  }

  if (millis() > 30*60*1000) {ESP.restart();} //30 minute timeout
}
