#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad columns
const int C1 = 2;
const int C2 = 3;
const int C3 = 4;

// Door / MOSFET Gate output
const int DOOR_PIN = 5;

// OR encoder outputs
const int ROW0 = 6;
const int ROW1 = 7;
const int VALID = 10;

int columns[3] = {C1, C2, C3};

// RTC DS3231
const byte RTC_ADDRESS = 0x68;

// EEPROM log addresses
const int EEPROM_NEXT_ADDR = 0;
const int EEPROM_COUNT_ADDR = 1;
const int EEPROM_TIME_SET_ADDR = 2;
const int EEPROM_CODE_INIT_ADDR = 3;

const int LOG_START_ADDR = 10;
const int LOG_SIZE = 12;
const int MAX_LOGS = 50;

const byte LOG_MARKER = 0xA5;
const byte TIME_SET_MARKER = 0x5A;

// EEPROM code addresses
const int CODE_START_ADDR = 700;
const int CODE_SLOT_SIZE = 4;
const int MAX_CODES = 5;

const byte CODE_MARKER = 0xB6;
const byte CODE_INIT_MARKER = 0xC3;

// If true, time setup screen always opens.
// Final use: false
const bool FORCE_TIME_SETUP = false;

char inputText[4] = "";
int inputCount = 0;

char actionCode[4] = "";
int actionCount = 0;

char oldCode[4] = "";
int oldCodeCount = 0;

char newCode[4] = "";
int newCodeCount = 0;

bool keyLocked = false;

int wrongTryCount = 0;
const int MAX_WRONG_TRIES = 3;
const unsigned long BLOCK_TIME_MS = 10000UL;

byte secondValue;
byte minuteValue;
byte hourValue;
byte weekDayValue;
byte dayValue;
byte monthValue;
byte yearValue;

byte setupSecond;
byte setupMinute;
byte setupHour;
byte setupWeekDay;
byte setupDay;
byte setupMonth;
byte setupYear;

char timeInput[3] = "";
int timeInputCount = 0;
int timeStep = 0;
bool timeConfirmScreen = false;

int adminPage = 0;
const int ADMIN_PAGE_COUNT = 5;

enum Mode {
  MODE_TIME_SETUP,
  MODE_ACCESS,
  MODE_ADMIN_CODE,
  MODE_ADMIN_MENU,
  MODE_ADD_CODE,
  MODE_ADD_CONFIRM,
  MODE_EDIT_OLD_CODE,
  MODE_EDIT_NEW_CODE,
  MODE_EDIT_CONFIRM,
  MODE_DELETE_CODE,
  MODE_DELETE_CONFIRM,
  MODE_TIME_RESET_CONFIRM
};

Mode mode = MODE_ACCESS;

void setup() {
  Wire.begin();

  pinMode(C1, OUTPUT);
  pinMode(C2, OUTPUT);
  pinMode(C3, OUTPUT);

  pinMode(DOOR_PIN, OUTPUT);
  digitalWrite(DOOR_PIN, LOW);

  pinMode(ROW0, INPUT);
  pinMode(ROW1, INPUT);
  pinMode(VALID, INPUT);

  allColumnsLow();

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Start");
  delay(1500);

  initEEPROMIfNeeded();

  if (FORCE_TIME_SETUP == true || EEPROM.read(EEPROM_TIME_SET_ADDR) != TIME_SET_MARKER) {
    startTimeSetup();
  } else {
    showAccessScreen();
  }
}

void loop() {
  char key = scanKeypad();

  if (key != '\0' && keyLocked == false) {
    if (mode == MODE_TIME_SETUP) {
      handleTimeSetupKey(key);
    } else {
      handleKey(key);
    }

    keyLocked = true;
  }

  if (digitalRead(VALID) == LOW) {
    keyLocked = false;
  }

  delay(50);
}

void handleTimeSetupKey(char key) {
  if (timeConfirmScreen == true) {
    if (key == '#') {
      setRTCTime(setupSecond, setupMinute, setupHour, setupWeekDay, setupDay, setupMonth, setupYear);
      EEPROM.update(EEPROM_TIME_SET_ADDR, TIME_SET_MARKER);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Time Saved");
      delay(1500);

      showAccessScreen();
      return;
    }

    if (key == '*') {
      startTimeSetup();
      return;
    }

    return;
  }

  if (key == '*') {
    clearTimeInput();
    showTimeStepScreen();
    return;
  }

  if (isDigitKey(key)) {
    if (timeInputCount < getRequiredDigitsForTimeStep()) {
      timeInput[timeInputCount] = key;
      timeInputCount++;
      timeInput[timeInputCount] = '\0';

      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(timeInput);
    }

    if (timeInputCount == getRequiredDigitsForTimeStep()) {
      int value = atoi(timeInput);

      if (isValidTimeValue(timeStep, value) == true) {
        saveTimeStepValue(timeStep, value);
        timeStep++;
        clearTimeInput();

        if (timeStep > 6) {
          showTimeConfirmScreen();
        } else {
          showTimeStepScreen();
        }
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Invalid Value");
        delay(1500);

        clearTimeInput();
        showTimeStepScreen();
      }
    }
  }
}

void handleKey(char key) {
  if (mode == MODE_ACCESS) {
    handleAccessKey(key);
  }

  else if (mode == MODE_ADMIN_CODE) {
    handleAdminCodeKey(key);
  }

  else if (mode == MODE_ADMIN_MENU) {
    handleAdminMenuKey(key);
  }

  else if (mode == MODE_ADD_CODE) {
    handleAddCodeKey(key);
  }

  else if (mode == MODE_ADD_CONFIRM) {
    handleAddConfirmKey(key);
  }

  else if (mode == MODE_EDIT_OLD_CODE) {
    handleEditOldCodeKey(key);
  }

  else if (mode == MODE_EDIT_NEW_CODE) {
    handleEditNewCodeKey(key);
  }

  else if (mode == MODE_EDIT_CONFIRM) {
    handleEditConfirmKey(key);
  }

  else if (mode == MODE_DELETE_CODE) {
    handleDeleteCodeKey(key);
  }

  else if (mode == MODE_DELETE_CONFIRM) {
    handleDeleteConfirmKey(key);
  }

  else if (mode == MODE_TIME_RESET_CONFIRM) {
    handleTimeResetConfirmKey(key);
  }
}

void handleAccessKey(char key) {
  if (key == '#') {
    clearInput();
    showAccessScreen();
    return;
  }

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

void handleAdminCodeKey(char key) {
  if (key == '#' || key == '*') {
    clearInput();
    showAccessScreen();
    return;
  }

  if (isDigitKey(key)) {
    addInput(key);

    lcd.setCursor(0, 1);
    lcd.print(inputText);

    if (inputCount == 3) {
      if (isAdminCode(inputText) == true) {
        clearInput();
        showAdminIntro();
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wrong Admin");
        delay(2000);

        clearInput();
        showAccessScreen();
      }
    }
  }
}

void handleAdminMenuKey(char key) {
  if (key == '#') {
    adminPage++;

    if (adminPage >= ADMIN_PAGE_COUNT) {
      adminPage = 0;
    }

    showAdminMenuPage();
    return;
  }

  if (key == '*') {
    adminPage--;

    if (adminPage < 0) {
      adminPage = ADMIN_PAGE_COUNT - 1;
    }

    showAdminMenuPage();
    return;
  }

  if (key == '1') {
    showLastLog();
    showAdminMenuPage();
    return;
  }

  if (key == '2') {
    showAllLogs();
    showAdminMenuPage();
    return;
  }

  if (key == '3') {
    processExitOpen();
    showAdminMenuPage();
    return;
  }

  if (key == '4') {
    startAddCode();
    return;
  }

  if (key == '5') {
    startEditCode();
    return;
  }

  if (key == '6') {
    startDeleteCode();
    return;
  }

  if (key == '7') {
    startTimeResetConfirm();
    return;
  }

  if (key == '0') {
    clearLogs();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Logs Cleared");
    delay(2000);

    showAdminMenuPage();
    return;
  }

  if (key == '9') {
    clearInput();
    showAccessScreen();
    return;
  }
}

void handleAddCodeKey(char key) {
  if (key == '*') {
    cancelAdminAction();
    return;
  }

  if (isDigitKey(key)) {
    addActionDigit(key);

    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(actionCode);

    if (actionCount == 3) {
      mode = MODE_ADD_CONFIRM;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(actionCode);
      lcd.print(" Save?");
      lcd.setCursor(0, 1);
      lcd.print("#Save *Cancel");
    }
  }
}

void handleAddConfirmKey(char key) {
  if (key == '*') {
    cancelAdminAction();
    return;
  }

  if (key == '#') {
    addAccessCode(actionCode);
    clearActionCode();
    showAdminMenuPage();
    return;
  }
}

void handleEditOldCodeKey(char key) {
  if (key == '*') {
    cancelAdminAction();
    return;
  }

  if (isDigitKey(key)) {
    addOldCodeDigit(key);

    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(oldCode);

    if (oldCodeCount == 3) {
      if (findCodeSlot(oldCode) < 0) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Old Not Found");
        delay(2000);

        clearOldCode();
        showAdminMenuPage();
        return;
      }

      mode = MODE_EDIT_NEW_CODE;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("New Code:");
    }
  }
}

void handleEditNewCodeKey(char key) {
  if (key == '*') {
    cancelAdminAction();
    return;
  }

  if (isDigitKey(key)) {
    addNewCodeDigit(key);

    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(newCode);

    if (newCodeCount == 3) {
      mode = MODE_EDIT_CONFIRM;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(oldCode);
      lcd.print(">");
      lcd.print(newCode);
      lcd.setCursor(0, 1);
      lcd.print("#Save *Cancel");
    }
  }
}

void handleEditConfirmKey(char key) {
  if (key == '*') {
    cancelAdminAction();
    return;
  }

  if (key == '#') {
    editAccessCode(oldCode, newCode);

    clearOldCode();
    clearNewCode();

    showAdminMenuPage();
    return;
  }
}

void handleDeleteCodeKey(char key) {
  if (key == '*') {
    cancelAdminAction();
    return;
  }

  if (isDigitKey(key)) {
    addActionDigit(key);

    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(actionCode);

    if (actionCount == 3) {
      mode = MODE_DELETE_CONFIRM;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Del ");
      lcd.print(actionCode);
      lcd.print("?");
      lcd.setCursor(0, 1);
      lcd.print("#Del *Cancel");
    }
  }
}

void handleDeleteConfirmKey(char key) {
  if (key == '*') {
    cancelAdminAction();
    return;
  }

  if (key == '#') {
    deleteAccessCode(actionCode);
    clearActionCode();
    showAdminMenuPage();
    return;
  }
}

void startTimeResetConfirm() {
  mode = MODE_TIME_RESET_CONFIRM;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reset Time?");
  lcd.setCursor(0, 1);
  lcd.print("#Yes *Cancel");
}

void handleTimeResetConfirmKey(char key) {
  if (key == '*') {
    cancelAdminAction();
    return;
  }

  if (key == '#') {
    EEPROM.update(EEPROM_TIME_SET_ADDR, 0);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time Reset");
    delay(1500);

    startTimeSetup();
    return;
  }
}

void startTimeSetup() {
  mode = MODE_TIME_SETUP;
  timeStep = 0;
  timeConfirmScreen = false;
  clearTimeInput();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Date/Time");
  delay(1500);

  showTimeStepScreen();
}

void showTimeStepScreen() {
  lcd.clear();

  if (timeStep == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Day DD:");
  }

  else if (timeStep == 1) {
    lcd.setCursor(0, 0);
    lcd.print("Month MM:");
  }

  else if (timeStep == 2) {
    lcd.setCursor(0, 0);
    lcd.print("Year YY:");
  }

  else if (timeStep == 3) {
    lcd.setCursor(0, 0);
    lcd.print("WeekDay 1-7:");
  }

  else if (timeStep == 4) {
    lcd.setCursor(0, 0);
    lcd.print("Hour 00-23:");
  }

  else if (timeStep == 5) {
    lcd.setCursor(0, 0);
    lcd.print("Minute 00-59:");
  }

  else if (timeStep == 6) {
    lcd.setCursor(0, 0);
    lcd.print("Second 00-59:");
  }

  lcd.setCursor(0, 1);
}

int getRequiredDigitsForTimeStep() {
  if (timeStep == 3) return 1;
  return 2;
}

bool isValidTimeValue(int step, int value) {
  if (step == 0 && value >= 1 && value <= 31) return true;
  if (step == 1 && value >= 1 && value <= 12) return true;
  if (step == 2 && value >= 0 && value <= 99) return true;
  if (step == 3 && value >= 1 && value <= 7) return true;
  if (step == 4 && value >= 0 && value <= 23) return true;
  if (step == 5 && value >= 0 && value <= 59) return true;
  if (step == 6 && value >= 0 && value <= 59) return true;

  return false;
}

void saveTimeStepValue(int step, int value) {
  if (step == 0) setupDay = value;
  else if (step == 1) setupMonth = value;
  else if (step == 2) setupYear = value;
  else if (step == 3) setupWeekDay = value;
  else if (step == 4) setupHour = value;
  else if (step == 5) setupMinute = value;
  else if (step == 6) setupSecond = value;
}

void showTimeConfirmScreen() {
  timeConfirmScreen = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  printTwoDigits(setupDay);
  lcd.print("/");
  printTwoDigits(setupMonth);
  lcd.print("/");
  printTwoDigits(setupYear);
  lcd.print(" W");
  lcd.print(setupWeekDay);

  lcd.setCursor(0, 1);
  printTwoDigits(setupHour);
  lcd.print(":");
  printTwoDigits(setupMinute);
  lcd.print(":");
  printTwoDigits(setupSecond);

  delay(2500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("# Save");
  lcd.setCursor(0, 1);
  lcd.print("* Restart");
}

void showAdminIntro() {
  mode = MODE_ADMIN_MENU;
  adminPage = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ADMIN MODE");
  lcd.setCursor(0, 1);
  lcd.print("#Next *Prev");
  delay(2000);

  showAdminMenuPage();
}

void showAdminMenuPage() {
  mode = MODE_ADMIN_MENU;

  lcd.clear();

  if (adminPage == 0) {
    lcd.setCursor(0, 0);
    lcd.print("1:Last 2:All");
    lcd.setCursor(0, 1);
    lcd.print("#Next *Prev");
  }

  else if (adminPage == 1) {
    lcd.setCursor(0, 0);
    lcd.print("3:Open 4:Add");
    lcd.setCursor(0, 1);
    lcd.print("#Next *Prev");
  }

  else if (adminPage == 2) {
    lcd.setCursor(0, 0);
    lcd.print("5:Edit 6:Del");
    lcd.setCursor(0, 1);
    lcd.print("#Next *Prev");
  }

  else if (adminPage == 3) {
    lcd.setCursor(0, 0);
    lcd.print("0:Clear 9:Exit");
    lcd.setCursor(0, 1);
    lcd.print("#Next *Prev");
  }

  else if (adminPage == 4) {
    lcd.setCursor(0, 0);
    lcd.print("7:Time Reset");
    lcd.setCursor(0, 1);
    lcd.print("#Next *Prev");
  }
}

void startAddCode() {
  mode = MODE_ADD_CODE;
  clearActionCode();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("New Code:");
}

void startEditCode() {
  mode = MODE_EDIT_OLD_CODE;
  clearOldCode();
  clearNewCode();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Old Code:");
}

void startDeleteCode() {
  mode = MODE_DELETE_CODE;
  clearActionCode();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Del Code:");
}

void cancelAdminAction() {
  clearActionCode();
  clearOldCode();
  clearNewCode();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cancelled");
  delay(1200);

  showAdminMenuPage();
}

void processAccessCode() {
  readRTCTime();

  bool accessOK = isCorrectCode();

  if (accessOK == true) {
    wrongTryCount = 0;

    saveLog(inputText[0], inputText[1], inputText[2], 'O');

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ACCESS OK");

    lcd.setCursor(0, 1);
    lcd.print("Door OPEN");

    digitalWrite(DOOR_PIN, HIGH);
    delay(10000);
    digitalWrite(DOOR_PIN, LOW);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door CLOSED");
    delay(1500);
  }

  else {
    wrongTryCount++;

    saveLog(inputText[0], inputText[1], inputText[2], 'X');

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

      delay(3000);
    }
  }

  clearInput();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Reset");
  delay(1500);

  showAccessScreen();
}

void processExitOpen() {
  readRTCTime();

  saveLog('E', 'X', 'T', 'E');

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EXIT OPEN");
  lcd.setCursor(0, 1);
  lcd.print("Door OPEN");

  digitalWrite(DOOR_PIN, HIGH);
  delay(10000);
  digitalWrite(DOOR_PIN, LOW);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Door CLOSED");
  delay(1500);
}

void blockSystem() {
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

bool isCorrectCode() {
  if (findCodeSlot(inputText) >= 0) {
    return true;
  }

  return false;
}

bool isAdminCode(const char code[]) {
  if (code[0] == '9' && code[1] == '9' && code[2] == '9') {
    return true;
  }

  return false;
}

bool isValidUserCode(const char code[]) {
  if (code[0] < '0' || code[0] > '9') return false;
  if (code[1] < '0' || code[1] > '9') return false;
  if (code[2] < '0' || code[2] > '9') return false;

  if (isAdminCode(code) == true) {
    return false;
  }

  return true;
}

int findCodeSlot(const char code[]) {
  for (int slot = 0; slot < MAX_CODES; slot++) {
    int addr = CODE_START_ADDR + (slot * CODE_SLOT_SIZE);

    if (EEPROM.read(addr) == CODE_MARKER) {
      char c1 = (char)EEPROM.read(addr + 1);
      char c2 = (char)EEPROM.read(addr + 2);
      char c3 = (char)EEPROM.read(addr + 3);

      if (c1 == code[0] && c2 == code[1] && c3 == code[2]) {
        return slot;
      }
    }
  }

  return -1;
}

int findEmptyCodeSlot() {
  for (int slot = 0; slot < MAX_CODES; slot++) {
    int addr = CODE_START_ADDR + (slot * CODE_SLOT_SIZE);

    if (EEPROM.read(addr) != CODE_MARKER) {
      return slot;
    }
  }

  return -1;
}

void writeCodeSlot(int slot, const char code[]) {
  int addr = CODE_START_ADDR + (slot * CODE_SLOT_SIZE);

  EEPROM.update(addr, CODE_MARKER);
  EEPROM.update(addr + 1, code[0]);
  EEPROM.update(addr + 2, code[1]);
  EEPROM.update(addr + 3, code[2]);
}

void clearCodeSlot(int slot) {
  int addr = CODE_START_ADDR + (slot * CODE_SLOT_SIZE);

  EEPROM.update(addr, 0);
  EEPROM.update(addr + 1, 0);
  EEPROM.update(addr + 2, 0);
  EEPROM.update(addr + 3, 0);
}

void addAccessCode(const char code[]) {
  if (isValidUserCode(code) == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Invalid Code");
    delay(2000);
    return;
  }

  if (findCodeSlot(code) >= 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Code Exists");
    delay(2000);
    return;
  }

  int emptySlot = findEmptyCodeSlot();

  if (emptySlot < 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Code Full");
    delay(2000);
    return;
  }

  writeCodeSlot(emptySlot, code);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Code Saved");
  delay(2000);
}

void editAccessCode(const char oldC[], const char newC[]) {
  int oldSlot = findCodeSlot(oldC);

  if (oldSlot < 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Old Not Found");
    delay(2000);
    return;
  }

  if (isValidUserCode(newC) == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Invalid Code");
    delay(2000);
    return;
  }

  int newSlot = findCodeSlot(newC);

  if (newSlot >= 0 && newSlot != oldSlot) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Code Exists");
    delay(2000);
    return;
  }

  writeCodeSlot(oldSlot, newC);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Code Changed");
  delay(2000);
}

void deleteAccessCode(const char code[]) {
  int slot = findCodeSlot(code);

  if (slot < 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Not Found");
    delay(2000);
    return;
  }

  clearCodeSlot(slot);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Code Deleted");
  delay(2000);
}

void initDefaultCodes() {
  for (int slot = 0; slot < MAX_CODES; slot++) {
    clearCodeSlot(slot);
  }

  char code1[4] = "123";
  char code2[4] = "456";
  char code3[4] = "789";

  writeCodeSlot(0, code1);
  writeCodeSlot(1, code2);
  writeCodeSlot(2, code3);

  EEPROM.update(EEPROM_CODE_INIT_ADDR, CODE_INIT_MARKER);
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

bool isDigitKey(char key) {
  return key >= '0' && key <= '9';
}

void addInput(char key) {
  if (inputCount < 3) {
    inputText[inputCount] = key;
    inputCount++;
    inputText[inputCount] = '\0';
  }
}

void clearInput() {
  inputText[0] = '\0';
  inputText[1] = '\0';
  inputText[2] = '\0';
  inputText[3] = '\0';
  inputCount = 0;
}

void addActionDigit(char key) {
  if (actionCount < 3) {
    actionCode[actionCount] = key;
    actionCount++;
    actionCode[actionCount] = '\0';
  }
}

void clearActionCode() {
  actionCode[0] = '\0';
  actionCode[1] = '\0';
  actionCode[2] = '\0';
  actionCode[3] = '\0';
  actionCount = 0;
}

void addOldCodeDigit(char key) {
  if (oldCodeCount < 3) {
    oldCode[oldCodeCount] = key;
    oldCodeCount++;
    oldCode[oldCodeCount] = '\0';
  }
}

void clearOldCode() {
  oldCode[0] = '\0';
  oldCode[1] = '\0';
  oldCode[2] = '\0';
  oldCode[3] = '\0';
  oldCodeCount = 0;
}

void addNewCodeDigit(char key) {
  if (newCodeCount < 3) {
    newCode[newCodeCount] = key;
    newCodeCount++;
    newCode[newCodeCount] = '\0';
  }
}

void clearNewCode() {
  newCode[0] = '\0';
  newCode[1] = '\0';
  newCode[2] = '\0';
  newCode[3] = '\0';
  newCodeCount = 0;
}

void clearTimeInput() {
  timeInput[0] = '\0';
  timeInput[1] = '\0';
  timeInput[2] = '\0';
  timeInputCount = 0;
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

void saveLog(char c1, char c2, char c3, char status) {
  int nextIndex = EEPROM.read(EEPROM_NEXT_ADDR);
  int count = EEPROM.read(EEPROM_COUNT_ADDR);

  if (nextIndex >= MAX_LOGS) nextIndex = 0;
  if (count > MAX_LOGS) count = 0;

  int addr = LOG_START_ADDR + (nextIndex * LOG_SIZE);

  EEPROM.update(addr + 0, LOG_MARKER);
  EEPROM.update(addr + 1, c1);
  EEPROM.update(addr + 2, c2);
  EEPROM.update(addr + 3, c3);
  EEPROM.update(addr + 4, status);
  EEPROM.update(addr + 5, hourValue);
  EEPROM.update(addr + 6, minuteValue);
  EEPROM.update(addr + 7, secondValue);
  EEPROM.update(addr + 8, dayValue);
  EEPROM.update(addr + 9, monthValue);
  EEPROM.update(addr + 10, yearValue);
  EEPROM.update(addr + 11, weekDayValue);

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

void showLastLog() {
  int count = EEPROM.read(EEPROM_COUNT_ADDR);
  int nextIndex = EEPROM.read(EEPROM_NEXT_ADDR);

  if (count <= 0 || count > MAX_LOGS) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No Logs");
    delay(2500);
    return;
  }

  int lastIndex = nextIndex - 1;

  if (lastIndex < 0) {
    lastIndex = MAX_LOGS - 1;
  }

  showLogAtIndex(lastIndex, 1);
}

void showAllLogs() {
  int count = EEPROM.read(EEPROM_COUNT_ADDR);
  int nextIndex = EEPROM.read(EEPROM_NEXT_ADDR);

  if (count <= 0 || count > MAX_LOGS) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No Logs");
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

  char c1 = (char)EEPROM.read(addr + 1);
  char c2 = (char)EEPROM.read(addr + 2);
  char c3 = (char)EEPROM.read(addr + 3);
  char status = (char)EEPROM.read(addr + 4);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("#");
  lcd.print(number);
  lcd.print(" ");
  lcd.print(c1);
  lcd.print(c2);
  lcd.print(c3);
  lcd.print(" ");

  if (status == 'O') {
    lcd.print("OK");
  } else if (status == 'X') {
    lcd.print("NO");
  } else if (status == 'E') {
    lcd.print("EXT");
  }

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

  if (EEPROM.read(EEPROM_CODE_INIT_ADDR) != CODE_INIT_MARKER) {
    initDefaultCodes();
  }
}

void setRTCTime(byte second, byte minute, byte hour, byte weekDay, byte day, byte month, byte year) {
  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write(0);

  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(weekDay));
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
  weekDayValue = bcdToDec(Wire.read());
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

void allColumnsLow() {
  digitalWrite(C1, LOW);
  digitalWrite(C2, LOW);
  digitalWrite(C3, LOW);
}