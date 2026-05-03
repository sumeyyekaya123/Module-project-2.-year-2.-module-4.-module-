#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);

  Serial.println("I2C Scanner Started");
}

void loop() {
  byte error;
  byte address;
  int deviceCount = 0;

  Serial.println("Scanning...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");

      if (address < 16) {
        Serial.print("0");
      }

      Serial.println(address, HEX);
      deviceCount++;
    }
  }

  if (deviceCount == 0) {
    Serial.println("No I2C devices found");
  } else {
    Serial.println("Scan finished");
  }

  Serial.println();
  delay(3000);
}