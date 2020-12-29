/**
 * Front and read bicycle flashlighs working time measurement tool
 * Asaklitt front: 3 AAA NiMH batteries
 * Asaklitt rear : 2 AAA NiMH batteries
 * 
 * arachnothrone 2019
 *   //rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // UNCOMMENT TO SET TIME & DATE TO THAT AT COMPILATION.
 *   //rtc.adjust(DateTime(2019, 9, 30, 11, 44, 59 )); // UNCOMMENT TO SET TIME & DATE MANUALLY. (YEAR, MONTH, DAY, 24-HOUR, MINUTE, SECOND)
 */

// #include <LiquidCrystal.h>
#include <Thread.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>

// #include "Adafruit_Sensor.h"
// #include "Adafruit_AM2320.h"
// #include "Adafruit_BMP085.h"  // Pressure sensor, I2C (with pullup resistors onboard)

#include "DS3231.h"
#include <U8x8lib.h>

// Adafruit_AM2320 temp_humid = Adafruit_AM2320();
// Adafruit_BMP085 bmp;
/**
 * Constants
 * 
 * CdS cell: dark = 1010
 */

#define ILLUMINANCE_THR   (500)   /* Absolute threshold for CdS cell */
#define BEAM_OFF_DELTA    (100)   /* CdS cell illuminance delta for 'Torch Beam Off' state detection */
#define ILLUMINANCE_DARK  (1000)  /* Value for background noise */
#define SD_LOG_BUFFER_LEN (78)    /* String buffer length for SD log file */
#define SD_LOG_FILE_NAME_LEN  (13)  
// #define HIGH            (1)
// #define LOW             (0)

/**
 * Module globals
 */

//static String   GV_LogFileName = "asddhhmm.txt";    /* File name format: "asDDHHMM.txt" -> asaklitt, day, hours, minutes */
static char     GV_LogFileName[SD_LOG_FILE_NAME_LEN] = "asddhhmm.txt";
static bool     GV_LogFileErrorIndicator = false;

//const char someString[10] PROGMEM;

/**
 * 
 */

// LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
RTClib RTC;

Thread taskOne = Thread();

File logAsklitt;

//   U8X8_SSD1306_128X32_UNIVISION_SW_I2C oled(/* clock=*/ 5, /* data=*/ 4, /* reset=*/ U8X8_PIN_NONE);   // OLEDs without Reset of the Display
//U8X8_SSD1306_128X64_NONAME_HW_I2C oled(/* reset=*/ U8X8_PIN_NONE);
U8X8_SSD1306_128X32_UNIVISION_HW_I2C oled(/* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);   // pin remapping with ESP8266 HW I2C

/** 
 * Returns the number of digits of the integer value
 * param: num   considered to be a positive number within [0..99999]
 */
int getNumDigits(int* num)
{
  return 
    *num < 10 ? 1 :
    *num < 100 ? 2 :
    *num < 1000 ? 3 :
    *num < 10000 ? 4 : 5;
}

void taskOneFunc(){
    static bool previousState = LOW;                        /* Detected torch beam state (HIGH = on, LOW = off */
    static bool currentState = LOW;
    static int activeTime = 0;                              /* Overall time of the beam in HIGH state */
    static unsigned int stopTimer = 0;                               /* Stopwatch start */
    static unsigned int startTimer = 0;                              /* Stopwatch stop */
    static int dynamicIlluminanceThr = ILLUMINANCE_DARK;    /* Dynamic illuminance threshold, calculated when photocell value difference is greater than BEAM_OFF_DELTA */
    static int previousDynamicThr = 0;
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

    char sdLogBuffer[SD_LOG_BUFFER_LEN];

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
      oled.setCursor(8, 3);
      oled.print(activeTime);
    }

    /* Print progress since stopwatch start */
    if (currentState == HIGH)
    {
      activeTimeProgressInd = (millis() / 1000 - startTimer) + activeTime;
      oled.setCursor(8, 3);
      oled.print(activeTimeProgressInd);
    }

    lcdTaskTimerStart = millis();
    
    /* Refresh display area if number gets shorter than previous and print value */
    if (getNumDigits(&previousDynamicThr) > getNumDigits(&dynamicIlluminanceThr))
    {
      oled.drawString(8, 2, "    ");
    }
    oled.setCursor(8, 2);
    oled.print(dynamicIlluminanceThr);  /* dynamic illuminance threshold */

    /* Refresh display area if number gets shorter than previous and print value */
    if (getNumDigits(&previousSensorValue) > getNumDigits(&sensorValue))
    {
      oled.drawString(8, 1, "    ");
    }
    oled.setCursor(8, 1);
    oled.print(sensorValue);          /* light sensor value */
    lcdTaskTimerStop = millis();

  /* Prepare the log strings (for Serial and SD card logs */
  Serial.print(F("Time: "));
  Serial.print(year);
  Serial.print(mnth < 10 ? F("/0") : F("/"));
  Serial.print(mnth);
  Serial.print(day < 10 ? F("/0") : F("/"));
  Serial.print(day);
  Serial.print(hour < 10 ? F("_0") : F("_"));
  Serial.print(hour);
  Serial.print(minu < 10 ? F(":0") : F(":"));
  Serial.print(minu);
  Serial.print(seco < 10 ? F(":0") : F(":"));
  Serial.print(seco);
  Serial.print(F(" LightSensorValue="));
  Serial.print(sensorValue);
  Serial.print(F(" ActiveTime(sec)="));
  Serial.print(activeTimeProgressInd);
  Serial.print(F(" PreviousState="));
  Serial.print(previousState);
  Serial.print(F(" CurrentState="));
  Serial.print(currentState);
  Serial.print(F(" startTimer="));
  Serial.print(startTimer);
  Serial.print(F(" stopTimer="));
  Serial.print(stopTimer);
  Serial.print(F(" dynamicIlluminanceThr="));
  Serial.print(dynamicIlluminanceThr);
  Serial.print(F(" FullTask(ms)="));
  Serial.print(fullTaskTimerVal);
  Serial.print(F(" SDlogTask(ms)="));
  Serial.print(logTaskTimerVal);
  Serial.print(F(" 2xlcdTaskTimerVal(ms)="));
  Serial.println(lcdTaskTimerVal);
  
  logTaskTimerStart = millis();

  if (GV_LogFileErrorIndicator != true)
  {
    logAsklitt = SD.open(GV_LogFileName, FILE_WRITE); //O_APPEND
    // logAsklitt = SD.open("lgfile.txt", FILE_WRITE);
    if(logAsklitt)
    {
      sprintf(sdLogBuffer, "%04d/%02d/%02d_%02d:%02d:%02d %4d %6d %d %d %4d %4d %4d %4d \n"
          , year, mnth, day, hour, minu, seco
          , sensorValue, activeTimeProgressInd
          , previousState, currentState, dynamicIlluminanceThr
          , fullTaskTimerVal, logTaskTimerVal, lcdTaskTimerVal
          );
      //logAsklitt.print(logStringFile);
      logAsklitt.print(sdLogBuffer);
      // logAsklitt.flush();
      // logAsklitt.close();
    }
    else
    {
      /* Error indicator */
      oled.drawString(15, 3, "*");
      oled.setCursor(11, 2);
      oled.print(logAsklitt);
      Serial.println(logAsklitt.availableForWrite());
    }
    logAsklitt.close();
  }
  logTaskTimerStop = millis();

  /* Display time and date */
  printRtcValueOnDisplay(0, 0, hour);
  oled.print(":");

  printRtcValueOnDisplay(3, 0, minu);
  oled.print(":");

  printRtcValueOnDisplay(6, 0, seco);
  oled.print(" ");
  
  printRtcValueOnDisplay(9, 0, day);
  oled.print("-");

  printRtcValueOnDisplay(12, 0, mnth);

  // Serial.print(logString);
  // Serial.print(logStringFile);

  previousSensorValue = sensorValue;
  previousDynamicThr = dynamicIlluminanceThr;
  fullTaskTimerStop = millis();
}

/**
 * Print Hour, Minute, Second, Date or Month to occupy 2 decimal places
 * (when value is "1" -> print "01")
 */
void printRtcValueOnDisplay(uint8_t xPos, uint8_t yPos, uint8_t value)
{
  oled.setCursor(xPos, yPos);
  if (value < 10)
  {
    oled.print("0");
  }
  oled.print(value);
}

void sdCardProgram()
{
  // SD reader is connected to SPI pins (MISO/MOSI/SCK/CS, 5V pwr)
  // setup sd card variables
  Sd2Card card;
  SdVolume volume;
  SdFile root;

  oled.drawString(0, 0, "SD Card Init...");

  if (!card.init(SPI_HALF_SPEED, SD_CHIP_SELECT_PIN))  // SPI_HALF_SPEED, 4    SPI_FULL_SPEED SD_CHIP_SELECT_PIN
  {
    oled.drawString(0, 1, "SD Init Failed.");
    char sdErrCode = card.errorCode();
    char sdErrData = card.errorData();
    // oled.drawString(0, 2, sdErrCode);
    // oled.drawString(0, 3, sdErrData);
    oled.drawString(0, 2, "ErrCode/Data:");
    oled.setCursor(0, 3);
    oled.print(sdErrCode);
    oled.setCursor(5, 3);
    oled.print(sdErrData);
    delay(3000);
  }
  else
  {
    oled.drawString(0, 1, "SD Init OK.");
  }
  delay(500);
  //char cardType[10];
  String cardType = "----";
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
      cardType = "--";
  }
  oled.drawString(0, 2, "Card Type: ");
  oled.setCursor(11, 2);
  oled.print(cardType);
  delay(900);
  if (!volume.init(card)) {
    oled.drawString(0, 3, "No FAT partition");
    delay(3000);
    oled.clear();
  }
  else {
    oled.clear();
    uint32_t volumeSize;
    volumeSize = volume.blocksPerCluster();
    volumeSize *= volume.clusterCount();
    volumeSize /= 2;                           /* One SD card block is 512 bytes */
    oled.print("Vol. size (Kb):\n");
    oled.print(volumeSize);
    delay(1000);
    oled.clear();

    SD.begin(SPI_HALF_SPEED, SD_CHIP_SELECT_PIN);
  }
}

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  oled.begin();

  oled.setPowerSave(0);
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.setContrast(11);

  sdCardProgram();

  DateTime now = RTC.now();
  int dd = now.day();
  int hh = now.hour();
  int mm = now.minute();
  sprintf(GV_LogFileName, "as%02d%02d%02d.txt", dd, hh, mm);
  oled.setCursor(0, 0);
  oled.print(GV_LogFileName);
  delay(3000);
  
  logAsklitt = SD.open(GV_LogFileName, FILE_WRITE);
  // logAsklitt = SD.open("lgfile.txt", FILE_WRITE);
  if(logAsklitt)
  {
    logAsklitt.print(F("--- Starting new log entry ---\n"));
    logAsklitt.print(F("Time, Light_sensor_value, Active_time_(sec), Previous_state, Current_state, dynamicIlluminanceThr, FullTask(ms), SDlogTask(ms), 2xLcdTaskTimerVal(ms)\n"));
    // logAsklitt.close();
  }
  else
  {
    GV_LogFileErrorIndicator = true;
    oled.drawString(0, 0, "FILE_ERROR_01");
    delay(2000);
    oled.drawString(0, 0, "             ");

    /* Error indicator */
    oled.drawString(15, 3, "x");
    delay(1000);
  }
  logAsklitt.close();

  oled.drawString(0, 1, "Sensor: ");
  oled.drawString(0, 2, "Thresh: ");
  oled.drawString(0, 3, "Active: ");
  taskOne.onRun(taskOneFunc);
  taskOne.setInterval(1000);   // call taskOne every 1000 ms

  //oled.clear();
}

void loop()
{  
  if (taskOne.shouldRun())
    taskOne.run();
}

// -------------------------
// Sketch uses 25396 bytes (88%) of program storage space. Maximum is 28672 bytes.
// Global variables use 1601 bytes (62%) of dynamic memory, leaving 959 bytes for local variables. Maximum is 2560 bytes.