#include <Wire.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(10, 11, 12, 13, A0, A1);

const byte DS1307_ADDRESS = 0x68;

byte secondValue;
byte minuteValue;
byte hourValue;
byte dayValue;
byte monthValue;
byte yearValue;

void setup() {
  Wire.begin();

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RTC Test");

  delay(1000);

  // Test time:
  // 29/04/2026 14:30:00
  setDS1307Time(0, 30, 14, 29, 4, 26);
}

void loop() {
  readDS1307Time();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Time ");
  printTwoDigits(hourValue);
  lcd.print(":");
  printTwoDigits(minuteValue);
  lcd.print(":");
  printTwoDigits(secondValue);

  lcd.setCursor(0, 1);
  lcd.print("Date ");
  printTwoDigits(dayValue);
  lcd.print("/");
  printTwoDigits(monthValue);
  lcd.print("/20");
  printTwoDigits(yearValue);

  delay(1000);
}

void setDS1307Time(byte second, byte minute, byte hour, byte day, byte month, byte year) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0);

  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(1));
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));

  Wire.endTransmission();
}

void readDS1307Time() {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 7);

  secondValue = bcdToDec(Wire.read() & 0x7F);
  minuteValue = bcdToDec(Wire.read());
  hourValue = bcdToDec(Wire.read() & 0x3F);

  Wire.read();

  dayValue = bcdToDec(Wire.read());
  monthValue = bcdToDec(Wire.read());
  yearValue = bcdToDec(Wire.read());
}

byte decToBcd(byte value) {
  return ((value / 10) * 16) + (value % 10);
}

byte bcdToDec(byte value) {
  return ((value / 16) * 10) + (value % 16);
}

void printTwoDigits(byte value) {
  if (value < 10) {
    lcd.print("0");
  }

  lcd.print(value);
}