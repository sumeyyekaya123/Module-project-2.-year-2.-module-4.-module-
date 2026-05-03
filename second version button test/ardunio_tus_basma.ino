#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int C1 = 2;
const int C2 = 3;
const int C3 = 4;

const int ROW0 = 6;
const int ROW1 = 7;
const int VALID = 10;

int columns[3] = {C1, C2, C3};

bool keyLocked = false;

void setup() {
  pinMode(C1, OUTPUT);
  pinMode(C2, OUTPUT);
  pinMode(C3, OUTPUT);

  pinMode(ROW0, INPUT);
  pinMode(ROW1, INPUT);
  pinMode(VALID, INPUT);

  allColumnsLow();

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("KEYPAD TEST");
}

void loop() {
  char key = scanKeypad();

  if (key != '\0' && keyLocked == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pressed:");
    lcd.setCursor(0, 1);
    lcd.print(key);

    keyLocked = true;
  }

  if (digitalRead(VALID) == LOW) {
    keyLocked = false;
  }

  delay(50);
}

char scanKeypad() {
  for (int col = 0; col < 3; col++) {
    allColumnsLow();
    delay(5);

    digitalWrite(columns[col], HIGH);
    delay(20);

    if (digitalRead(VALID) == HIGH) {
      int rowCode = readRowCode();

      allColumnsLow();

      return decodeKey(rowCode, col);
    }
  }

  allColumnsLow();
  return '\0';
}

int readRowCode() {
  int value = 0;

  if (digitalRead(ROW0) == HIGH) value += 1;
  if (digitalRead(ROW1) == HIGH) value += 2;

  return value;
}

char decodeKey(int rowCode, int col) {
  if (rowCode == 0 && col == 0) return '#';
  if (rowCode == 0 && col == 1) return '0';
  if (rowCode == 0 && col == 2) return '*';

  if (rowCode == 1 && col == 0) return '9';
  if (rowCode == 1 && col == 1) return '8';
  if (rowCode == 1 && col == 2) return '7';

  if (rowCode == 2 && col == 0) return '6';
  if (rowCode == 2 && col == 1) return '5';
  if (rowCode == 2 && col == 2) return '4';

  if (rowCode == 3 && col == 0) return '3';
  if (rowCode == 3 && col == 1) return '2';
  if (rowCode == 3 && col == 2) return '1';

  return '?';
}

void allColumnsLow() {
  digitalWrite(C1, LOW);
  digitalWrite(C2, LOW);
  digitalWrite(C3, LOW);
}