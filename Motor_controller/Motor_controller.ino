// Connor Wilson
// Winter 2022
// SMSS Motor Controller V1.0


#include <TimerOne.h>           // Custom PWM freq
#include <Wire.h>               // I2C 


// Test Variables
#define TESTING               // Comment to cancel testing
//#define SERIAL_COMMS          // Comment to cancel serial comms [manual testing]
#define I2C_COMMS               // Comment to cancel I2C comms
#define ard_nano_uno            // Using nano/uno board 
//#define ard_micro               // Using micro board


// State machine 
enum State_enum {STANDBY, DATAPULL_S, DATAPULL_J, RUN_S, RUN_J, STOP};  // States of the state machine
unsigned int state = STANDBY;                                           // default state 
#define START 'S'
#define JOG 'J'
#define BACK 'B'
#define ESTOP 'E'
bool TICKET = false;        // true if user supplied data is valid with hardware
bool FINISH = false;        // true if user requested duration is matched by hardware 

#ifdef ard_nano_uno
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
#endif


#ifdef ard_micro
  // Pin Connections 
  #define STEP 5                
  #define DIR 4                 
  #define STBY 6                
  #define M1 7                  
  #define M2 8                  

  // LED State Indicators
  #define YELLOW 9
  #define GREEN 10
  #define WHITE 11 
  #define BLUE 12

#endif

// Motor States
#define FORWARD LOW           // motor direction for pin logic
#define REVERSE HIGH          // motor direction for pin logic
bool DIRECTION = FORWARD;     // motor direction
bool RUNNING  = false;        // true if motor is running


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
char step_config = S_FULL;                  // Set default to full step


// Pulse/Freq Variables
#define DC  0.6 * 1024                      // Duty Cycle of PWM                                          [UNSURE WHAT RATIO IS BEST]
#define MTR_FREQ_MAX 875                    // Max freq motor can handle [Hz]
#define MTR_RES_MIN 120                     // Min freq for smooth performance [Hz]
#define MTR_FREQ_MIN 0                      // Min freq motor can handle [Hz]                             [UNKNOWN]


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
unsigned long hour_user = 0;                // User "         hour "
unsigned long duration = 0;                 // Run time {msec}
unsigned long time_stamp_start = 0;         // time keeping 
unsigned long time_stamp_end = 0;           // "


// Serial Variables
#ifdef SERIAL_COMMS
  char receivedChar;                          // pool for char data
  boolean newData_char = false;               // true if new char data is RX 
  boolean newData_str = false;                // true if new str data is RX 
  const byte numChars = 32;                   // str length when TX 
  char receivedChars[numChars];               // pool for str data 
  bool displayed = false;                     // true if graphic is displayed 
#endif

// I2C Variables
#ifdef I2C_COMMS

  // I2C Addresses
  #define PI_ADD 0x01                         // Pi address
  #define ARD_ADD_1 0x14                      // Arduino #1
  #define ARD_ADD_2 0x28                      // Arduino #2
  #define ARD_ADD_3 0x42                      // Arduino #3

  // I2C Variables
  char poll_user = 0;                       // run state of the ard [OFF(0), JOG(1), START(2) ]
  char recData = 0;                         // storage for dictation codes
  char strRX[200];                          // RX storage
  char dur_mark = 0;                        // marker to aid recv/req track incoming hour, min, sec 
  bool datapull_flag = false;               // completed datapull from I2C
  char motor_dir = 0;                       // [ (0) - REV, (1) - FWD, (2) - *BACK to STANDBY] *some cases(change l8r)

  // Pi's Dictating (main) codes 
  #define POLLDATA 1                        // Ard will recv[OFF, JOG, START] 
  #define Q_DATA 2                          // Ard will recv[q_user]
  #define R_DATA 3                          // Ard will recv[syr_radius_user]
  #define CAP_DATA 4                        // Ard will recv[syr_capacity]
  #define DUR_DATA 5                        // Ard will recv[hour_user, min_user, sec_user]
  #define DIR_DATA 6                        // Ard will recv[DIRECTION]
  #define EVENT_DATA 7                      // Ard will proceed to RUN
  #define ESTOP_DATA 8                      // Ard will proceed to STOP

  // Arduino's Requested (Sub) codes                       
  #define LARGE_ERR 0x3                     // User req too large (Q) for hardware constraints
  #define SMALL_ERR 0x4                     // User req too small (Q) for hardware constraints
  #define DUR_ERR 0x5                       // User req too long (time) for hardware constraints
  #define LD_ERR 0x6                        // User too large and long
  #define SD_ERR 0x7                        // user too small and long
  #define DICT_ERR 0x8                      // wrong dictating codes
  #define NO_ERR 0x10                       // No error to report          
  char error_code = NO_ERR;

  #define WAIT_CODE 0x15                    // Ard needs time to pull data and validate 
  #define FINISH_CODE 0x16                  // Ard's duration has been reached and finished RUN_S
  #define TIME_CODE 0x17                    // Ard is not done with RUN_S and will return time(h,m,s) if prompted
  bool time_flag = false;                   // bool to help with req time code [ true - sent TIME_CODE already]


  // Arduino checkpoints
  bool dp_checkpoint = false;               // true - ard has finished processing TICKET
  bool eventstart = false;                  // true - Pi has req to RUN the DATAPULL'd ard 
  bool estop_user = false;                  // true - Pi req arduino to stop
  

    
  
#endif


void setup() {

  #ifdef SERIAL_COMMS
  Serial.begin(9600);                           // Serial comms init
  #endif

  #ifdef I2C_COMMS
  
    #ifdef TESTING
    Serial.begin(9600);
    #endif
  
  Wire.begin(ARD_ADD_1);                        // I2C bus logon with sub address
  Wire.onReceive(recvEvent);                    // run recvEvent on a main write to read <-
  Wire.onRequest(reqEvent);                     // run reqEvent on a main read to write ->
  #endif

  pinMode(STEP, OUTPUT);                        // Setup controller pins
  pinMode(DIR, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);

  pinMode(BLUE, OUTPUT);                        // Indicator LED pin setup
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(WHITE, OUTPUT);
  digitalWrite(DIR, REVERSE);                   // Direction of motor
  
}


void loop() {
  
  #ifdef SERIAL_COMMS
    serial_state_machine();
  #endif

  #ifdef I2C_COMMS
    i2c_state_machine();
  #endif
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
 *  Input : frequency, [global] step_config, [global] MTR_FREQ_MAX, [global] MTR_RES_MIN
 *  Output : frequency, [global] step_config, [global] TICKET
 *  Notes : Removes decimal point to save memory
 */
double MicroCali( float frequency ){


  // If greater than the max frequency
  if ( frequency > MTR_FREQ_MAX){
    
    step_config = S_FULL;                                                 // Set controller to full step
    TICKET = false;                                                         // Mark ticket invalid
    
        #ifdef SERIAL_COMMS
        Serial.println(" Requested Q value exceeds hardware's MAX RPS");  // Inform user
        #endif

        #ifdef I2C_COMMS
        error_code = LARGE_ERR;
        #endif
        
    return MTR_FREQ_MAX;                                                  // Set freq to max
  }

  // Or if less than the min freq
  else if (frequency < MTR_RES_MIN){
    
    double microstep = MTR_RES_MIN / frequency;                          // Microstep size 
    
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
      
        #ifdef SERIAL_COMMS
        Serial.println(" Requested Q value is below hardware's MIN RPS");  // Inform user
        #endif

        #ifdef I2C_COMMS
        error_code = SMALL_ERR;
        #endif
        
    return MTR_RES_MIN;                                                  // Set freq to min
      
    }
 }

 // No microstepping change needed
 else{
  TICKET = true;                                                          // Approved ticket
  return frequency;
 }
   
}

  
/* ~~~~~~~~~~~~~~~~~~~~~~~~ validDur_Start() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : Confirm volumetric flow rate x duration wont exceed the syringe capacity 
 *  Input : [global]   q_user, syr_capacity, duration
 *  Output : [global] bool TICKET
 */
void validDur_Start(float Q,float Vol, unsigned int Time){
  if ( (Q * Time) <= Vol ){ TICKET = true;}
  else {

    #ifdef I2C_COMMS
    if(error_code == NO_ERR){error_code = DUR_ERR;}
    else if(error_code == LARGE_ERR){error_code = LD_ERR;}
    else if(error_code == SMALL_ERR){error_code = SD_ERR;}
    #endif
    
    TICKET = false;}
  }





#ifdef SERIAL_COMMS
  /* ~~~~~~~~~~~~~~~~~~~~~~~~ serial_state_machine() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   *  Purpose : Control the flow of the motor controller 
   *  Input : Serial user input signals
   *  Output : 
   */
   void serial_state_machine(){
    
    switch(state){
      
      case STANDBY:
  
      digitalWrite(YELLOW, HIGH);         // LED indicator
  
          if(displayed == false){                                           // dont print unless its the first
          Serial.println(" ~~~~~~~~~~~~Standby Mode~~~~~~~~~~~ ");
          Serial.println(" Press S for START mode or J for JOG (No line ending) \n\n\n\n");
          displayed = true;
          }
          
          recvOneChar();
             
  
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
  
          Serial.println(" ~~~~~~~~~~DATAPULL_S~~~~~~~~~ ");
          Serial.println(" What is the fluid capacity of the syringe[mL]? (newline)");
          while(newData_str == false){recvWithEndMarker();}                                                     
          syr_capacity = userString2Float();
          
          Serial.println(" What is the radius of the syringe plunger[um]? (newline)");
          while(newData_str == false){recvWithEndMarker();}
          syr_radius_user = userString2Float() / 1000000;
          
          Serial.println(" What is the requested volumetric flow rate Q [nL/s] ? Range [69n , 10u] (newline)");
          while(newData_str == false){recvWithEndMarker();}
          q_user = userString2Float() / 1000000000;
          
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
            state = RUN_S;}                                                       // Valid data, move forward
          else{state = DATAPULL_S;}                                               // Repeat if the data wasnt RX/right
          
          break;
  
  
      case DATAPULL_J:
          digitalWrite(BLUE, HIGH);                                               // LED indicator
          digitalWrite(WHITE, HIGH);                                              // LED indicator
          
          Serial.println(" \n\n\n\n~~~~~~~~~~DATAPULL_JOG~~~~~~~~~ ");
          Serial.println(" What is the radius of the syringe plunger [um]? (newline)");
          while(newData_str == false){recvWithEndMarker();}                                                     
          syr_radius_user = userString2Float() / 1000000;
          
          Serial.println(" What is the requested volumetric flow rate Q [nL/s] ? Range [69n , 10u] (newline)");
          while(newData_str == false){recvWithEndMarker();} 
          q_user = userString2Float() / 1000000000;
          
          Serial.println(" Direction? F for forward, R for reverse, or B to back to Standby (No line ending)");
          newData_char = false;
          while(newData_char == false){recvOneChar();}
          if( receivedChar == 'R'){DIRECTION = REVERSE; }
          else if( receivedChar == 'F'){DIRECTION = FORWARD;} 
                
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
          if(displayed == false){
            Serial.println(" ~~~~~~~~~~~~ Run_JOG ~~~~~~~~~~~ ");
            Serial.println(" Press E for ESTOP (No line ending)");
            displayed = true;
          }
          
          recvOneChar();
          
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
    
          if(displayed == false){
            Serial.println(" ~~~~~~~~~~~~ Run_START ~~~~~~~~~~~ ");
            Serial.println(" Press E for ESTOP (No line ending)\n\n\n\n");
            displayed = true;
          }
          
          recvOneChar();
          
          if(receivedChar == ESTOP || FINISH == true){
              displayed = false;
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
          hour_user = 0;
          min_user = 0;
          sec_user = 0;
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
    
  
#endif






#ifdef I2C_COMMS
 /* ~~~~~~~~~~~~~~~~~~~~~~~~ i2c_state_machine() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : Control the flow of the motor controller 
 *  Input : I2C user input signals
 *  Output : 
 */
 void i2c_state_machine(){
  
  switch(state){

    
    
    case STANDBY:
        
        digitalWrite(YELLOW, HIGH);                                // LED indicator
      
        if (poll_user == 2){                                       // User selects START for normal use
          digitalWrite(YELLOW, LOW);                             // LED indicator
          state = DATAPULL_S;
          }  
                    
        else if(poll_user == 1){                                   // User selects JOG to jog motor
          digitalWrite(YELLOW, LOW);                             // LED indicator
          state = DATAPULL_J;
          }
                  
        else{state = STANDBY;}                                     // Or wait until user chooses
        
        break;

    case DATAPULL_S:

        digitalWrite(BLUE, HIGH);                                                 // LED indicator

        // if all data has been pulled from Pi, process and find errors (if exist)
        if(dp_checkpoint == false && datapull_flag == true){                                                

          // Process variables for valid TICKET and check for errors
          duration = ( (3600 * hour_user) + (60 * min_user) + sec_user) * 1000;   // entire duration in msec for easy compare later
          freq =  Q_to_Freq(q_user, syr_radius_user);                             // Get period from Q
          freq = MicroCali(freq);                                                 // Calibrate for microstepping + error check (TICKET)
          if(duration == 0){TICKET = false;}
          else if(TICKET == true){validDur_Start(q_user, syr_capacity, duration);}     // final TICKET confirm duration + error
        
          // Go to STANDBY is user sel to go back
          if(motor_dir == 2){
            poll_user = 0;              // reset user seletion
            digitalWrite(BLUE, LOW);                                             
            state = STANDBY;}

          // Proceed with current data                                                              
          else{
            // ready to give feedback to Pi
            dp_checkpoint = true;
            state = DATAPULL_S;}
        }

        // if all pulled data is valid, Pi has valid TICKET, and is ready to run
        else if(TICKET == true && eventstart == true){                            // Must lie within Q and duration range
          digitalWrite(BLUE, LOW);                                                // LED indicator
          init_control(step_config);                                              // Calibrate controller
          datapull_flag = false;
          eventstart = false;
          TICKET = false;
          state = RUN_S;
        }                                                   

        // Not all data has been pulled/ Pi not ready to RUN / Errors exist
        else{state = DATAPULL_S;}
        
        break;


    case DATAPULL_J:
        digitalWrite(BLUE, HIGH);                                               // LED indicator
        digitalWrite(WHITE, HIGH);                                              // LED indicator

        // if all data has been pulled from Pi, process and find errors (if exist)
        if(dp_checkpoint == false && datapull_flag == true){
            freq =  Q_to_Freq(q_user, syr_radius_user);                             // Get period from Q
            freq = MicroCali(freq);                                                 // Calibrate microstepping & TICKET validation
            
          // Go to STANDBY is user sel to go back
          if(motor_dir == 2){
            digitalWrite(BLUE, LOW);                                             
            state = STANDBY;}

          // Proceed with current data                                                              
          else{
            // ready to give feedback to Pi
            dp_checkpoint = true;
            state = DATAPULL_J;}
        
        }

        
        // if all pulled data is valid, Pi has valid TICKET, and is ready to run
        else if(TICKET == true && eventstart == true){
          digitalWrite(BLUE, LOW);                                               // LED indicator
          digitalWrite(WHITE, LOW);                                              // LED indicator
          init_control(step_config);                                              // Calibrate controller
          datapull_flag = false;
          eventstart = false;
          TICKET = false;
          state = RUN_J;
          }                                                       

        // Not all data has been pulled/ Pi not ready to RUN / Errors exist      
        else{state = DATAPULL_J;}                                               
        
        break;


    case RUN_J:

        digitalWrite(GREEN, HIGH);                                               // LED indicator
        digitalWrite(WHITE, HIGH);                                              // LED indicator

        
        if(estop_user == true){
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
        
        else{state = RUN_J;}
          
        break;


    case RUN_S:
    
      digitalWrite(GREEN, HIGH);                                               // LED indicator

      
      if(estop_user == ESTOP || FINISH == true){
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

        q_user = 0;                                                   
        syr_radius_user = 0;
        freq = 0;
        duration = 0; 
        hour_user = 0;
        min_user = 0;
        sec_user = 0;
        motor_dir = 0;
        
        estop_user = false;
        dp_checkpoint = false;               
        eventstart = false; 
        RUNNING = false;
        TICKET = false;
        FINISH = false;
        datapull_flag = false; 
        time_flag = false;

        poll_user = 0;                       
        error_code = NO_ERR;
        recData = 0;                          
        strRX[200] = {0};                          
        dur_mark = 0;                                        

        state = STANDBY;
        break;

    default:
        state = STOP;
        break;

    
  }
 }



  /* ~~~~~~~~~~~~~~~~~~~~~~~~ recvEvent() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   *  Purpose : Read the incoming data from main's request
   *  Input : I2C Rx codes from Pi
   *  Output : [global] user variable write
   */
  void recvEvent(int numBytes_TX){

    // Read the dictating code first
    if (recData == 0 || recData == -1){             // 0 for init and -1 for i2cdetect query
      
      recData = Wire.read();
  
      #ifdef TESTING
      Serial.print("recData : ");
      Serial.println(recData, HEX); 
      Serial.print("numBytes : ");
      Serial.println(numBytes_TX);
      #endif
    }


    // If dictating code is known, transfer data to the correct variable
    else{
      switch(recData){

          case POLLDATA:
            poll_user = Wire.read();             
            recData = 0;                      
            break;

          case Q_DATA:
            q_user = Wire.read();
            q_user /= 1000000000;
            recData = 0;
            break;
  
          case R_DATA:
            syr_radius_user = Wire.read();             
            syr_radius_user /= 1000;
            recData = 0;
            break;
  
          case CAP_DATA:
            syr_capacity = Wire.read();             
            recData = 0;
            break;
  
          case DUR_DATA:
            if(dur_mark == 0){
              hour_user = Wire.read();
              dur_mark++;
              }
            else if (dur_mark == 1){
              min_user = Wire.read();
              dur_mark++;
              }
            else if (dur_mark == 2){
              sec_user = Wire.read();             
              dur_mark = 0;
            }

            recData = 0;
            break;
  
          case DIR_DATA: 
            motor_dir = Wire.read();             
            datapull_flag = true;                             

            #ifdef TESTING
              Serial.println(poll_user);
              Serial.println(q_user,10);
              Serial.println(syr_radius_user,8);
              Serial.println(syr_capacity);
              Serial.println(hour_user);
              Serial.println(min_user);
              Serial.println(sec_user);
              Serial.println(motor_dir);
            #endif
            
            recData = 0;
            break;

          case EVENT_DATA:
            eventstart = true;             
            recData = 0;                      
            break;

          case ESTOP_DATA:
            estop_user = Wire.read();
            recData = 0;
            break;

          default:
            error_code = DICT_ERR;
            recData = 0;
            break;
      }
    }

  }
  
  
  
  /* ~~~~~~~~~~~~~~~~~~~~~~~~ reqEvent() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   *  Purpose : Write data to main at its request
   *  Input : I2C request
   *  Output : TX corresponding error code
   */
  void reqEvent(){

    
    // Tell Pi RUN_S has finished 
    if(FINISH == true){Wire.write(FINISH_CODE);}       


    // If in RUN_S but enough time hasn't elapsed, send time_code for next TX
    else if (state == RUN_S && time_flag == false){
      Wire.write(TIME_CODE);
      time_flag = true;
      }



    // Once Pi has recv time_code, send the time metrics
    else if (state == RUN_S && time_flag == true){
         
        if(dur_mark == 0){
          hour_user = time_stamp_end / 3600000;
          Wire.write(hour_user);
          dur_mark++;
          }
          
        else if(dur_mark == 1){
          min_user = (time_stamp_end - (3600*hour_user)) / 60;
          Wire.write(min_user);
          dur_mark++;
          }

        else if(dur_mark == 2){
          sec_user = time_stamp_end - (3600 * hour_user) - (60 * min_user);
          Wire.write(sec_user);
          dur_mark = 0;
          time_flag = false;
          }
    }



    // Tell Pi to wait if ard is still calculating/validating TICKET
    else if(dp_checkpoint == false && state != RUN_S){
      Wire.write(WAIT_CODE);                                 // Ard hasnt had time to validate TICKET yet
      } 



    // Return error (or no error) in DATAPULL
    else{ 
      if(error_code != NO_ERR){                              // If errors exist, prepare for another datapull
        dp_checkpoint = false;
        datapull_flag = false;  
      }
      Wire.write(error_code);}                              // TX error
      #ifdef TESTING
        Serial.print("Error code :");
        Serial.println(error_code, HEX);
      #endif
  }


#endif
