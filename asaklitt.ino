/**
 * Front and read bicycle flashlighs working time measurement tool
 * Asaklitt front: 3 AAA NiMH batteries
 * Asaklitt rear : 2 AAA NiMH batteries
 * 
 * arachnothrone 2019
 *   //rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // UNCOMMENT TO SET TIME & DATE TO THAT AT COMPILATION.
 *   //rtc.adjust(DateTime(2019, 9, 30, 11, 44, 59 )); // UNCOMMENT TO SET TIME & DATE MANUALLY. (YEAR, MONTH, DAY, 24-HOUR, MINUTE, SECOND)
 */

#include <LiquidCrystal.h>
#include <Thread.h>
#include <SPI.h>
#include <SD.h>

//#include <Wire.h>

#include "Adafruit_Sensor.h"
#include "Adafruit_AM2320.h"
#include "Adafruit_BMP085.h"  // Pressure sensor, I2C (with pullup resistors onboard)

#include "DS3231.h"
#include <U8x8lib.h>

Adafruit_AM2320 temp_humid = Adafruit_AM2320();
Adafruit_BMP085 bmp;
/**
 * Constants
 * 
 * CdS cell: dark = 1010
 */

#define ILLUMINANCE_THR   (500)   /* Absolute threshold for CdS cell */
#define BEAM_OFF_DELTA    (100)   /* CdS cell illuminance delta for 'Torch Beam Off' state detection */
#define ILLUMINANCE_DARK  (1000)  /* Value for background noise */
// #define HIGH            (1)
// #define LOW             (0)

/**
 * Module globals
 */

static String   GV_LogFileName = "asddhhmm.txt";    /* File name format: "asDDHHMM.txt" -> asaklitt, day, hours, minutes */
static bool     GV_LogFileErrorIndicator = false;

//const char someString[10] PROGMEM;

/**
 * 
 */

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
RTClib RTC;

Thread taskOne = Thread();

File logAsklitt;

//U8X8_SSD1306_128X64_NONAME_SW_I2C oled(/* reset=*/ U8X8_PIN_NONE);
//U8X8_SSD1306_128X64_NONAME_SW_I2C oled(/* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // OLEDs without Reset of the Display
U8X8_SSD1306_128X32_UNIVISION_SW_I2C oled(/* clock=*/ 5, /* data=*/ 4, /* reset=*/ U8X8_PIN_NONE);   // OLEDs without Reset of the Display
//U8X8_SSD1306_128X64_NONAME_HW_I2C oled(/* reset=*/ U8X8_PIN_NONE);
//U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C oled(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//U8X8_SSD1306_128X32_UNIVISION_HW_I2C oled(/* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);   // pin remapping with ESP8266 HW I2C

void taskOneFunc(){
    static bool previousState = LOW;                        /* Detected torch beam state (HIGH = on, LOW = off */
    static bool currentState = LOW;
    static int activeTime = 0;                              /* Overall time of the beam in HIGH state */
    static unsigned int stopTimer = 0;                               /* Stopwatch start */
    static unsigned int startTimer = 0;                              /* Stopwatch stop */
    static int dynamicIlluminanceThr = ILLUMINANCE_DARK;    /* Dynamic illuminance threshold, calculated when photocell value difference is greater than BEAM_OFF_DELTA */
    static int previousSensorValue = 0;                     /* Previous illuminance value */
    static int firstCycle = true;
    static unsigned int activeTimeProgressInd = 0;                   /* Active time progress indicator (global indicator updated in real time */

    /* Execution time analysis */
    static unsigned long fullTaskTimerStart = 0;
    static unsigned long fullTaskTimerStop = 0;
    static unsigned int fullTaskTimerVal = 0;
    static unsigned long logTaskTimerStart = 0;
    static unsigned long logTaskTimerStop = 0;
    static unsigned int logTaskTimerVal = 0;
    static unsigned long lcdTaskTimerStart = 0;
    static unsigned long lcdTaskTimerStop = 0;
    static unsigned int  lcdTaskTimerVal = 0;

    fullTaskTimerVal = fullTaskTimerStop - fullTaskTimerStart;
    logTaskTimerVal = logTaskTimerStop - logTaskTimerStart;
    lcdTaskTimerVal = lcdTaskTimerStop - lcdTaskTimerStart;

    fullTaskTimerStart = millis();


    /* Read current date and time */
    DateTime now = RTC.now();
    int year = now.year();
    int mnth = now.month();
    int day = now.day();
    int hour = now.hour();
    int minu = now.minute();
    int seco = now.second();

    /* Read the light sensor to A0 */
    int sensorValue = analogRead(A0);

    /* Update dynamicIlluminanceThr to the value of the last Beam_ON state (only skip for the first cycle) */
    if (firstCycle == false)
    {
      if ((sensorValue - previousSensorValue) > BEAM_OFF_DELTA)
      {
        dynamicIlluminanceThr = previousSensorValue;
      }
    }
    else
    {
      dynamicIlluminanceThr = sensorValue - 100;
      firstCycle = false;
    }
    
    /* Detect the torch beam state */
    //if (sensorValue < ILLUMINANCE_THR)
    if (sensorValue < dynamicIlluminanceThr)
    {
      currentState = HIGH;
    }
    else
    {
      currentState = LOW;
    }
    
    /* Start the stopwatch if torch has been switched ON */
    if (previousState == LOW && currentState == HIGH)
    {
      startTimer = millis() / 1000;
      previousState = HIGH;
    }

    /* Stop the stopwatch when the lights are gone */
    if (previousState == HIGH && currentState == LOW)
    {
      stopTimer = millis() / 1000;
      activeTime += (stopTimer - startTimer);
      previousState = LOW;
      lcd.setCursor(10, 1);
      lcd.write("      ");
      lcd.setCursor(10, 1);
      lcd.print(activeTime);
    }

    /* Print progress since stopwatch start */
    if (currentState == HIGH)
    {
      activeTimeProgressInd = (millis() / 1000 - startTimer) + activeTime;
      lcd.setCursor(10, 1);
      lcd.write("      ");
      lcd.setCursor(10, 1);
      lcd.print(activeTimeProgressInd);
    }

    // Printing seconds since restart on the first row
    // lcd.setCursor(3, 0);
    // lcd.write("     ");             /* date 5 symbols, e.g. "11/21" */
    // lcd.setCursor(3, 0);
    // lcd.print(mnth);
    // lcd.print(day);
    lcdTaskTimerStart = millis();
    lcd.setCursor(0, 0);
    lcd.write("      ");
    lcd.setCursor(0, 0);
    lcd.print(dynamicIlluminanceThr);

    // lcd.setCursor(3, 1);
    // lcd.write("        ");          /* time 8 symbols, e.g. "00:32:56" */
    // lcd.setCursor(3, 1);
    // lcd.print(hour);
    // lcd.print(minu);
    // lcd.print(seco);

    lcd.setCursor(10, 0);
    lcd.write("    ");              /* light sensor value */
    lcd.setCursor(10, 0);
    lcd.print(sensorValue);
    // oled.setCursor(2, 4);
    // oled.print(sensorValue);
    lcdTaskTimerStop = millis();

  /* Prepare the log strings (for Serial and SD card logs */
  // ardprintf("test %d %l %c %s %f", l, k, s, j, f);
  String logString = String("Time: ") 
    + (year < 10 ? "0" : ""  ) + year
    + (mnth < 10 ? "/0" : "/") + mnth
    + (day  < 10 ? "/0" : "/") + day
    + (hour < 10 ? " 0" : " ") + hour
    + (minu < 10 ? ":0" : ":") + minu 
    + (seco < 10 ? ":0" : ":") + seco + ","
    + " Light sensor value: " + sensorValue 
    + " Active time (sec): " + activeTimeProgressInd
    + " Previous state: " + previousState 
    + " Current state: " + currentState 
    //+ " startTimer=" + startTimer 
    //+ " stopTimer=" + stopTimer 
    //+ " dynamicIlluminanceThr=" + dynamicIlluminanceThr 
    // + " FullTask(ms): " + fullTaskTimerVal 
    // + " SDlogTask(ms): " + logTaskTimerVal 
    // + " 2xlcdTaskTimerVal(ms): " + lcdTaskTimerVal 
    + "\n";
  
  String logStringFile = String("TS:")
    + year
    + (mnth < 10 ? "/0" : "/") + mnth
    + (day  < 10 ? "/0" : "/") + day
    + (hour < 10 ? "_0" : "_") + hour
    + (minu < 10 ? ":0" : ":") + minu 
    + (seco < 10 ? ":0" : ":") + seco + " " 
    + sensorValue + " "
    + activeTimeProgressInd + " " 
    + previousState + " "
    + currentState + " "
    + startTimer + " "
    + stopTimer + " "
    + dynamicIlluminanceThr + " "
    + fullTaskTimerVal + " "
    + logTaskTimerVal + " "
    + lcdTaskTimerVal  + " "
    + "\n";
  
  logTaskTimerStart = millis();
  if (GV_LogFileErrorIndicator != true)
  {
    logAsklitt = SD.open(GV_LogFileName, FILE_WRITE);
    
    if(logAsklitt)
    {
      logAsklitt.print(logStringFile);
      logAsklitt.close();
    }
    else
    {
      /* Error indicator */
      lcd.setCursor(15, 0);
      lcd.print("*");
    }
  }
  logTaskTimerStop = millis();

  Serial.print(logString);
  Serial.print(logStringFile);
  // Serial.print(seco);
  oled.setCursor(1, 3);
  oled.print(seco);

  previousSensorValue = sensorValue;
  fullTaskTimerStop = millis();
}

// String logFunction(void* pVarList)
// {
//   3;
// }

void sdCardProgram()
{
  // SD reader is connected to SPI pins (MISO/MOSI/SCK/CS, 5V pwr)
  // setup sd card variables
  Sd2Card card;
  SdVolume volume;
  SdFile root;

  // oled.drawString(0, 1, "SD Card Init...");
  
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.write("SD Card Init...");
  lcd.setCursor(0, 1);
  if (!card.init(SPI_HALF_SPEED, 4))
  {
    lcd.write("Init failed");
    // oled.drawString(0, 1, "SD Init Failed.");
  }
  else
  {
    lcd.write("Init OK");
    // oled.drawString(0, 1, "SD Init OK.");
  }
  delay(500);
  //char cardType[10];
  String cardType = "xxxx";
  lcd.setCursor(0, 1);
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      cardType = "SD1";
      break;
    case SD_CARD_TYPE_SD2:
      cardType = "SD2";
      break;
    case SD_CARD_TYPE_SDHC:
      cardType = "SDHC";
      break;
    default:
      cardType = "Unknown";
  }
  lcd.print("Card Type: ");
  lcd.print(cardType);
  // oled.drawString(0, 2, "Card Type: ");
  // oled.setCursor(11, 2);
  // oled.print(cardType);
  delay(500);
  if (!volume.init(card)) {
    lcd.print("No FAT16/32\npartition.");
    // oled.drawString(0, 1, "No FAT16/32\npartition.");
    delay(3000);
  }
  else {
    lcd.clear();
    lcd.setCursor(0, 0);
    //lcd.autoscroll();
    uint32_t volumeSize;
    volumeSize = volume.blocksPerCluster();    // clusters are collections of blocks
    volumeSize *= volume.clusterCount();       // we'll have a lot of clusters
    volumeSize /= 2;                           // SD card blocks are always 512 bytes (2 blocks are 1KB)
    lcd.print("Vol. size (Kb):");
    lcd.setCursor(0, 1);
    lcd.print(volumeSize);
    delay(1000);
    lcd.clear();

    SD.begin();
  }
}

void setup()
{
  //Wire.begin();
  Serial.begin(115200);
  temp_humid.begin();   // init am2320
  bmp.begin();          // init bmp180
  
  oled.begin();
  oled.setPowerSave(0);
  oled.setFont(u8x8_font_chroma48medium8_r);

  sdCardProgram();
  
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.write("T: ");
  lcd.setCursor(0, 1);
  lcd.write("D: ");

  DateTime now = RTC.now();
  int dd = now.day();
  int hh = now.hour();
  int mm = now.minute();
  // int sc = now.second();
  GV_LogFileName = String("as") + 
    + (dd < 10 ? "0" : "") + dd
    + (hh < 10 ? "0" : "") + hh
    + (mm < 10 ? "0" : "") + mm 
  //  + (sc < 10 ? "0" : "") + sc
    + ".txt";
  logAsklitt = SD.open(GV_LogFileName, FILE_WRITE);
  if(logAsklitt)
  {
    logAsklitt.print("--- Starting new log entry ---\n");
    logAsklitt.print("Time, Light_sensor_value, Active_time_(sec), Previous_state, Current_state, startTimer, stopTimer, dynamicIlluminanceThr, FullTask(ms), SDlogTask(ms), 2xLcdTaskTimerVal(ms)\n");
    logAsklitt.close();
  }
  else
  {
    GV_LogFileErrorIndicator = true;
    lcd.setCursor(3, 0);
    lcd.print("FILE_ERR_01");
    oled.drawString(0,0,"FILE_ERROR_01");
    delay(2000);
    lcd.setCursor(3, 0);
    lcd.print("           ");

    /* Error indicator */
    lcd.setCursor(15, 0);
    lcd.print("x");
    // oled.drawString(9, 0, "x");
    delay(1000);
  }

  taskOne.onRun(taskOneFunc);
  taskOne.setInterval(1000);   // call taskOne every 1000 ms
}

void loop()
{  
  if (taskOne.shouldRun())
    taskOne.run();
}
