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