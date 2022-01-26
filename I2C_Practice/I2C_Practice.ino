// Purpose : Communicate with the raspberry pi as a sub device
// Context : Pi will give the arduino a number and the Duino will repond with bool if its divisable by 2

#include <Wire.h>

// Arduino Addresses
#define PI_ADD 0x01
#define ARD_ADD_1 0x7                 // This device
#define ARD_ADD_2 0x14
#define ARD_ADD_3 0x28

bool EVEN = false;

void setup() {
  Wire.begin(ARD_ADD_1);              // ARD device joins I2C bus as sub with address
  Serial.begin(9600);                 // Serial for user
}

void loop() {

Wire.beginTransmission(44); // transmit to device #44 (0x2c)
                              // device address is specified in datasheet
  Wire.write(val);             // sends value byte  
  Wire.endTransmission();     // stop transmitting

  val++;        // increment value
  if(val == 64) // if reached 64th position (max)
  {
    val = 0;    // start over from lowest value
  }
  delay(500);
}


void I2C_TX
