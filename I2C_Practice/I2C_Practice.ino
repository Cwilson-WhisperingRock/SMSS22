// Purpose : Communicate with the raspberry pi as a sub device
// Context : Pi will give the arduino a number and the Duino will repond with bool if its divisable by 2

#include <Wire.h>

// Arduino Addresses
#define PI_ADD 0x01
#define ARD_ADD_1 0x14                 // This device
#define ARD_ADD_2 0x28
#define ARD_ADD_3 0x42

bool EVEN = false;
char i2c_error = 0;

void setup() {
  Wire.begin(ARD_ADD_2);              // ARD device joins I2C bus as sub with address
  Wire.onRequest(receivedEvent);      // Go to function is requested
  Serial.begin(9600);                 // Serial for user
}

void loop() {
  delay(100);
  }

void receivedEvent(int numBytes_TX){
  int x = Wire.read();

  if( x %2 == 0){EVEN = true;}
  else{EVEN = false;}

  Wire.beginTransmission(PI_ADD);
  Wire.write(EVEN);
  i2c_error = Wire.endTransmission(true);

  if(i2c_error == 4){Serial.println("Other Errors");}
  else if (i2c_error == 3){Serial.println("NACK on TX Data");}
  else if (i2c_error == 2){Serial.println("NACK on TX Address");}
  else if (i2c_error == 1){Serial.println("Data too long in buffer");}
  else if (i2c_error == 0){Serial.println("Sucessful TX");}
  
}
