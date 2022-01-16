#include <TimerOne.h>
#include <Wire.h>


// Test Variables
#define MANUAL      // Comment to cancel manual mode
#define TESTING     // Comment to cancel testing 

// Pin Connections
#define STEP 9      // PWM signal for motor
#define DIR 6       // Direction of motor
#define STBY 2      // Controller's standby enable (both EN and STBY must be low for low power mode)
#define M1 7        // microstep config bit 1
#define M2 8        // microstep config bit 2
// NOTE : STEP, DIR, M2, M1 are all used to set the microstep in the init latching 
//         but only STEP and DIR are availible after init



// Common States
#define FORWARD LOW    // motor direction
#define REVERSE HIGH     // motor direction


// Microstep settings  [ M1, M2, STEP, DIR ]
#define S_FULL  0x0
#define S_HALF  0xA
#define S_QRTR  0x5
#define S_8     0xE
#define S_16    0xF
#define S_32    0x4
#define S_64    0xD
#define S_128   0x8
#define S_256   0x6


// Pulse/Freq Variables
#define DutyCycle  0.5 * 1024
#define MTR_FREQ_MAX 20000          // Max freq motor can handle [Hz]
#define MTR_FREQ_MIN 1              // Min freq " 


// Physical Variables
#define INTERNAL_STEP 200           // Internal step size of the motor
#define GEAR_RATIO  99.05             // gear ratio of the planetary gear
#define THREAD_PITCH  0.005         // thread pitch of the actuator's screw [m]
float syr_radius = 0.010845;           //RADIUS of the 30ml BD syringe [m]
float q_user = 0.001;               // volumetric flow rate as requested by user [L/s]
float period = 0.01;        // [sec/step]




void setup() {

  Serial.begin(9600);               // Serial comms init

  //I2C comms setup

  pinMode(STEP, OUTPUT);            // Setup controller pins
  pinMode(DIR, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);

  char step_config = S_FULL;        // Set default to full step
  init_control(step_config);        // init controller

    #ifdef MANUAL
    //q_user = 0.00000006944444444;                 // Manually set the Q from code  0.00000006944444444
    q_user = 0.0000105;
    #endif

  period = Q_to_Period(q_user, syr_radius);     // Get period from Q

    #ifdef TESTING
    Serial.println(1/period,8);
    #endif
    
  digitalWrite(DIR, REVERSE);                   // Direction of motor
  Timer1.initialize(period * 1000000);          // Init custom PWM freq[microsec] (mult to conv to usec)
  Timer1.pwm(STEP, DutyCycle);                  // Pulse the motor 
    
}

void loop() {

 

}




/*~~~~~~~~~~~~~~~~~~~~~~~ init_config() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : initialize or change the md39a controllers step size
 *  Input : 0x4'b  [ M1, M2, STEP, DIR ] as calculated by the requested step setting
 *  Outut : 
 *  Notes : ~EN pin is held low 
 */
void init_control(char pin_config) {

  //The recommended power-up sequence is following:
  
  //1. Power-up the device applying the VS supply voltage but keeping both STBY and EN/FAULT inputs low.
  digitalWrite(STBY, LOW);
  
  //2. Set the MODEx inputs according to the target step resolution (see Table 1).
  digitalWrite(DIR, pin_config & 1);
  digitalWrite(STEP, (pin_config >> 1) & 1);
  digitalWrite(M2, (pin_config >> 2) & 1);
  digitalWrite(M1, (pin_config >> 3) & 1);
  
  //3. Wait for at least 1 µs (minimum tMODEsu time).
  delayMicroseconds(1);
  
  //4. Set the STBY high. The MODEx configuration is now latched inside the device.
  digitalWrite(STBY, HIGH);
  
  //5. Wait for at least 100 µs (minimum tMODEho time).
  delayMicroseconds(100);
  
  //6. Enable the power stage releasing the EN/FAULT input and start the operation.
}





/*~~~~~~~~~~~~~~~~~~~~~~~ Q_to_Period() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : Convert the requested Q rate into a frequency for the stepper motor 
 *  Input : q_user, syr_radius, Physical quantities
 *  Outut : period 
 */
float Q_to_Period( float Q, float Radius){
  
  float period = (500 * Radius * Radius * THREAD_PITCH) /  ( Q * INTERNAL_STEP * GEAR_RATIO) ;
  return period;
}
