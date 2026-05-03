#include <Wire.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

LiquidCrystal lcd(10, 11, 12, 13, A0, A1);

// Keypad columns
const int C1 = 2;
const int C2 = 3;
const int C3 = 4;

// Door output
const int DOOR_PIN = 5;

// Encoder outputs
const int ENC0 = 6;
const int ENC1 = 7;
const int ENC2 = 8;
const int ENC3 = 9;

int columns[3] = {C1, C2, C3};

// RTC address
const byte RTC_ADDRESS = 0x68;

// FIRST UPLOAD: true
// FINAL REAL USE: false
const bool SET_RTC_ON_BOOT = true;

// Initial RTC time
const byte INIT_SECOND = 0;
const byte INIT_MINUTE = 30;
const byte INIT_HOUR = 14;
const byte INIT_DAY = 30;
const byte INIT_MONTH = 4;
const byte INIT_YEAR = 26;

// EEPROM log
const int EEPROM_NEXT_ADDR = 0;
const int EEPROM_COUNT_ADDR = 1;
const int LOG_START_ADDR = 10;
const int LOG_SIZE = 12;
const int MAX_LOGS = 50;
const byte LOG_MARKER = 0xA5;

byte secondValue;
byte minuteValue;
byte hourValue;
byte dayValue;
byte monthValue;
byte yearValue;

char inputText[5] = "";
int inputCount = 0;

bool keyLocked = false;
int noKeyCount = 0;

// Wrong code blocking
int wrongTryCount = 0;
const int MAX_WRONG_TRIES = 3;
const unsigned long BLOCK_TIME_MS = 10000UL;

enum Mode {
  MODE_ACCESS,
  MODE_ADMIN_CODE,
  MODE_ADMIN_MENU
};

Mode mode = MODE_ACCESS;

void setup() {
  Wire.begin();

  pinMode(C1, OUTPUT);
  pinMode(C2, OUTPUT);
  pinMode(C3, OUTPUT);

  pinMode(DOOR_PIN, OUTPUT);
  digitalWrite(DOOR_PIN, LOW);

  pinMode(ENC0, INPUT);
  pinMode(ENC1, INPUT);
  pinMode(ENC2, INPUT);
  pinMode(ENC3, INPUT);

  allColumnsLow();

  lcd.begin(16, 2);
  lcd.clear();

  if (SET_RTC_ON_BOOT == true) {
    setRTCTime(INIT_SECOND, INIT_MINUTE, INIT_HOUR, INIT_DAY, INIT_MONTH, INIT_YEAR);
  }

  initEEPROMIfNeeded();
  showAccessScreen();
}

void loop() {
  char key = scanKeypadReliable();

  if (key != '\0' && keyLocked == false) {
    handleKey(key);
    keyLocked = true;
    noKeyCount = 0;
  }

  if (key == '\0') {
    noKeyCount++;

    if (noKeyCount >= 2) {
      keyLocked = false;
      noKeyCount = 0;
    }
  } else {
    noKeyCount = 0;
  }

  delay(15);
}

void handleKey(char key) {
  if (key == '#') {
    clearInput();
    mode = MODE_ACCESS;
    showAccessScreen();
    return;
  }

  if (mode == MODE_ACCESS) {
    if (key == '*') {
      clearInput();
      mode = MODE_ADMIN_CODE;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Admin Code:");
      return;
    }

    if (isDigitKey(key)) {
      addInput(key);
      showAccessInput();

      if (inputCount == 3) {
        processAccessCode();
      }
    }
  }

  else if (mode == MODE_ADMIN_CODE) {
    if (isDigitKey(key)) {
      addInput(key);
      lcd.setCursor(0, 1);
      lcd.print(inputText);

      if (inputCount == 3) {
        if (inputText[0] == '9' && inputText[1] == '9' && inputText[2] == '9') {
          clearInput();
          mode = MODE_ADMIN_MENU;
          showAdminMenu();
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Wrong Admin");
          delay(2000);
          clearInput();
          mode = MODE_ACCESS;
          showAccessScreen();
        }
      }
    }
  }

  else if (mode == MODE_ADMIN_MENU) {
    if (key == '1') {
      showLastSuccessLog();
      clearInput();
      mode = MODE_ADMIN_MENU;
      showAdminMenu();
    }

    else if (key == '2') {
      showAllSuccessLogs();
      clearInput();
      mode = MODE_ADMIN_MENU;
      showAdminMenu();
    }

    else if (key == '3') {
      processExitOpen();
      clearInput();
      mode = MODE_ACCESS;
      showAccessScreen();
    }

    else if (key == '0') {
      clearLogs();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Logs Cleared");
      delay(2000);
      clearInput();
      mode = MODE_ADMIN_MENU;
      showAdminMenu();
    }
  }
}

void processAccessCode() {
  allColumnsLow();

  readRTCTime();

  bool accessOK = isCorrectCode();

  if (accessOK == true) {
    wrongTryCount = 0;

    saveSuccessLog();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Code: ");
    lcd.print(inputText);

    lcd.setCursor(0, 1);
    lcd.print("Time ");
    printTwoDigits(hourValue);
    lcd.print(":");
    printTwoDigits(minuteValue);
    lcd.print(":");
    printTwoDigits(secondValue);

    delay(3000);

    openDoor("ACCESS OK");
  } 
  
  else {
    wrongTryCount++;

    digitalWrite(DOOR_PIN, LOW);

    if (wrongTryCount >= MAX_WRONG_TRIES) {
      blockSystem();
      wrongTryCount = 0;
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WRONG CODE");
      lcd.setCursor(0, 1);
      lcd.print("Wrong ");
      lcd.print(wrongTryCount);
      lcd.print("/3");

      delay(5000);
    }
  }

  clearInput();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Resetting...");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready");
  delay(1000);

  showAccessScreen();
}

void blockSystem() {
  allColumnsLow();

  digitalWrite(DOOR_PIN, LOW);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM BLOCKED");
  lcd.setCursor(0, 1);
  lcd.print("Wait 10 sec");

  delay(BLOCK_TIME_MS);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Block Finished");
  delay(1500);
}

void processExitOpen() {
  allColumnsLow();

  readRTCTime();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ADMIN EXIT");
  lcd.setCursor(0, 1);
  lcd.print("Door OPEN");

  digitalWrite(DOOR_PIN, HIGH);
  delay(10000);
  digitalWrite(DOOR_PIN, LOW);
}

void openDoor(const char message[]) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
  lcd.setCursor(0, 1);
  lcd.print("Door OPEN");

  digitalWrite(DOOR_PIN, HIGH);
  delay(10000);
  digitalWrite(DOOR_PIN, LOW);
}

bool isCorrectCode() {
  if (inputText[0] == '1' && inputText[1] == '2' && inputText[2] == '3') return true;
  if (inputText[0] == '4' && inputText[1] == '5' && inputText[2] == '6') return true;
  if (inputText[0] == '7' && inputText[1] == '8' && inputText[2] == '9') return true;

  return false;
}

char scanKeypadReliable() {
  char detectedKey = '\0';

  for (int col = 0; col < 3; col++) {
    allColumnsLow();
    delay(8);

    digitalWrite(columns[col], HIGH);
    delay(25);

    int value1 = readEncoder();
    delay(5);
    int value2 = readEncoder();

    allColumnsLow();
    delay(8);

    if (value1 != 0 && value1 == value2) {
      if (isValueValidForColumn(value1, col)) {
        if (detectedKey == '\0') {
          detectedKey = decodeKey(value1);
        }
      }
    }
  }

  allColumnsLow();
  return detectedKey;
}

int readEncoder() {
  int value = 0;

  if (digitalRead(ENC0) == HIGH) value += 1;
  if (digitalRead(ENC1) == HIGH) value += 2;
  if (digitalRead(ENC2) == HIGH) value += 4;
  if (digitalRead(ENC3) == HIGH) value += 8;

  return value;
}

bool isValueValidForColumn(int value, int col) {
  if (col == 0) {
    if (value == 1) return true;
    if (value == 4) return true;
    if (value == 7) return true;
    if (value == 10) return true;
  }

  if (col == 1) {
    if (value == 2) return true;
    if (value == 5) return true;
    if (value == 8) return true;
    if (value == 11) return true;
  }

  if (col == 2) {
    if (value == 3) return true;
    if (value == 6) return true;
    if (value == 9) return true;
    if (value == 12) return true;
  }

  return false;
}

char decodeKey(int value) {
  if (value == 1) return '1';
  if (value == 2) return '2';
  if (value == 3) return '3';
  if (value == 4) return '4';
  if (value == 5) return '5';
  if (value == 6) return '6';
  if (value == 7) return '7';
  if (value == 8) return '8';
  if (value == 9) return '9';
  if (value == 10) return '*';
  if (value == 11) return '0';
  if (value == 12) return '#';

  return '\0';
}

bool isDigitKey(char key) {
  return key >= '0' && key <= '9';
}

void addInput(char key) {
  if (inputCount < 4) {
    inputText[inputCount] = key;
    inputCount++;
    inputText[inputCount] = '\0';
  }
}

void saveSuccessLog() {
  saveLog(inputText[0], inputText[1], inputText[2]);
}

void saveLog(char c1, char c2, char c3) {
  int nextIndex = EEPROM.read(EEPROM_NEXT_ADDR);
  int count = EEPROM.read(EEPROM_COUNT_ADDR);

  if (nextIndex >= MAX_LOGS) nextIndex = 0;
  if (count > MAX_LOGS) count = 0;

  int addr = LOG_START_ADDR + (nextIndex * LOG_SIZE);

  EEPROM.update(addr + 0, LOG_MARKER);
  EEPROM.update(addr + 1, c1);
  EEPROM.update(addr + 2, c2);
  EEPROM.update(addr + 3, c3);
  EEPROM.update(addr + 4, 'O');
  EEPROM.update(addr + 5, hourValue);
  EEPROM.update(addr + 6, minuteValue);
  EEPROM.update(addr + 7, secondValue);
  EEPROM.update(addr + 8, dayValue);
  EEPROM.update(addr + 9, monthValue);
  EEPROM.update(addr + 10, yearValue);
  EEPROM.update(addr + 11, 0);

  nextIndex++;

  if (nextIndex >= MAX_LOGS) {
    nextIndex = 0;
  }

  if (count < MAX_LOGS) {
    count++;
  }

  EEPROM.update(EEPROM_NEXT_ADDR, nextIndex);
  EEPROM.update(EEPROM_COUNT_ADDR, count);
}

void showLastSuccessLog() {
  int count = EEPROM.read(EEPROM_COUNT_ADDR);
  int nextIndex = EEPROM.read(EEPROM_NEXT_ADDR);

  if (count <= 0 || count > MAX_LOGS) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No Unlock Log");
    delay(2500);
    return;
  }

  int lastIndex = nextIndex - 1;

  if (lastIndex < 0) {
    lastIndex = MAX_LOGS - 1;
  }

  showLogAtIndex(lastIndex, 1);
}

void showAllSuccessLogs() {
  int count = EEPROM.read(EEPROM_COUNT_ADDR);
  int nextIndex = EEPROM.read(EEPROM_NEXT_ADDR);

  if (count <= 0 || count > MAX_LOGS) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No Unlock Log");
    delay(2500);
    return;
  }

  for (int shown = 0; shown < count; shown++) {
    int index = nextIndex - 1 - shown;

    if (index < 0) {
      index = index + MAX_LOGS;
    }

    showLogAtIndex(index, shown + 1);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("End of Logs");
  delay(2000);
}

void showLogAtIndex(int index, int number) {
  int addr = LOG_START_ADDR + (index * LOG_SIZE);

  if (EEPROM.read(addr + 0) != LOG_MARKER) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Log Error");
    delay(2500);
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("#");
  lcd.print(number);
  lcd.print(" Code:");
  lcd.print((char)EEPROM.read(addr + 1));
  lcd.print((char)EEPROM.read(addr + 2));
  lcd.print((char)EEPROM.read(addr + 3));

  lcd.setCursor(0, 1);
  printTwoDigits(EEPROM.read(addr + 8));
  lcd.print("/");
  printTwoDigits(EEPROM.read(addr + 9));
  lcd.print(" ");
  printTwoDigits(EEPROM.read(addr + 5));
  lcd.print(":");
  printTwoDigits(EEPROM.read(addr + 6));

  delay(4000);
}

void clearLogs() {
  EEPROM.update(EEPROM_NEXT_ADDR, 0);
  EEPROM.update(EEPROM_COUNT_ADDR, 0);

  for (int i = 0; i < MAX_LOGS; i++) {
    int addr = LOG_START_ADDR + (i * LOG_SIZE);
    EEPROM.update(addr + 0, 0);
  }
}

void initEEPROMIfNeeded() {
  int nextIndex = EEPROM.read(EEPROM_NEXT_ADDR);
  int count = EEPROM.read(EEPROM_COUNT_ADDR);

  if (nextIndex >= MAX_LOGS) {
    EEPROM.update(EEPROM_NEXT_ADDR, 0);
  }

  if (count > MAX_LOGS) {
    EEPROM.update(EEPROM_COUNT_ADDR, 0);
  }
}

void setRTCTime(byte second, byte minute, byte hour, byte day, byte month, byte year) {
  Wire.beginTransmission(RTC_ADDRESS);
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

void readRTCTime() {
  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();

  Wire.requestFrom((uint8_t)RTC_ADDRESS, (uint8_t)7);

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

void showAccessScreen() {
  mode = MODE_ACCESS;
  clearInput();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Code:");
}

void showAccessInput() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Code:");
  lcd.setCursor(0, 1);
  lcd.print(inputText);
}

void showAdminMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1:Last 2:All");
  lcd.setCursor(0, 1);
  lcd.print("3:Exit 0:Clr");
}

void clearInput() {
  inputText[0] = '\0';
  inputText[1] = '\0';
  inputText[2] = '\0';
  inputText[3] = '\0';
  inputText[4] = '\0';
  inputCount = 0;
}

void allColumnsLow() {
  digitalWrite(C1, LOW);
  digitalWrite(C2, LOW);
  digitalWrite(C3, LOW);
}