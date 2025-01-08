#include <Arduino.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "time.h"
#include <SensirionI2CScd4x.h>
#include <Wire.h>
#include <GxEPD2_BW.h>
#include "driver/periph_ctrl.h"

#define maxArray 750
//#define controlpin 10
#define TEMP_OFFSET 0.8
#define SLEEPTIME_SECS 120
#define switch1pin 0

int newVal;
float volts;

RTC_DATA_ATTR float co20[maxArray];
RTC_DATA_ATTR float volts0[maxArray];
 RTC_DATA_ATTR   int firstrun = 0;

RTC_DATA_ATTR int readingCount = 0; // Counter for the number of readings
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=5*/ SS, /*DC=*/ 1, /*RES=*/ 2, /*BUSY=*/ 3)); // GDEH0154D67 200x200, SSD1681

SensirionI2CScd4x scd4x;

const char* ssid = "mikesnet";
const char* password = "springchicken";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;  //Replace with your GMT offset (secs)
const int daylightOffset_sec = 0;   //Replace with your daylight offset (secs)
int hours, mins, secs;
uint16_t co2;
float temp, hum, abshum;
float toff;

#include <Fonts/FreeSans12pt7b.h> 
#include <Fonts/Roboto_Condensed_12.h>

char auth[] = "8gJkMOvx8u5vKCVbjsAheg-gL9mp64Cg";

void gotosleep(int sleeptimeSecs) {
  Serial.println("1");
  scd4x.stopPeriodicMeasurement();
  Serial.println("2");
      WiFi.disconnect();
      Serial.println("3");
      display.hibernate();
      Serial.println("4");
      SPI.end();
      Serial.println("5");
      Wire.end();
      Serial.println("6");
  pinMode(SS, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  //  pinMode(I2C_PIN, INPUT );
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);

  pinMode(5, INPUT_PULLUP);


            esp_deep_sleep_enable_gpio_wakeup(1 << switch1pin, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_sleep_enable_timer_wakeup(sleeptimeSecs * 1000000ULL);
  delay(1);
  esp_deep_sleep_start();
  //esp_light_sleep_start();
  delay(1000);
}



WidgetTerminal terminal(V10);

#define every(interval) \
  static uint32_t __every__##interval = millis(); \
  if (millis() - __every__##interval >= interval && (__every__##interval = millis()))

BLYNK_WRITE(V10) {
  if (String("help") == param.asStr()) {
    terminal.println("==List of available commands:==");
    terminal.println("wifi");
    terminal.println("recal");
    terminal.println("scd");
    terminal.println("sleep");
    terminal.println("==End of list.==");
  }
  if (String("wifi") == param.asStr()) {
    terminal.print("Connected to: ");
    terminal.println(ssid);
    terminal.print("IP address:");
    terminal.println(WiFi.localIP());
    terminal.print("Signal strength: ");
    terminal.println(WiFi.RSSI());
    printLocalTime();
  }
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
    sleep(SLEEPTIME_SECS);
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
      error = scd4x.readMeasurement(co2, temp, hum);
      if (error) {
        terminal.print("Error trying to execute readMeasurement(): ");
        errorToString(error, errorMessage, 256);
        terminal.println(errorMessage);
        terminal.flush();
      } else if (co2 == 0) {
        terminal.println("Invalid sample detected, skipping.");
        terminal.flush();
      } else {
        float abshum = (6.112 * pow(2.71828, ((17.67 * temp) / (temp + 243.5))) * hum * 2.1674) / (273.15 + temp);
        terminal.print("CO2: ");
        terminal.println(co2);

        terminal.print("Temp: ");
        terminal.println(temp);

        terminal.print("Hum: ");
        terminal.println(hum);

        terminal.print("Abshum: ");
        terminal.println(abshum);
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
  terminal.flush();
}

bool buttonstart = false;

BLYNK_WRITE(V11)
{
  if (param.asInt() == 1) {buttonstart = true;}
  if (param.asInt() == 0) {buttonstart = false;}
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V11);
}


void printLocalTime() {
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  terminal.print(asctime(timeinfo));
}

void printUint16Hex(uint16_t value) {
  terminal.print(value < 4096 ? "0" : "");
  terminal.print(value < 256 ? "0" : "");
  terminal.print(value < 16 ? "0" : "");
  terminal.print(value, HEX);
  terminal.flush();
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
  terminal.print("Serial: 0x");
  printUint16Hex(serial0);
  printUint16Hex(serial1);
  printUint16Hex(serial2);
  terminal.println();
  terminal.flush();
}

double mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void doChart() {
       //newVal = ads.computeVolts(ads.readADC_SingleEnded(0)) * 2.0;
    
    // Shift the previous data points


    // Recalculate min and max values
    float minVal = co20[maxArray - readingCount];
    float maxVal = co20[maxArray - readingCount];

    for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
        if ((co20[i] < minVal) && (co20[i] > 0)) {
            minVal = co20[i];
        }
        if (co20[i] > maxVal) {
            maxVal = co20[i];
        }
    }

    // Calculate scaling factors
    float yScale = 199.0 / (maxVal - minVal);
    float xStep = 199.0 / (readingCount - 1);

    // Draw the line chart
   // display.firstPage();
  //  do {
   // display.fillScreen(GxEPD_WHITE);
  //  } while (display.nextPage());
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

    int startIndex = 0;
    for (int i = 0; i < maxArray; i++) {
        if (co20[i] != 0) {
            startIndex = i;
            break;
        }
    }
    
    
    display.firstPage();
    do {
        display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
        display.setCursor(0, 9);
        display.print(maxVal, 0);
        display.setCursor(0, 190);
        display.print(minVal, 0);
        display.setCursor(100, 9);
        display.print(">");
        display.print(co2, 0);
        display.print("<");
        
        display.setCursor(100, 190);
        display.print("#");
        display.print(readingCount);
        display.print("*");
        display.print(SLEEPTIME_SECS);
        
        for (int i = maxArray - (readingCount); i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 199 - ((co20[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 199 - ((co20[i + 1] - minVal) * yScale);
            //if (co20[i] > 0) {
                display.drawLine(x0, y0, x1, y1, GxEPD_BLACK);
            //}
        }
    } while (display.nextPage());

    display.setFullWindow();

}

void setup(void) {
  pinMode(switch1pin, INPUT_PULLUP);

  Wire.begin();
  scd4x.begin(Wire);
  uint16_t error;
  char errorMessage[256];
  error = scd4x.stopPeriodicMeasurement();

if (scd4x.getTemperatureOffset(toff) != (4 - TEMP_OFFSET))
  //terminal.print("Temperature offset: ");
  //terminal.println(toff);
  {scd4x.setTemperatureOffset(4 - TEMP_OFFSET);}
  //terminal.print("New Temperature offset: ");
  //scd4x.getTemperatureOffset(toff);
  //terminal.println(toff);
  // Start Measurement
  error = scd4x.startPeriodicMeasurement();

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");  
unsigned long time1, time2;
  while ((WiFi.status() != WL_CONNECTED) && (millis() < 20000)) {
    delay(500);
    if (millis() > 15000) {
      WiFi.setTxPower(WIFI_POWER_8_5dBm);
      Serial.print("!");
    }
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ArduinoOTA.setHostname("scd41test");
  ArduinoOTA.begin();
  Serial.println("HTTP server started");
  delay(250);
  //configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Blynk.config(auth, IPAddress(192, 168, 50, 197), 8080);
  Blynk.connect();
  while ((!Blynk.connected()) && (millis() < 20000)) {
    Serial.print(".");
    delay(250);
  }
  time1 = millis();
  delay(250);
  //struct tm timeinfo;
  //getLocalTime(&timeinfo);
  //hours = timeinfo.tm_hour;
  //mins = timeinfo.tm_min;
  //secs = timeinfo.tm_sec;

  //terminal.flush();


  //terminal.println("Waiting for first measurement... (5 sec)");
  //terminal.flush();
     // uint16_t error;
    //char errorMessage[256];
    bool isDataReady = false;
    error = scd4x.getDataReadyFlag(isDataReady);
    if (error) {
      terminal.print("Error trying to execute getDataReadyFlag(): ");
      errorToString(error, errorMessage, 256);
      terminal.println(errorMessage);
      terminal.flush();
      return;
    }
    while(!isDataReady){delay(250);
    Serial.println("waiting for data...");
    error = scd4x.getDataReadyFlag(isDataReady);}
    time2 = millis() - time1;
    error = scd4x.readMeasurement(co2, temp, hum);
      if (error) {
        terminal.print("Error trying to execute readMeasurement(): ");
        errorToString(error, errorMessage, 256);
        terminal.println(errorMessage);
        terminal.flush();
      } else if (co2 == 0) {
        terminal.println("Invalid sample detected, skipping.");
        terminal.flush();
      } else {
        float abshum = (6.112 * pow(2.71828, ((17.67 * temp) / (temp + 243.5))) * hum * 2.1674) / (273.15 + temp);
        Blynk.virtualWrite(V1, co2);
        if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
        Blynk.virtualWrite(V2, temp);
        if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
        Blynk.virtualWrite(V3, hum);
        if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
        Blynk.virtualWrite(V4, abshum);
        if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
        Blynk.virtualWrite(V5, time1);
        if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
        Blynk.virtualWrite(V6, time2);
        if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
        Blynk.virtualWrite(V6, time2);
        if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
      }

   if (firstrun > 1) {
    for (int i = 0; i < (maxArray - 1); i++) { //add to array for chart drawing
        co20[i] = co20[i + 1];
        volts0[i] = volts0[i + 1];
    }
    volts0[(maxArray - 1)] = (volts);
    co20[(maxArray - 1)] = (co2);
    // Increase the reading count up to maxArray
    if (readingCount < maxArray) {
        readingCount++;
    }
   }
    display.init(115200, false, 10, false); // void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 10, bool pulldown_rst_mode = false)
    display.setRotation(0);
    display.setFont(&Roboto_Condensed_12);
    display.setTextColor(GxEPD_BLACK);
    
    if (firstrun == 2) {
      display.clearScreen();
    }
    firstrun++;
    if (firstrun > 99) {firstrun = 2;}
    doChart();

      if (!buttonstart) {gotosleep(SLEEPTIME_SECS);}
      else {
        terminal.println("***SERVER STARTED***");
        terminal.print("Connected to ");
        terminal.println(ssid);
        terminal.print("IP address: ");
        terminal.println(WiFi.localIP());
        terminal.println(WiFi.RSSI());
        printLocalTime();
      }

      
}

void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() == WL_CONNECTED) { Blynk.run(); }
  every(60000) {
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
      error = scd4x.readMeasurement(co2, temp, hum);
      if (error) {
        terminal.print("Error trying to execute readMeasurement(): ");
        errorToString(error, errorMessage, 256);
        terminal.println(errorMessage);
        terminal.flush();
      } else if (co2 == 0) {
        terminal.println("Invalid sample detected, skipping.");
        terminal.flush();
      } else {
        float abshum = (6.112 * pow(2.71828, ((17.67 * temp) / (temp + 243.5))) * hum * 2.1674) / (273.15 + temp);
        Blynk.virtualWrite(V1, co2);
        Blynk.virtualWrite(V2, temp);
        Blynk.virtualWrite(V3, hum);
        Blynk.virtualWrite(V4, abshum);
      }
    }
  }
}