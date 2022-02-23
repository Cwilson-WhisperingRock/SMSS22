// Connor Wilson
// Winter 2022
// SMSS Motor Controller V1.0

#include <TimerOne.h>           // Custom PWM freq (ONLY WORKS WITH MEGA328 - NO MICRO BOARDS) - PWM only for pins(9, 10) on nano
#include <Wire.h>               // I2C 



// Test Variables (COMMENT OUT TO DESELECT) -- DONT FORGET TO CHANGE ARD ADDRESS (i2c_address) BELOW
#define TESTING               // Comment to cancel testing
//#define SERIAL_COMMS          // Comment to cancel serial comms [manual testing]
#define I2C_COMMS               // Comment to cancel I2C comms
//#define STSPIN220             // Comment out to NOT use the STSPIN220 Pololu low-volt motor controller
#define STSPIN820             // Comment out to NOT use the STSPIN820 Pololu high-volt motor controller


// State machine 
enum State_enum {STANDBY, DATAPULL_S, DATAPULL_J, RUN_S, RUN_J, STOP};  // States of the state machine
unsigned int state = STANDBY;                                           // default state 
#define START 'S'
#define JOG 'J'
#define BACK 'B'
#define ESTOP 'E'
bool TICKET = false;        // true if user supplied data is valid with hardware
bool FINISH = false;        // true if user requested duration is matched by hardware 



#ifdef STSPIN220 // Low voltage motor controller library
    // Pin Connections 
    #define M1 2                  // microstep config bit 1
    #define M2 3                  // microstep config bit 2
    #define STBY 5                // Controller's standby enable (both EN and STBY must be low for low power mode)
    #define STEP 9                // PWM signal for motor
    #define DIR 7                 // Direction of motor
    // NOTE : STEP, DIR, M2, M1 are all used to set the microstep in the init latching 
    //        but only STEP and DIR are availible after init
  
  
    // LED State Indicators
    #define YELLOW 8
    #define BLUE 10
    #define WHITE 11 
    #define GREEN 12



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
#endif


#ifdef STSPIN820 // High voltage motor controller library
    // Pin Connections 
    #define M1 2                  // microstep config bit 1
    #define M2 3                  // microstep config bit 2
    #define M3 4                  // microstep config bit 3
    #define STBY 5                // Controller's standby enable (both EN and STBY must be low for low power mode)
    #define STEP 9                // PWM signal for motor
    #define DIR 7                 // Direction of motor
    // NOTE : M1-3 are used for microstep latching (ie STEP/DIR are not used for latching like the STSPIN220)
  
  
    // LED State Indicators
    #define YELLOW 8
    #define BLUE 10
    #define WHITE 11 
    #define GREEN 12



        // Microstep settings for md39a[ M1, M2, STEP, DIR ]
    #define S_FULL  0x0
    #define S_HALF  0x4
    #define S_QRTR  0x2
    #define S_8     0x6
    #define S_16    0x1
    #define S_32    0x5
    //#define S_64  no 64 bit
    #define S_128   0x3
    #define S_256   0x7
    char step_config = S_FULL;                  // Set default to full step
#endif


// Motor States
#define FORWARD LOW           // motor direction for pin logic
#define REVERSE HIGH          // motor direction for pin logic
bool DIRECTION = FORWARD;     // motor direction
bool RUNNING  = false;        // true if motor is running





// Pulse/Freq Variables
#define DC  0.8 * 1024                      // Duty Cycle of PWM                                          [UNSURE WHAT RATIO IS BEST]
#define MTR_FREQ_MAX 875                    // Max freq motor can handle [Hz]
#define MTR_RES_MIN 120                     // Min freq for smooth performance [Hz]
#define MTR_FREQ_MIN 0                      // Min freq motor can handle [Hz]                             [UNKNOWN]


// Physical Variables
#define INTERNAL_STEP 200                   // Internal step size of the motor
#define GEAR_RATIO  99.05                   // gear ratio of the planetary gear
#define THREAD_PITCH  0.005                 // thread pitch of the actuator's screw [m]
float syr_capacity = 30;                    // capacity of syringe in mL
float syr_radius_user = 11;                 // RADIUS of the 30ml BD syringe [m]
float q_user = 0;                           // volumetric flow rate as requested by user [L/s]
float freq = 200;                           // [sec/step]
unsigned long sec_user = 0;                 // User requested sec duration
unsigned long min_user = 0;                 // User requested min duration
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
    int i2c_address = ARD_ADD_3;                // EDIT THIS TO CHANGE ADDRESS
  
    // I2C Variables
    int poll_user = 0;                       // run state of the ard [OFF(0), JOG(1), START(2) ]
    int recData = 0;                         // storage for dictation codes
    char strRX[2000];                          // RX storage
    char dur_mark = 0;                        // marker to aid recv/req track incoming hour, min, sec 
    bool datapull_flag = false;               // completed datapull from I2C
    int motor_dir = FORWARD;                  // [ (1) - REV, (0) - FWD, (2) - *BACK to STANDBY] *some cases(change l8r)
  
    // Pi's Dictating (main) codes 
    #define POLLDATA 1                        // Ard will recv[OFF, JOG, START] 
    #define Q_DATA 2                          // Ard will recv[q_user]
    #define R_DATA 3                          // Ard will recv[syr_radius_user]
    #define CAP_DATA 4                        // Ard will recv[syr_capacity]
    #define DUR_DATA 5                        // Ard will recv[hour_user, min_user, sec_user]
    #define DIR_DATA 6                        // Ard will recv[DIRECTION]
    #define EVENT_DATA 7                      // Ard will proceed to RUN
    #define ESTOP_DATA 8                      // Ard will proceed to STOP
    #define REDO_DATA 9                       // Ard will reset to STANDBY from DATAPULL
    #define BLOCK_DATA 10
  
    // Arduino's Requested (Sub) codes                       
    #define LARGE_ERR 0x3                     // User req too large (Q) for hardware constraints
    #define SMALL_ERR 0x4                     // User req too small (Q) for hardware constraints
    #define DUR_ERR 0x5                       // User req too long (time) for hardware constraints
    #define LD_ERR 0x6                        // User too large and long
    #define SD_ERR 0x7                        // user too small and long
    #define DICT_ERR 0x8                      // wrong dictating codes
    #define NO_ERR 0x10                       // No error to report          
    int error_code = NO_ERR;
  
    #define WAIT_CODE 0x15                    // Ard needs time to pull data and validate 
    #define FINISH_CODE 0x16                  // Ard's duration has been reached and finished RUN_S
    #define TIME_CODE 0x17                    // Ard is not done with RUN_S and will return time(h,m,s) if prompted
    bool redo_flag = false;
    bool time_flag = false;                   // bool to help with req time code [ true - sent TIME_CODE already]
    char block_recv = 0;
    unsigned long block[6] = {0,0,0,0,0,0};   // [BLOCK_DATA, dict_code, fac1, rem1, fac2, rem2] for data RX (I2c only transmitts 256 bit)
  
  
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
    
    Wire.begin(i2c_address);                        // I2C bus logon with sub address
    Wire.onReceive(recvEvent);                    // run recvEvent on a main write to read <-
    Wire.onRequest(reqEvent);                     // run reqEvent on a main read to write ->
  #endif

  pinMode(STEP, OUTPUT);                        // Setup controller pins
  pinMode(DIR, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);

  #ifdef STSPIN820
    pinMode(M3, OUTPUT);
  #endif

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


#ifdef STSPIN220
/*~~~~~~~~~~~~~~~~~~~~~~~ MtrCtrl_init_220() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : initialize or change the low voltage md39a controllers step size
 *  Input : 0x4'b  [ M1, M2, STEP, DIR ] as calculated by the requested step setting
 *  Outut : digital write to pins
 *  Notes : ~EN pin is held low 
 */
void MtrCtrl_init_220(char pin_config) {


  #ifdef TESTING
      Serial.print("pin_config : ");
      Serial.println(pin_config, HEX);
  #endif
    
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

#endif



#ifdef STSPIN820

/*~~~~~~~~~~~~~~~~~~~~~~~ MtrCtrl_init_820() ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Purpose : initialize or change the high voltage md37a controllers step size
 *  Input : 0x4'b  [ M1, M2, M3 ] as calculated by the requested step setting
 *  Outut : digital write to pins
 *  Notes : ~EN pin is held low 
 */
void MtrCtrl_init_820(char pin_config) {



  #ifdef TESTING
      Serial.print("pin_config : ");
      Serial.println(pin_config, HEX);
  #endif
    
  //The recommended power-up sequence is following:
  
  //1. Power-up the device applying the VS supply voltage but keeping both STBY and EN/FAULT inputs low.
  digitalWrite(STBY, LOW);
  
  //2. Set the MODEx inputs according to the target step resolution (see Table 1).
  digitalWrite(M3, pin_config & 1);
  digitalWrite(M2, (pin_config >> 1) & 1);
  digitalWrite(M1, (pin_config >> 2) & 1);

  #ifdef TESTING
      Serial.println(pin_config & 1);
      Serial.println((pin_config >> 1) & 1);
      Serial.println((pin_config >> 2) & 1);
  #endif
  
  //3. Wait for at least 1 µs (minimum tMODEsu time).
  delayMicroseconds(1);
  
  //4. Set the STBY high. The MODEx configuration is now latched inside the device.
  digitalWrite(STBY, HIGH);
  
  //5. Wait for at least 100 µs (minimum tMODEho time).
  delayMicroseconds(100);
  
  //6. Enable the power stage releasing the EN/FAULT input and start the operation.
}

#endif

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
      
      // the STSPIN820 doesnt have 64 bit -> make it 128 bit instead
      #ifdef STSPIN220
          step_config = S_64;
          TICKET = true;                                                      // Approved ticket                                               
          return 64 * frequency;
     #else
          step_config = S_128;
          TICKET = true;                                                      // Approved ticket                                               
          return 128 * frequency;
     #endif
                                                     
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
          error_code = SMALL_ERR;                                         // report error to pi
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
 *  Input : [global]   q_user[L/s], syr_capacity[m], duration[s]
 *  Output : [global] bool TICKET
 */
void validDur_Start(float Q,float Vol, float Time){
  if ( (Q * Time) <= Vol ){ 

    #ifdef TESTING
      Serial.println(" Valid dur");
    #endif
    
    TICKET = true;}
  else {

    #ifdef I2C_COMMS
    if(error_code == NO_ERR){error_code = DUR_ERR;}
    else if(error_code == LARGE_ERR){error_code = LD_ERR;}
    else if(error_code == SMALL_ERR){error_code = SD_ERR;}
    #endif

    #ifdef TESTING
      Serial.println("Bad Dur");
      Serial.println(Q, 10);
      Serial.println(Vol, 10);
      Serial.println(Time, 15);
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
          syr_capacity = userString2Float() / 1000;
          
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
  
          duration = ( (3600 * hour_user) + (60 * min_user) + sec_user);   // entire duration in msec for easy compare later
          
          freq =  Q_to_Freq(q_user, syr_radius_user);                             // Get period from Q
          freq = MicroCali(freq);                                                 // Calibrate for microstepping + TICKET
        
          if(duration == 0){TICKET = false;}
          else if(TICKET == true){validDur_Start(q_user, syr_capacity , duration);}     // final TICKET confirm duration 
          
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

              #ifdef STSPIN220
                  MtrCtrl_init_220(step_config);                              // setup motor controller
              #else
                  MtrCtrl_init_820(step_config);                              // setup motor controller
              #endif
              
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
            
              #ifdef STSPIN220
                  MtrCtrl_init_220(step_config);                              // setup motor controller
              #else
                  MtrCtrl_init_820(step_config);                              // setup motor controller
              #endif
              
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
            time_stamp_end = (time_stamp_end - time_stamp_start);       // total elapsed time
            if( time_stamp_end/1000 >= duration){ FINISH = true;}          // if time had reached user's request
            state = RUN_S;
          }
          
          break;
  
      case STOP:
          digitalWrite(STBY, LOW);                                             // Place motor controller in standby
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

        if(estop_user == true){state = STOP;}
      
        else if (poll_user == 2){                                       // User selects START for normal use
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

        if(redo_flag == true || estop_user == true){state = STOP;}

        // if all data has been pulled from Pi, process and find errors (if exist)
        else if(dp_checkpoint == false && datapull_flag == true){                                                

          
          // Process variables for valid TICKET and check for errors
          duration =  (3600 * hour_user) + (60 * min_user) + sec_user;   // entire duration in msec for easy compare later

          #ifdef TESTING
              Serial.print("hour_user : ");
              Serial.println(3600 * hour_user);
              Serial.print("min_user : ");
              Serial.println(60 * min_user);
              Serial.print("sec_user : ");
              Serial.println(sec_user);
              Serial.print("Duration : ");
              Serial.println(duration, 10);
          #endif
          
          freq =  Q_to_Freq(q_user, syr_radius_user);                             // Get period from Q

          #ifdef TESTING
              Serial.print("Freq : ");
              Serial.println(freq);
          #endif
          
          freq = MicroCali(freq);                                                 // Calibrate for microstepping + error check (TICKET)

          #ifdef TESTING
              Serial.print("Freq: ");
              Serial.println(freq);
          #endif
          
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

        if(redo_flag == true || estop_user == true){state = STOP;}
        
        // if all data has been pulled from Pi, process and find errors (if exist)
        else if(dp_checkpoint == false && datapull_flag == true){
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
          datapull_flag = false;
          eventstart = false;
          TICKET = false;
          state = RUN_J;
          }                                                       

        // Not all data has been pulled/ Pi not ready to RUN / Errors exist      
        else{state = DATAPULL_J;}                                               
        
        break;


    case RUN_J:

        // Device order delay                    
        if (i2c_address == ARD_ADD_1){delay(1800);}
        else if(i2c_address == ARD_ADD_2){delay(900);}
      
        digitalWrite(GREEN, HIGH);                                               // LED indicator
        digitalWrite(WHITE, HIGH);                                              // LED indicator

        
        if(estop_user == true){
            digitalWrite(GREEN, LOW);                                               // LED indicator
            digitalWrite(WHITE, LOW);                                              // LED indicator
            state = STOP;
        }
        
        else if (RUNNING == false){
          
              #ifdef STSPIN220
                  MtrCtrl_init_220(step_config);                              // setup motor controller
              #else
                  MtrCtrl_init_820(step_config);                              // setup motor controller
              #endif

            if (motor_dir == 1){digitalWrite(DIR,LOW);}          
              
            else{digitalWrite(DIR,HIGH);}


                #ifdef TESTING
                  Serial.print("freq");
                  Serial.println(freq);
                #endif
                
            Timer1.initialize((1/freq) * 1000000);                  // Init PWM freq[microsec] (mult to conv to usec)
            Timer1.pwm(STEP, DC);                                   // Start motor  
            RUNNING = true;
            state = RUN_J;
        }
        
        else{state = RUN_J;}
          
        break;


    case RUN_S:

      // Device order delay
      if (i2c_address == ARD_ADD_1){delay(1800);}
      else if(i2c_address == ARD_ADD_2){delay(900);}
    
      digitalWrite(GREEN, HIGH);                                               // LED indicator
      
      if(estop_user == true){
          digitalWrite(GREEN, LOW);                                               // LED indicator
          state = STOP;
          Timer1.disablePwm(STEP);
      }

      // keep ard here until finish is sent
      else if(FINISH == true){
        digitalWrite(GREEN, LOW);                                               // LED indicator
        Timer1.disablePwm(STEP);
        state = RUN_S;
      }
  
      // init run of motor
      else if (RUNNING == false){
        
              #ifdef STSPIN220
                  MtrCtrl_init_220(step_config);                              // setup motor controller
              #else
                  MtrCtrl_init_820(step_config);                              // setup motor controller
              #endif
          
          if (motor_dir == 1){digitalWrite(DIR,FORWARD); }         // rewrite DIR pin for direction
          else{digitalWrite(DIR,REVERSE);}
          
          Timer1.initialize((1/freq) * 1000000);                  // Init PWM freq[microsec] (mult to conv to usec)
          time_stamp_start = millis();                            // Start recording time
          Timer1.pwm(STEP, DC);                                   // Start motor  
          RUNNING = true;
          state = RUN_S;
      }
  
      // Running until final time
      else{
  
        time_stamp_end = millis();
        time_stamp_end = (time_stamp_end - time_stamp_start);       // total elapsed time
        if( time_stamp_end/1000 >= duration){ FINISH = true;}          // if time had reached user's request
        state = RUN_S;
      }
      
        
      break;

    case STOP:

        digitalWrite(STBY, LOW);                                             // Place motor controller in standby
        Timer1.disablePwm(STEP);                                             // turn off pwm

        digitalWrite(GREEN, LOW);                                               // LED indicator
        digitalWrite(WHITE, LOW);
        digitalWrite(YELLOW, LOW);                                               // LED indicator
        digitalWrite(BLUE, LOW);
        
        q_user = 0;                                                   
        syr_radius_user = 0;
        freq = 0;
        duration = 0; 
        hour_user = 0;
        min_user = 0;
        sec_user = 0;
        motor_dir = 0;
        step_config = 0;
        
        estop_user = false;
        dp_checkpoint = false;               
        eventstart = false; 
        RUNNING = false;
        TICKET = false;
        FINISH = false;
        datapull_flag = false; 
        time_flag = false;
        redo_flag = false;

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



void recvEvent(int numBytes){

    unsigned long temp;
  
    recData = Wire.read();

    // Ignore data unless recv BLOCK_CODE
    if(block_recv == 0){
        //Ignore data is not a block (its rollcall or i2cdetect instead)
        if(recData == BLOCK_DATA){
          //block[0] = recData;
          block_recv++;
        }
    }


    // Recv first data block
    else if (block_recv == 1){
        block[1] = recData;
        block_recv++;
  
         #ifdef TESTING
          Serial.print("block[1] : ");
          Serial.println(block[1]); 
        #endif
      }

    // Recv second data block
    else if (block_recv == 2){
      block[2] = recData;
      block_recv++;

       #ifdef TESTING
        Serial.print("block[2] : ");
        Serial.println(block[2]); 
      #endif
      
      }

    // Recv third data block
    else if (block_recv == 3){
      block[3] = recData;
      block_recv++;
      
       #ifdef TESTING
        Serial.print("block[3] : ");
        Serial.println(block[3]); 
      #endif
      }

    // Recv fourth data block
    else if (block_recv == 4){
      block[4] = recData;
      block_recv++;

       #ifdef TESTING
        Serial.print("block[4] : ");
        Serial.println(block[4]); 
      #endif
      }

    // Recv fifth data block
    else if (block_recv == 5){
      block[5] = recData;
      block_recv++;

       #ifdef TESTING
        Serial.print("block[5] : ");
        Serial.println(block[5]); 
      #endif
      
      }


    // If entire block has been recv, process data
    if (block_recv == 6){

        //if second factor is non-zero, data uses fac2, rem2
        if(block[4] != 0){
          temp = 256 * (256 * block[4] + block[5]) + block[3];
          }
  
        // Only one factor of 256
        else if (block[2] != 0){
          temp = 256 * block[2]  + block[3];
          }

        // Recv data is below 256
        else{temp = block[3];}

        #ifdef TESTING
          Serial.print("temp : ");
          Serial.println(temp); 
        #endif  

        //reset index
        block_recv = 0;


        // Read Dict Codes to store in right variable
        switch(block[1]){

          case POLLDATA:
              poll_user = temp;                                 
              break;

          case Q_DATA:
              q_user = temp;
              q_user /=1000000000; 
              break;
  
          case R_DATA:
              syr_radius_user = temp;
              syr_radius_user /= 1000;             
              break;
  
          case CAP_DATA:
              syr_capacity = temp;  
              syr_capacity /= 1000;           
              break;
  
          case DUR_DATA:
              if(dur_mark == 0){
                hour_user = temp;
                dur_mark++;
                }
              else if (dur_mark == 1){
                min_user = temp;
                dur_mark++;
                }
              else if (dur_mark == 2){
                sec_user = temp;           
                dur_mark = 0;
              }
  
              break;
  
          case DIR_DATA: 
              motor_dir = temp;            
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
              
              break;

          case EVENT_DATA:
              eventstart = true;                                  
              break;

          case REDO_DATA:
              redo_flag = temp; 
              Serial.println("STOPPED");
              state = STOP;            
              break;

          case ESTOP_DATA:
            estop_user = true;
            break;

          default:
            error_code = DICT_ERR;
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
    if(FINISH == true){
      Wire.write(FINISH_CODE);
      estop_user = true;       
    }


    // If in RUN_S but enough time hasn't elapsed, send time_code for next TX
    else if (state == RUN_S && time_flag == false){
      Wire.write(TIME_CODE);
      time_flag = true;
      }



    // Once Pi has recv time_code, send the time metrics
    else if (state == RUN_S && time_flag == true){

        // convert time_end to sec from ms
        unsigned long time_end = time_stamp_end / 1000;

        #ifdef TESTING
            Serial.print("time_end : ");
            Serial.println(time_end);
        #endif
         
        if(dur_mark == 0){
          hour_user = int(time_end / 3600);
          Wire.write(hour_user);
          dur_mark++;

          #ifdef TESTING
            Serial.print("hour_user : ");
            Serial.println(hour_user);
          #endif
          
          }
          
        else if(dur_mark == 1){
          min_user = int(( time_end - (3600*hour_user)) / 60);
          Wire.write(min_user);
          dur_mark++;

          #ifdef TESTING
            Serial.print("min_user : ");
            Serial.println(min_user);
          #endif
          
          }

        else if(dur_mark == 2){
          sec_user = int( time_end - (3600 * hour_user) - (60 * min_user));
          Wire.write(sec_user);
          dur_mark = 0;
          time_flag = false;


          #ifdef TESTING
            Serial.print("sec_user : ");
            Serial.println(sec_user);
          #endif
          
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
      Wire.write(error_code);
                                         // TX error
      #ifdef TESTING
        Serial.print("Error code : ");
        Serial.println(error_code, DEC);
      #endif

      error_code = NO_ERR;

    }
  }


#endif
