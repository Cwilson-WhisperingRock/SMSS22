#include <TimerOne.h>         // Custom PWM freq
#include <Wire.h>             // I2C 


// Test Variables
#define MANUAL                // Comment to cancel manual mode
#define TESTING               // Comment to cancel testing
#define SERIAL_COMMS          // Comment to cancel serial comms
#define I2C_COMMS             // Comment to cancel I2C comms


// State machine 
enum State_enum {STANDBY, DATAPULL, CALC, RUN_S, RUN_J, STOP};           // States of the state machine
enum Signal_enum {ESTOP, START, JOG, BACK};                     // signals for traversing 
unsigned int state = STANDBY;                                   // default state 
bool TICKET = false;                                            // Confirmation to run the motor after data RX
bool FINISH = false;                                            // User def duration has been reached
 

// Pin Connections
#define STEP 9                // PWM signal for motor
#define DIR 6                 // Direction of motor
#define STBY 2                // Controller's standby enable (both EN and STBY must be low for low power mode)
#define M1 7                  // microstep config bit 1
#define M2 8                  // microstep config bit 2
// NOTE : STEP, DIR, M2, M1 are all used to set the microstep in the init latching 
//        but only STEP and DIR are availible after init


// Common States
#define FORWARD LOW           // motor direction
#define REVERSE HIGH          // motor direction


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
char step_config = S_FULL;          // Set default to full step


// Pulse/Freq Variables
#define DC  0.5 * 1024              // Duty Cycle of PWM
#define MTR_FREQ_MAX 681            // Max freq motor can handle [Hz]
#define MTR_FREQ_MIN 120            // Min freq " 


// Physical Variables
#define INTERNAL_STEP 200           // Internal step size of the motor
#define GEAR_RATIO  99.05           // gear ratio of the planetary gear
#define THREAD_PITCH  0.005         // thread pitch of the actuator's screw [m]
float syr_radius_user = 0.010845;   // RADIUS of the 30ml BD syringe [m]
float q_user = 0.00001;             // volumetric flow rate as requested by user [L/s]
float freq = 200;                   // [sec/step]
unsigned int sec_user = 0;          // User requested sec duration
unsigned int min_user = 0;          // User requested min duration
unsigned int hour_user = 0;         // User "         hour "






void setup() {

  Serial.begin(9600);                           // Serial comms init

  //I2C comms setup

  pinMode(STEP, OUTPUT);                        // Setup controller pins
  pinMode(DIR, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  digitalWrite(DIR, REVERSE);                   // Direction of motor

  
  init_control(step_config);                    // init controller
  delay(10);

      #ifdef MANUAL
      //q_user = 0.00000006944444444;             // Manually set the Q from code 
      q_user = 0.00001;
      #endif

  freq =  Q_to_Freq(q_user, syr_radius_user);     // Get period from Q

      #ifdef TESTING
      Serial.println(freq,8);
      #endif
    
  
  Timer1.initialize((1/freq) * 1000000);          // Init custom PWM freq[microsec] (mult to conv to usec)

      #ifdef MANUAL
      Timer1.pwm(STEP, DC);                  // Pulse the motor 
      #endif
    
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





/*~~~~~~~~~~~~~~~~~~~~~~~ Q_to_Freq() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : Convert the requested Q rate into a frequency for the stepper motor 
 *  Input : q_user, syr_radius, Physical quantities
 *  Outut : freq 
 */
float Q_to_Freq( float Q, float Radius){
  
  float freq = ( Q * INTERNAL_STEP * GEAR_RATIO) / (500 * Radius * Radius * THREAD_PITCH);
  return freq;
}





/*~~~~~~~~~~~~~~~~~~~~~~~~ MicroCali() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : Recalibrate the md39a motor controller's microstep setting to accomidate the min freq recommendation
 *            Max freq will result in Full step at max freq + warning
 *  Input : frequency, [global] step_config, [global] MTR_FREQ_MAX, [global] MTR_FREQ_MIN
 *  Output : frequency, [global] step_config
 *  Notes : Removes decimal point to save memory
 */
double MicroCali( float frequency ){


  // If greater than the max frequency
  if ( frequency > MTR_FREQ_MAX){
    step_config = S_FULL;                                                 // Set controller to full step

        #ifdef MANUAL
        Serial.println(" Requested Q value exceeds hardware's MAX RPS");  // Inform user
        #endif
        
    return MTR_FREQ_MAX;                                                  // Set freq to max
  }

  // Or if less than the min freq
  else if (frequency < MTR_FREQ_MIN){
    
    double microstep = MTR_FREQ_MIN / frequency;                 // Microstep size 
    
    if ( microstep <= 2 ){                                                // Half step
      step_config = S_HALF;                                               // Set controller to half step
      return 2 * frequency;                                               // double freq
    }

    else if ( microstep <= 4 ){
      step_config = S_QRTR;                                               // Set controller to half step
      return 4 * frequency;                                               // double freq
    }

    else if ( microstep <= 8 ){
      step_config = S_8;                                               
      return 8 * frequency;                                               
    }

    else if ( microstep <= 16 ){
      step_config = S_16;                                               
      return 16 * frequency;                                               
    }

    else if ( microstep <= 32 ){
      step_config = S_32;                                               
      return 32 * frequency;                                               
    }

    else if ( microstep <= 64 ){
      step_config = S_64;                                               
      return 64 * frequency;                                               
    }

    else if ( microstep <= 128 ){
      step_config = S_128;                                              
      return 128 * frequency;                                              
    }

    else if ( microstep <= 256 ){
      step_config = S_256;                                              
      return 256 * frequency;                                             
    }

    else{
      step_config = S_256;                                                 // Set controller to smallest step

        #ifdef MANUAL
        Serial.println(" Requested Q value is below hardware's MIN RPS");  // Inform user
        #endif
        
    return MTR_FREQ_MIN;                                                  // Set freq to min
      
    }
 }

 // No microstepping change needed
 else{
  return frequency;
 }
   
}




/* ~~~~~~~~~~~~~~~~~~~~~~~~ state_machine() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : Control the flow of the motor controller 
 *  Input : User input signals
 *  Output : 
 */

 void state_machine(uint8_t signals){
  
  switch(state){
    
    case STANDBY:
        
        // Serial GUI

        // read serial/I2C

        //logic placement
        
        break;

    case DATAPULL:
        break;

    case CALC:
        break;

    case RUN_J:
        break;


    case RUN_S:
        break;

    case STOP:
        break;

    default:
        state = STOP;
        break;

    
  }
 }
