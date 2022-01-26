#include <TimerOne.h>         // Custom PWM freq
#include <Wire.h>             // I2C 


// Test Variables
#define MANUAL                // Comment to cancel manual mode
#define TESTING               // Comment to cancel testing
#define SERIAL_COMMS          // Comment to cancel serial comms
//#define I2C_COMMS             // Comment to cancel I2C comms


// State machine 
enum State_enum {STANDBY, DATAPULL_S, DATAPULL_J, RUN_S, RUN_J, STOP};  // States of the state machine
unsigned int state = STANDBY;                                                 // default state 
#define START 'S'
#define JOG 'J'
#define BACK 'B'
#define ESTOP 'E'
bool TICKET = false;
bool FINISH = false; 

 

// Pin Connections
#define STEP 9                // PWM signal for motor
#define DIR 6                 // Direction of motor
#define STBY 2                // Controller's standby enable (both EN and STBY must be low for low power mode)
#define M1 7                  // microstep config bit 1
#define M2 8                  // microstep config bit 2
// NOTE : STEP, DIR, M2, M1 are all used to set the microstep in the init latching 
//        but only STEP and DIR are availible after init


// LED State Indicators
#define YELLOW 3
#define GREEN 4
#define WHITE 10 
#define BLUE 11


// Motor States
#define FORWARD LOW           // motor direction
#define REVERSE HIGH          // motor direction
bool DIRECTION = FORWARD;
bool RUNNING  = false;        // status of motor


// Microstep settings for md39a[ M1, M2, STEP, DIR ]
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
#define DC  0.6 * 1024              // Duty Cycle of PWM
#define MTR_FREQ_MAX 875            // Max freq motor can handle [Hz]
#define MTR_FREQ_MIN 120            // Min freq " 


// Physical Variables
#define INTERNAL_STEP 200                   // Internal step size of the motor
#define GEAR_RATIO  99.05                   // gear ratio of the planetary gear
#define THREAD_PITCH  0.005                 // thread pitch of the actuator's screw [m]
unsigned int syr_capacity = 30;             // capacity of syringe in mL
float syr_radius_user = 0.010845;           // RADIUS of the 30ml BD syringe [m]
float q_user = 0;                           // volumetric flow rate as requested by user [L/s]
float freq = 200;                           // [sec/step]
unsigned int sec_user = 0;                  // User requested sec duration
unsigned int min_user = 0;                  // User requested min duration
unsigned long hour_user = 0;                 // User "         hour "
unsigned long duration = 0;                 // Run time {msec}
unsigned long time_stamp_start = 0;          
unsigned long time_stamp_end = 0;           


// Serial/I2C Variables
char receivedChar;
boolean newData_char = false;
boolean newData_str = false;
const byte numChars = 32;
char receivedChars[numChars];               // an array to store the received data
bool displayed = false;





void setup() {

  Serial.begin(9600);                           // Serial comms init

  //I2C comms setup

  pinMode(STEP, OUTPUT);                        // Setup controller pins
  pinMode(DIR, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);

  pinMode(BLUE, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(WHITE, OUTPUT);
  digitalWrite(DIR, REVERSE);                   // Direction of motor
  
}

void loop() {
  state_machine();
}




/*~~~~~~~~~~~~~~~~~~~~~~~ init_config() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : initialize or change the md39a controllers step size
 *  Input : 0x4'b  [ M1, M2, STEP, DIR ] as calculated by the requested step setting
 *  Outut : digital write to pins
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
 *  Output : frequency, [global] step_config, [global] TICKET
 *  Notes : Removes decimal point to save memory
 */
double MicroCali( float frequency ){


  // If greater than the max frequency
  if ( frequency > MTR_FREQ_MAX){
    
    step_config = S_FULL;                                                 // Set controller to full step
    TICKET = false;                                                         // Mark ticket invalid
    
        #ifdef MANUAL
        Serial.println(" Requested Q value exceeds hardware's MAX RPS");  // Inform user
        #endif
        
    return MTR_FREQ_MAX;                                                  // Set freq to max
  }

  // Or if less than the min freq
  else if (frequency < MTR_FREQ_MIN){
    
    double microstep = MTR_FREQ_MIN / frequency;                          // Microstep size 
    
    if ( microstep <= 2 ){                                                // Half step
      step_config = S_HALF;                                               // Set controller to half step
      TICKET = true;                                                      // Approved ticket
      return 2 * frequency;                                               // double freq
    }

    else if ( microstep <= 4 ){
      step_config = S_QRTR;                                               // Set controller to half step
      TICKET = true;                                                      // Approved ticket
      return 4 * frequency;                                               // double freq
    }

    else if ( microstep <= 8 ){
      step_config = S_8;
      TICKET = true;                                                      // Approved ticket                                               
      return 8 * frequency;                                               
    }

    else if ( microstep <= 16 ){
      step_config = S_16;
      TICKET = true;                                                      // Approved ticket                                               
      return 16 * frequency;                                               
    }

    else if ( microstep <= 32 ){
      step_config = S_32; 
      TICKET = true;                                                      // Approved ticket                                              
      return 32 * frequency;                                               
    }

    else if ( microstep <= 64 ){
      step_config = S_64;
      TICKET = true;                                                      // Approved ticket                                               
      return 64 * frequency;                                               
    }

    else if ( microstep <= 128 ){
      step_config = S_128;
      TICKET = true;                                                      // Approved ticket                                              
      return 128 * frequency;                                              
    }

    else if ( microstep <= 256 ){
      step_config = S_256;
      TICKET = true;                                                      // Approved ticket                                              
      return 256 * frequency;                                             
    }

    else{
      step_config = S_256;                                                 // Set controller to smallest step
      TICKET = false;
      
        #ifdef MANUAL
        Serial.println(" Requested Q value is below hardware's MIN RPS");  // Inform user
        #endif
        
    return MTR_FREQ_MIN;                                                  // Set freq to min
      
    }
 }

 // No microstepping change needed
 else{
  TICKET = true;                                                          // Approved ticket
  return frequency;
 }
   
}




/* ~~~~~~~~~~~~~~~~~~~~~~~~ state_machine() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : Control the flow of the motor controller 
 *  Input : User input signals
 *  Output : 
 */
 void state_machine(){
  
  switch(state){

    
    
    case STANDBY:

    digitalWrite(YELLOW, HIGH);         // LED indicator

    
        #ifdef SERIAL_COMMS
        
            if(displayed == false){                                           // dont print unless its the first
            Serial.println(" ~~~~~~~~~~~~Standby Mode~~~~~~~~~~~ ");
            Serial.println(" Press S for START mode or J for JOG (No line ending) \n\n\n\n");
            displayed = true;
            }
            
            recvOneChar();
           
        #endif

            if (receivedChar == START){                               // User selects START for normal use
              
              #ifdef SERIAL_COMMS
              displayed = false;                                      //graphic already displayed...no more pls
              #endif

              digitalWrite(YELLOW, LOW);                             // LED indicator
              state = DATAPULL_S;}  
                        
            else if(receivedChar == JOG){                              // User selects JOG to jog motor
              
              #ifdef SERIAL_COMMS
              displayed = false;
              #endif
              
              digitalWrite(YELLOW, LOW);                             // LED indicator
              state = DATAPULL_J;}
                      
            else{state = STANDBY;}                                     // Or wait until user chooses
                                                                           
      
        break;

    case DATAPULL_S:

        digitalWrite(BLUE, HIGH);                             // LED indicator

        #ifdef SERIAL_COMMS
            Serial.println(" ~~~~~~~~~~DATAPULL_S~~~~~~~~~ ");
            Serial.println(" What is the fluid capacity of the syringe[mL]? (newline)");
            while(newData_str == false){recvWithEndMarker();}                                                     
            syr_capacity = userString2Float();
            
            Serial.println(" What is the radius of the syringe plunger[m]? (newline)");
            while(newData_str == false){recvWithEndMarker();}
            syr_radius_user = userString2Float();
            
            Serial.println(" What is the requested volumetric flow rate Q [L/s] ? Range [69n , 10u] (newline)");
            while(newData_str == false){recvWithEndMarker();}
            q_user = userString2Float();
            
            Serial.println(" Duration hours? (newline)");
            while(newData_str == false){recvWithEndMarker();}
            hour_user = userString2Float();
            
            Serial.println(" Duration minutes? (newline)");
            while(newData_str == false){recvWithEndMarker();}
            min_user = userString2Float();
            
            Serial.println(" Duration seconds? (newline)");
            while(newData_str == false){recvWithEndMarker();}
            sec_user = userString2Float();
            
            Serial.println(" Direction? F for forward, R for reverse, or B to back to Standby (No line end)");
            newData_char = false;
            while(newData_char == false){recvOneChar();}
            if( receivedChar == 'R'){
              DIRECTION = REVERSE; 

              #ifdef TESTING
                  Serial.println(" Capacity, raduis, Q, hour, min, sec, Dir ");
                  Serial.print(syr_capacity); 
                  Serial.print(", ");
                  Serial.print(syr_radius_user, 8);
                  Serial.print(", ");
                  Serial.print(q_user, 10);
                  Serial.print(", ");
                  Serial.print(hour_user);
                  Serial.print(", ");
                  Serial.print(min_user);
                  Serial.print(", ");
                  Serial.print(sec_user);
                  Serial.print(", ");
                  Serial.println("Reverse\n\n");
              #endif
              }
            else if( receivedChar == 'F'){
              DIRECTION = FORWARD;
              
              #ifdef TESTING
                  Serial.println(" Capacity, raduis, Q, hour, min, sec, Dir ");
                  Serial.print(syr_capacity); 
                  Serial.print(", ");
                  Serial.print(syr_radius_user, 8);
                  Serial.print(", ");
                  Serial.print(q_user, 10);
                  Serial.print(", ");
                  Serial.print(hour_user);
                  Serial.print(", ");
                  Serial.print(min_user);
                  Serial.print(", ");
                  Serial.print(sec_user);
                  Serial.print(", ");
                  Serial.println("Forward\n\n");
              #endif
              
              }
        #endif

        duration = ( (3600 * hour_user) + (60 * min_user) + sec_user) * 1000;   // entire duration in msec for easy compare later
        
        freq =  Q_to_Freq(q_user, syr_radius_user);                             // Get period from Q
        freq = MicroCali(freq);                                                 // Calibrate for microstepping + TICKET
      
        if(duration == 0){TICKET = false;}
        else if(TICKET == true){validDur_Start(q_user, syr_capacity, duration);}     // final TICKET confirm duration 
        
        if(receivedChar == BACK){
          digitalWrite(BLUE, LOW);                                              // LED indicator
          state = STANDBY;}                                                     // User requests to go back
        else if(TICKET){                                                        // Must lie within Q and duration range
          digitalWrite(BLUE, LOW);                                               // LED indicator
          init_control(step_config);                                            // Calibrate controller
          state = RUN_S;}                                                       // Valid data, move forward
        else{state = DATAPULL_S;}                                               // Repeat if the data wasnt RX/right
        
        break;


    case DATAPULL_J:
        digitalWrite(BLUE, HIGH);                                               // LED indicator
        digitalWrite(WHITE, HIGH);                                              // LED indicator
        
        #ifdef SERIAL_COMMS
            
            Serial.println(" \n\n\n\n~~~~~~~~~~DATAPULL_JOG~~~~~~~~~ ");
            Serial.println(" What is the radius of the syringe plunger [m]? (newline)");
            while(newData_str == false){recvWithEndMarker();}                                                     
            syr_radius_user = userString2Float();
            
            Serial.println(" What is the requested volumetric flow rate (Q) ? Range [69n , 10u] L/s (newline)");
            while(newData_str == false){recvWithEndMarker();} 
            q_user = userString2Float();
            
            Serial.println(" Direction? F for forward, R for reverse, or B to back to Standby (No line ending)");
            newData_char = false;
            while(newData_char == false){recvOneChar();}
            if( receivedChar == 'R'){DIRECTION = REVERSE; }
            else if( receivedChar == 'F'){DIRECTION = FORWARD;} 
        #endif

          
              
        freq =  Q_to_Freq(q_user, syr_radius_user);                             // Get period from Q

        #ifdef TESTING
                  Serial.println("radius, Q, freq ");
                  Serial.print(syr_radius_user, 8);
                  Serial.print(", ");
                  Serial.print(q_user, 10);
                  Serial.print(", ");
                  Serial.println(freq);
          #endif

          
        freq = MicroCali(freq);                                                 // Calibrate microstepping & TICKET validation


        #ifdef TESTING
                  Serial.println("radius, Q, freq ");
                  Serial.print(syr_radius_user, 8);
                  Serial.print(", ");
                  Serial.print(q_user, 10);
                  Serial.print(", ");
                  Serial.println(freq);

          #endif

          
        if(receivedChar == BACK){                                               // User requests to go back
              //reset metrics and go back
              freq = 0;
              digitalWrite(BLUE, LOW);                                               // LED indicator
              digitalWrite(WHITE, LOW);                                              // LED indicator
              state = STANDBY;
        }
        else if(TICKET == true){
          digitalWrite(BLUE, LOW);                                               // LED indicator
          digitalWrite(WHITE, LOW);                                              // LED indicator
          state = RUN_J;}                                                       // Valid data, move forward
        else{state = DATAPULL_J;}                                               // Repeat if the data wasnt RX/right
        
        break;


    case RUN_J:

        digitalWrite(GREEN, HIGH);                                               // LED indicator
        digitalWrite(WHITE, HIGH);                                              // LED indicator
        
        // Allowing arduino to roam through cycles and not wait for user input while running
        #ifdef SERIAL_COMMS
            if(displayed == false){
              Serial.println(" ~~~~~~~~~~~~ Run_JOG ~~~~~~~~~~~ ");
              Serial.println(" Press E for ESTOP (No line ending)");
              displayed = true;
            }
            recvOneChar();
        #endif
        
        if(receivedChar == ESTOP){
            digitalWrite(GREEN, LOW);                                               // LED indicator
            digitalWrite(WHITE, LOW);                                              // LED indicator
            state = STOP;
        }
        
        else if (RUNNING == false){
            init_control(step_config);                              // setup motor controller
            digitalWrite(DIR, DIRECTION);                           // rewrite DIR pin for direction
            Timer1.initialize((1/freq) * 1000000);                  // Init PWM freq[microsec] (mult to conv to usec)
            Timer1.pwm(STEP, DC);                                   // Start motor  
            RUNNING = true;
            state = RUN_J;
        }
        
        else{
          
          state = RUN_J;
          }
          
        break;


    case RUN_S:
     digitalWrite(GREEN, HIGH);                                               // LED indicator


        #ifdef SERIAL_COMMS
            if(displayed == false){
              Serial.println(" ~~~~~~~~~~~~ Run_START ~~~~~~~~~~~ ");
              Serial.println(" Press E for ESTOP (No line ending)\n\n\n\n");
              displayed = true;
            }
            recvOneChar();
        #endif
        
        if(receivedChar == ESTOP || FINISH == true){
          
            #ifdef SERIAL_COMMS
              displayed = false;
            #endif
            digitalWrite(GREEN, LOW);                                               // LED indicator
            state = STOP;
        }

        // init run of motor
        else if (RUNNING == false){
            init_control(step_config);                              // setup motor controller
            digitalWrite(DIR, DIRECTION);                           // rewrite DIR pin for direction
            Timer1.initialize((1/freq) * 1000000);                  // Init PWM freq[microsec] (mult to conv to usec)
            time_stamp_start = millis();                            // Start recording time
            Timer1.pwm(STEP, DC);                                   // Start motor  
            RUNNING = true;
            state = RUN_S;
        }

        // Running until final time
        else{

          time_stamp_end = millis();
          time_stamp_end = time_stamp_end - time_stamp_start;       // total elapsed time
          
          if( time_stamp_end >= duration){ FINISH = true;}          // if time had reached user's request
        
        
        state = RUN_S;
        }
        
        break;

    case STOP:
        Timer1.disablePwm(STEP);                                             // turn off motor
        RUNNING = false;
        TICKET = false;
        displayed = false;
        FINISH = false;
        q_user = 0;                                                   // clear user variables
        syr_radius_user = 0;
        freq = 0;
        duration = 0; 
        state = STANDBY;
        break;

    default:
        state = STOP;
        break;

    
  }
 }




/*~~~~~~~~~~~~~~~~~~~~~~~~~ recvOneChar() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Purpose : Receive one char from the user
 * Input : serial
 * Output : [global] newData
 */
void recvOneChar() {
    if (Serial.available() > 0) {
        receivedChar = Serial.read();
        newData_char = true;
    }
}




/*~~~~~~~~~~~~~~~~~~~~~~~~ recvWithEndMarker() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Purpose : Receive set of char (up to 32) from the user
 * Input : serial
 * Output : [global] receivedChars[]
 */
void recvWithEndMarker() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
 
    
    while (Serial.available() > 0 && newData_str == false) {
        rc = Serial.read();

        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData_str = true;
        }
    }

    
}




/*~~~~~~~~~~~~~~~~~~~~~~~ userString2Float() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Purpose : convert recvWithEndMarker's output array of char receivedChars[ndx] to float & continues data flow 
 * Input : [global] char receivedChars[]
 * Output : float 
 */
float userString2Float(){
 if (newData_str == true) {
    String user_str;
    user_str = String(receivedChars);
    newData_str = false;
    return user_str.toFloat();
 }
}



/* ~~~~~~~~~~~~~~~~~~~~~~~~ validDur_Start() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : Confirm volumetric flow rate x duration wont exceed the syringe capacity 
 *  Input : [global]   q_user, syr_capacity, duration
 *  Output : [global] bool TICKET
 */
void validDur_Start(float Q,float Vol, unsigned int Time){
  if ( (Q * Time) <= Vol ){ TICKET = true;}
  else {TICKET = false;}
  }

  
