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
  Wire.onReceive(receivedEvent);      // Go to function is requested
  Serial.begin(9600);                 // Serial for user
  Serial.println("work");
  //Wire.onRequest(receivedReq);
}

void loop() {

  }

void receivedEvent(int numBytes_TX){

  Serial.println("Rc");
  
  int x = 0;

  //Serial.println("Just waiting now");
  
   x = Wire.read();  
   Serial.println(x);

  
  if( x %2 == 0){
    EVEN = true;
    Serial.println("Even");
    }
  else{
    EVEN = false;
    Serial.println("Odd");
    }

  
  delay(1000);

  
  Wire.beginTransmission(PI_ADD);
  Wire.write(EVEN);
  i2c_error = Wire.endTransmission(true);

  if(i2c_error == 4){Serial.println("Other Errors");}
  else if (i2c_error == 3){Serial.println("NACK on TX Data");}
  else if (i2c_error == 2){Serial.println("NACK on TX Address");}
  else if (i2c_error == 1){Serial.println("Data too long in buffer");}
  else if (i2c_error == 0){Serial.println("Sucessful TX");}

  
  
}

void receivedReq(int numBytes_TX){

  Serial.println("Rq");

  Wire.beginTransmission(PI_ADD);
  Wire.write(EVEN);
  i2c_error = Wire.endTransmission(true);

  if(i2c_error == 4){Serial.println("Other Errors");}
  else if (i2c_error == 3){Serial.println("NACK on TX Data");}
  else if (i2c_error == 2){Serial.println("NACK on TX Address");}
  else if (i2c_error == 1){Serial.println("Data too long in buffer");}
  else if (i2c_error == 0){Serial.println("Sucessful TX");}
  
}
