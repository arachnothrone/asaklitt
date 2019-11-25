/**
 * Front and read bicycle flashlighs working time measurement tool
 * Asaklitt front: 3 AAA NiMH batteries
 * Asaklitt rear : 2 AAA NiMH batteries
 * 
 * arachnothrone 2019
 */

#include <LiquidCrystal.h>
#include <Thread.h>
#include <SPI.h>
#include <SD.h>
#include "DS3231.h"

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

/**
 * 
 */

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
RTClib RTC;

Thread taskOne = Thread();

File logAsklitt;

void taskOneFunc(){
    static bool previousState = LOW;                        /* Detected torch beam state (HIGH = on, LOW = off */
    static bool currentState = LOW;
    static int activeTime = 0;                              /* Overall time of the beam in HIGH state */
    static int stopTimer = 0;                               /* Stopwatch start */
    static int startTimer = 0;                              /* Stopwatch stop */
    static int dynamicIlluminanceThr = ILLUMINANCE_DARK;    /* Dynamic illuminance threshold, calculated when photocell value difference is greater than BEAM_OFF_DELTA */
    static int previousSensorValue = 0;                     /* Previous illuminance value */
    static int firstCycle = true;
    static int activeTimeProgressInd = 0;                   /* Active time progress indicator (global indicator updated in real time */

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
      lcd.setCursor(12, 1);
      lcd.write("     ");
      lcd.setCursor(12, 1);
      lcd.print(activeTime);
    }

    /* Print progress since stopwatch start */
    if (currentState == HIGH)
    {
      activeTimeProgressInd = (millis() / 1000 - startTimer) + activeTime;
      lcd.setCursor(12, 1);
      lcd.write("     ");
      lcd.setCursor(12, 1);
      lcd.print(activeTimeProgressInd);
    }

    // Printing seconds since restart on the first row
    // lcd.setCursor(3, 0);
    // lcd.write("     ");             /* date 5 symbols, e.g. "11/21" */
    // lcd.setCursor(3, 0);
    // lcd.print(mnth);
    // lcd.print(day);
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

  /* Prepare the log string (for Serial and SD card logs */
  String logString = String("Time: ") 
    + (year < 10 ? "0" : ""  ) + year
    + (mnth < 10 ? "/0" : "/") + mnth
    + (day  < 10 ? "/0" : "/") + day
    + (hour < 10 ? " 0" : " ") + hour
    + (minu < 10 ? ":0" : ":") + minu 
    + (seco < 10 ? ":0" : ":") + seco + ","
    + " Light sensor value: " + sensorValue + " Active time (sec): " + activeTimeProgressInd
    + " Previous state: " + previousState + " Current state: " + currentState 
    + " startTimer=" + startTimer + " stopTimer=" + stopTimer 
    + " dynamicIlluminanceThr=" + dynamicIlluminanceThr + "\n";
  
  logAsklitt = SD.open("asaklitt.log", FILE_WRITE);
  
  if(logAsklitt)
  {
    logAsklitt.print(logString);
    logAsklitt.close();
  }

  Serial.print(logString);

  previousSensorValue = sensorValue;
}

void sdCardProgram()
{
  // SD reader is connected to SPI pins (MISO/MOSI/SCK/CS, 5V pwr)
  // setup sd card variables
  Sd2Card card;
  SdVolume volume;
  SdFile root;
  
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.write("SD Card Init...");
  lcd.setCursor(0, 1);
  if (!card.init(SPI_HALF_SPEED, 4))
    lcd.write("Init failed");
  else
    lcd.write("Init OK");
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
  delay(500);
  if (!volume.init(card)) {
    lcd.print("No FAT16/32\npartition.");
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

    SD.begin();   // (CS_pin) -> sd card initialization
    // logfile = SD.open("akaklitt.log", FILE_WRITE);    // create log file on the card
    // if (logfile){
    //   logfile.println("Starting log...");
    //   logfile.close();
    //   lcd.print("LOG CREATED [OK]");
    // }
    // else
    //   lcd.print("FILE ERROR [01]");
    
    // delay(2000);
  }
}

void setup()
{
  sdCardProgram();
  
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.write("T: ");
  lcd.setCursor(0, 1);
  lcd.write("D: ");

  logAsklitt = SD.open("asaklitt.log", FILE_WRITE);
  if(logAsklitt)
  {
    logAsklitt.print("--- Starting new log entry ---\n");
    logAsklitt.close();
  }

  taskOne.onRun(taskOneFunc);
  taskOne.setInterval(1000);   // call taskOne every 1000 ms
}

void loop()
{  
  if (taskOne.shouldRun())
    taskOne.run();
}
