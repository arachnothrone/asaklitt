/**
 * Front and read bicycle flashlighs working time measurement tool
 * Asaklitt front: 3 AAA NiMH batteries
 * Asaklitt rear : 2 AAA NiMH batteries
 * 
 * arachnothrone 2019
 */

#include <LiquidCrystal.h>
#include <Thread.h>
#include <SPI.h>      // for sd card
#include <SD.h>
#include "DS3231.h"   // RTC

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
RTClib RTC;

Thread taskOne = Thread();

void taskOneFunc(){
  // Printing seconds since restart on the first row
  lcd.setCursor(5, 0);
  lcd.write("    ");
  lcd.setCursor(5, 0);
  lcd.print(millis() / 1000);
}

  // Read the light sensor to A0
  //int sensorValue = analogRead(A0);
