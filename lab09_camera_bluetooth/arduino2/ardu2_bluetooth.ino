#include <SoftwareSerial.h>

SoftwareSerial btSerial(2, 3);

void setup() {
  Serial.begin(38400);
  btSerial.begin(38400);  
}

void loop() {
  if(btSerial.available()){
    char received = btSerial.read();
    Serial.write(received);
  }

  if(Serial.available()){
    char transmitted = Serial.read();
    btSerial.write(transmitted);
  }
}

