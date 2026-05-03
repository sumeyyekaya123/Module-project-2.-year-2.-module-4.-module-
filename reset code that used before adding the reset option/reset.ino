#include <EEPROM.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(11, 12, 13, A0, A1, A2);

void setup() {
  lcd.begin(16, 2);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EEPROM RESET");
  lcd.setCursor(0, 1);
  lcd.print("Please wait");

  delay(1000);

  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.update(i, 0);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EEPROM CLEARED");
  lcd.setCursor(0, 1);
  lcd.print("Upload main code");
}

void loop() {
}