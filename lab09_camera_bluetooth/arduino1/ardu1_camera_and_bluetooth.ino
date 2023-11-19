////////////////////////////////////////////////////////////////////////////////
// AUE3034, Lab 09: Camera
//  - Master (Send an image to another arduino via bluetooth)
//
// More camera functions can be found in ArduCam/arduino github repo
// : https://github.com/ArduCAM/Arduino/examples/mini/
//
// Adopted from "ArduCAM Mini demo" (C) 2017 Lee
// Web: http://www.ArduCAM.com
// This program is a demo of how to use most of the functions
// of the library with ArduCAM Mini camera, and can run on any Arduino platform.
// This demo was made for ArduCAM_Mini_2MP.
// It needs to be used in combination with PC software.
////////////////////////////////////////////////////////////////////////////////

#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include <SoftwareSerial.h>

// [TODO] Open below header file (memorysaver.h) and 
//        Select your device (#define OV2640_MINI_2MP)
//        Select your device (#define OV5642_MINI_5MP)
// #include "memorysaver.h"
#if !(defined OV5642_MINI_5MP || defined OV2640_MINI_2MP)
  #error Please select the hardware platform and camera module in the memorysaver.h
#endif

// Image will be sent to the PC via Bluetooth by SoftwareSerial
SoftwareSerial btSerial(2, 3);

// Set pin 7 as the slave select for the camera (OV2640)
const int CS = 7;
bool is_header = false;
int mode = 0;
uint8_t start_capture = 0;

#if defined (OV2640_MINI_2MP)
  ArduCAM myCAM( OV2640, CS );
#elif defined (OV5642_MINI_5MP)
  ArduCAM myCAM( OV5642, CS );  
#endif

uint8_t read_fifo_burst(ArduCAM myCAM);

void setup() {  
  // initialize Bluetooth
  btSerial.begin(38400);

  // initialize serial comm. (for debugging)
  Serial.begin(9600);
  Serial.println(F("ACK CMD ArduCAM Start! END"));
  
  // set the CS as an output (Camera, SPI)
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);
  
  // initialize SPI
  Wire.begin();
  SPI.begin();

  // reset the CPLD (by camera manufacturer)  
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);
  
  uint8_t temp;
  while(1){
    // check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55){
      Serial.println(F("ACK CMD SPI interface Error! END"));
      delay(1000);
      continue;
    }
    else{
      Serial.println(F("ACK CMD SPI interface OK. END"));
      break;
    }
  }

  uint8_t vid, pid;  
  while(1){
    #if defined (OV2640_MINI_2MP)
      // check if the camera module type is OV2640
      myCAM.wrSensorReg8_8(0xff, 0x01);      
      myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
      myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

      if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
        Serial.println(F("ACK CMD Can't find OV2640 module! END"));
        delay(1000);
        continue;
      }
      else{
        Serial.println(F("ACK CMD OV2640 detected. END"));
        break;
      } 
    #elif defined (OV5642_MINI_5MP)
      //Check if the camera module type is OV5642
      myCAM.wrSensorReg16_8(0xff, 0x01);
      myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
      myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
      
      if((vid != 0x56) || (pid != 0x42)){
        Serial.println(F("ACK CMD Can't find OV5642 module! END"));
        delay(1000);
        continue;
      }
      else{
        Serial.println(F("ACK CMD OV5642 detected. END"));
        break;
      }
    #endif
  }
  
  // change to JPEG capture mode
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  
  #if defined (OV2640_MINI_2MP)
    myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  #elif defined (OV5642_MINI_5MP)
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
    myCAM.OV5642_set_JPEG_size(OV5642_320x240);    
  #endif
  
  delay(1000);
  myCAM.clear_fifo_flag(); 

  #if !(defined (OV2640_MINI_2MP))
    myCAM.write_reg(ARDUCHIP_FRAMES,0x00);
  #endif
}


void loop() {
  uint8_t temp = 0xff, temp_last = 0;
  bool is_header = false;

  if (btSerial.available()){
    // to support bluetooth connection test
    temp = btSerial.read();
    Serial.write(temp);
  }
  
  if (Serial.available()){    
    temp = Serial.read();    
    if(temp == 'c'){
      // if a user sends a command via serial comm. (PC 1 <-> Arduino 1)
      // start grabbing an image frame
      mode = 1;
      temp = 0xff;
      start_capture = 1;
      Serial.println(F("ACK CMD CAM start single shoot. END")); 
    }
  }  
    
  if (start_capture == 1){
    myCAM.flush_fifo();
    myCAM.clear_fifo_flag();    
    myCAM.start_capture();
    start_capture = 0;
  }
  
  if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)){
    Serial.println(F("ACK CMD CAM Capture Done. END"));
    delay(50);
    read_fifo_burst(myCAM);
  
    //Clear the capture done flag
    myCAM.clear_fifo_flag();
  }
}


uint8_t read_fifo_burst(ArduCAM myCAM){
  uint8_t temp = 0, temp_last = 0;
  uint32_t length = 0;
  
  length = myCAM.read_fifo_length();
  Serial.println(length, DEC);
  if (length >= MAX_FIFO_SIZE){ //512 kb
    Serial.println(F("ACK CMD Over size. END"));
    return 0;
  }
  if (length == 0){ //0 kb
    Serial.println(F("ACK CMD Size is 0. END"));
    return 0;
  }

  // SPI comm: master makes CS pin LOW to select the slave
  myCAM.CS_LOW();

  //Set fifo burst mode
  myCAM.set_fifo_burst(); 

  // Send a dummy byte to receive a byte from the camera
  temp = SPI.transfer(0x00);
  length--;
  while ( length-- ){
    temp_last = temp;
    temp = SPI.transfer(0x00);
    if (is_header == true){      
      btSerial.write(temp);
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF)){
      is_header = true;
      Serial.println(F("ACK IMG END"));
      btSerial.write(temp_last);
      btSerial.write(temp);
    }

    if ((temp == 0xD9) && (temp_last == 0xFF)){
      break;
    }
      
    delayMicroseconds(15);
  }
  // SPI comm: master makes CS pin HIGH when the data transfer is completed
  myCAM.CS_HIGH();

  is_header = false;
  return 1;
}

