# Connor Wilson
# Winter 2022
# SMSS Hub Controller V1.0


from time import sleep                   # python delay
from smbus2 import SMBus
from transitions import Machine          # state machine
import curses                            # GUI
from curses import wrapper
from curses.textpad import Textbox
import random



class HubMachine(object):

    states = ['START', 'DATAPULL', 'RUN', 'RESET']   

    def __init__(self,name):
        self.name = name

        # States and their corresponding triggers/transitions
        self.machine = Machine(model= self, states = HubMachine.states, initial = 'START')
        self.machine.add_transition(trigger = 'CONFIRM', source = 'START', dest = 'DATAPULL')
        self.machine.add_transition(trigger = 'EVENTSTART', source = 'DATAPULL', dest = 'RUN')
        self.machine.add_transition(trigger = 'DONE', source = 'RUN', dest = 'RESET')
        self.machine.add_transition(trigger = 'RESTART', source = 'RESET', dest = 'START')


# States for GUI
global FRAME, POLL, DATA, RUN, ERROR, USER_CONFIRM, USER_ESTOP, reqGUI
FRAME = 0x0
POLL = 0x1
DATA = 0x2
ERROR = 0x3
RUN = 0x4
USER_CONFIRM = 0x6
USER_ESTOP = 0x7
reqGUI = 0x0          # Working var for GUI states

# Index for device spacing in the GUI
global column
column = 0

# Data between user-curses-arduino
global rollcall, poll_user, q_user, radius_user, cap_user
global hour_user, min_user, sec_user, dir_user, back_flag
global estop_flag, ard_duration
rollcall = [0, 0, 0]
poll_user = [0, 0, 0]
q_user = [0,0,0]
radius_user = [0,0,0]
cap_user = [0,0,0]
hour_user = [0,0,0]
min_user = [0,0,0]
sec_user = [0,0,0]
dir_user = [0,0,0]                                              # 1 - REV, 0 - FWD 
back_flag = [False, False, False]                               # User sel to go back
estop_flag = [False, False, False]                              # User sel for device estop
ard_duration = [[0,0,0], [0,0,0], [0,0,0]]                      # run time from ard

# Arduino's Requested (Sub) codes 
global LARGE_ERR, SMALL_ERR, DUR_ERR, LD_ERR, SD_ERR, DICT_ERR, NO_ERR
global WAIT_CODE, FINISH_CODE, TIME_CODE
global user_code

LARGE_ERR = 0x3                     # User req too large (Q) for hardware constraints
SMALL_ERR = 0x4                     # User req too small (Q) for hardware constraints
DUR_ERR = 0x5                       # User req too long (time) for hardware constraints
LD_ERR = 0x6                        # User too large and long
SD_ERR = 0x7                        # User too small and long
DICT_ERR = 0x8                      # Wrong dictating codes
NO_ERR = 0x10                       # No error to report
WAIT_CODE = 0x15                    # Ard needs time to pull data and validate 
FINISH_CODE=  0x16                  # Ard's duration has been reached and finished RUN_S
TIME_CODE =  0x17                   # Ard is not done with RUN_S and will return time(h,m,s) if prompted


user_code = 0                       # working variable

global estop_index
estop_index = 0
    
def main():

    # Working Variables that pass through main and GUI
    global rollcall, poll_user, q_user, radius_user, cap_user
    global hour_user, min_user, sec_user, dir_user, back_flag
    global estop_flag, ard_duration, reqGUI, user_code
    
    # I2C Addresses
    PI_ADD = 0x01                       
    ARD_ADD_1 = 0x14
    ARD_ADD_2 = 0x28
    ARD_ADD_3 = 0x42
    address = [ARD_ADD_1, ARD_ADD_2, ARD_ADD_3]

    # Pi's Dictating (main) codes 
    POLLDATA =  1                       # Ard will recv[OFF, JOG, START] 
    Q_DATA  = 2                         # Ard will recv[q_user]
    R_DATA  = 3                         # Ard will recv[syr_radius_user]
    CAP_DATA = 4                        # Ard will recv[syr_capacity]
    DUR_DATA = 5                        # Ard will recv[hour_user, min_user, sec_user]
    DIR_DATA = 6                        # Ard will recv[DIRECTION]
    EVENT_DATA = 7                      # Ard will proceed RUN
    ESTOP_DATA = 8                      # Ard will proceed to STOP
    REDO_DATA = 9                       # Ard will revert back to STANDBY from DATAPULL
    BLOCK_DATA = 10                     # Ard will recv a block (4) of data
    

    # Arduino's Requested (Sub) codes
    user_code = 0                       # working var for reading ERR/CODE from Ard

    #availability of arduino deivces 0 = false, 1 = true
    rollcall = [0,0,0]

    # GUI Window constants
    global column
    COL_CONST = 7
    COL_OFFSET = 5


    #curses window
    stdscr = curses.initscr()
    
    #state machine init
    Hub = HubMachine("SMSS")

    # Loop states
    while True:

        # Inital device check and device run-state selection
        if Hub.state == 'START' :

            #Identify devices on the buffer
            rollcall = deviceRollcall(address)

            #Display GUI background frame
            #global reqGUI
            reqGUI = FRAME
            curs(stdscr)
            
            #reset devices
            for i in range(3):
                if rollcall[i] > 0:
                    transmit_block(address[i], ESTOP_DATA, True )


            # Gather device run-state in GUI and TX to Ard
            for i in range(3):

                # GUI set to poll user for run-state
                reqGUI = POLL

                #if device is active
                if rollcall[i] > 0:

                    #set the correct column spacing
                    #global column
                    column = COL_CONST * i + COL_OFFSET
                    
                    # Go to GUI
                    curs(stdscr)
                    transmit_block(address[i], POLLDATA, poll_user[i] )
                    
                else:
                    #print("Dont got'em")
                    pass
                    
            
            # loop if all devices are off
            if (poll_user[0] == 0 and poll_user[1] == 0 and poll_user[2] == 0) :
                pass

            # Change GUI windows to DATAPULL 
            else:
                reqGUI = USER_CONFIRM
                curs(stdscr)
                Hub.CONFIRM()

        # Pull data from user using the Hub GUI
        elif Hub.state == 'DATAPULL' :
            
            #for all three devices
            for i in range(3):

                # if device is active
                if poll_user[i] > 0:

                    loop = True                         # loop to weed out user error
                    data_preserve = False               # flag to redo user data
                    
                    # update window column on GUI
                    column = COL_CONST * i + COL_OFFSET

                    # loop until all errors are out
                    while loop == True:

                        # Present data collection GUI
                        reqGUI = DATA
                        curs(stdscr)
                        
                        # if user selected to go back to poll GUI instead
                        if back_flag[i] == True:
                            
                            #reset flag
                            back_flag[i] = False

                            # transmit redo flag
                            transmit_block(address[i], REDO_DATA, True )
                            
                            # collect data again
                            reqGUI = POLL
                            curs(stdscr)
                            transmit_block(address[i], POLLDATA, poll_user[i] )
                            reqGUI = DATA
                            curs(stdscr)

                            #loop to transmit again
                            data_preserve = False           # TX new data to ard again
                            loop = True                     # continue to monitor for user error

                            #if user turns off device in redo, set device to standby
                            if poll_user[i] == 0:
                                 data_preserve = True       # dont write unwanted data
                        

                        # Send data batch (False) or keep data (True)
                        if data_preserve == False:

                            # if device is in JOG
                            if poll_user[i] == 1:
                                # transmit variables
                                transmit_block(address[i], Q_DATA, int(q_user[i]))
                                transmit_block(address[i], R_DATA, int(radius_user[i]) )
                                transmit_block(address[i], DIR_DATA, int(dir_user[i]) )
                                
                                                         
                            # if device is in RUN
                            elif poll_user[i] == 2:
                                transmit_block(address[i], Q_DATA, int(q_user[i]) )
                                transmit_block(address[i], R_DATA, int(radius_user[i]) )
                                transmit_block(address[i], CAP_DATA, int(cap_user[i]) )
                                transmit_block(address[i], DUR_DATA, int(hour_user[i]) )
                                transmit_block(address[i], DUR_DATA, int(min_user[i]) )
                                transmit_block(address[i], DUR_DATA, int(sec_user[i]) )
                                transmit_block(address[i], DIR_DATA, int(dir_user[i]) )

                        #if user turns off device in redo, dont loop for error removal
                        if poll_user[i] == 0:
                            loop = False

                        else:        
                            # read bus for error codes
                            with SMBus(1) as bus: 
                                user_code = bus.read_byte(address[i], 0)
      
                            # pass if no errors are present (exit condition)
                            if user_code == NO_ERR:
                                data_preserve = False       # reset var
                                loop = False                # dont loop for errors (none exist ..hopefully)
                                    
                            # loop if told to wait (and check again for errors)
                            elif user_code == WAIT_CODE:
                                data_preserve = True        # dont trash data yet
                                loop = True                 # loop to check for error though

                            # proceed to error screen and loop this again
                            else:
                                reqGUI = ERROR              # go to GUI screen to show error
                                curs(stdscr)
                                data_preserve = False       # transmit a new batch of data
                                loop = True                 # loop to prompt user for better data
                        
                                
                        



            # if all present devices have tickets, Hub.EVENTSTART()
            reqGUI = USER_CONFIRM
            curs(stdscr)
            for i in range(3):
                if poll_user[i] != 0:
                    transmit_block(address[i], EVENT_DATA, True )
            Hub.EVENTSTART()

        # Run motors, present estops, and monitor devices
        elif Hub.state == 'RUN' :

            # var to save redundant time checking
            time_saver = [0,0,0]

            # Print intro RUN window if device is on
            reqGUI = RUN
            for i in range(3):
                if rollcall[i] > 0: 
                    column = COL_CONST * i + COL_OFFSET
                    curs(stdscr)

            # Loop if at least one device is on
            while (poll_user[0] != 0 or  poll_user[1] != 0  or  poll_user[2] != 0) :

                #Poll for an estop
                reqGUI = USER_ESTOP
                curs(stdscr)

                for i in range(3):

                    # If user has marked one or all estops during a pull, send tx and stop
                    if estop_flag[0] == True and poll_user[0] > 0:
                        transmit_block(address[0], ESTOP_DATA, True )
                        poll_user[0] = 0

                    if estop_flag[1] == True and poll_user[1] > 0:
                        transmit_block(address[1], ESTOP_DATA, True )
                        poll_user[1] = 0

                    if estop_flag[2] == True and poll_user[2] > 0:
                        transmit_block(address[2], ESTOP_DATA, True )
                        poll_user[2] = 0
                        
                    # if it is time to sample the RUN_S state for FINISH or TIME
                    if time_saver[i] > 300 and poll_user[i] == 2:
                        
                        # Read ard status (FINISH or TIME_CODE)
                        with SMBus(1) as bus: 
                            user_code = bus.read_byte(address[i], 0)

                            #if FINISH, dont repoll device
                            if user_code == FINISH_CODE:
                                poll_user[i] = 0
                                #FINISH SCREEN

                            # if time_code, read data and store
                            elif user_code == TIME_CODE:
                                
                                global ard_duration
                                
                                with SMBus(1) as bus:
                                    for j in range(3):
                                        ard_duration[i][j]  = bus.read_byte(address[i], 0)

                                # Print time data
                                reqGUI = RUN
                                column = COL_CONST * i + COL_OFFSET
                                curs(stdscr)

                                #reset var
                                time_saver[i] = 0
                                
                    elif time_saver[i] > 300 and poll_user[i] == 1:
                        # Print time data
                        reqGUI = RUN
                        column = COL_CONST * i + COL_OFFSET
                        curs(stdscr)
                        time_saver[i] = 0
                                
                                
                    time_saver[i] += 1

            #Proceed if all devices are finished
            Hub.DONE()

                        
           
        #reset variables
        elif Hub.state == 'RESET' :
            rollcall = [0, 0, 0]
            poll_user = [0, 0, 0]
            q_user = [0,0,0]
            radius_user = [0,0,0]
            cap_user = [0,0,0]
            hour_user = [0,0,0]
            min_user = [0,0,0]
            sec_user = [0,0,0]
            dir_user = [0,0,0]                                               
            back_flag = [False, False, False]                               
            estop_flag = [False, False, False]                              
            ard_duration = [[0,0,0], [0,0,0], [0,0,0]]                      
            user_code = 0
            global estop_index
            estop_index = 0

            Hub.RESTART()


# Error_2Str()
# SIGNATURE : int -> String
# PURPOSE : Takes in Error code and returns a string 
#           that aids the user in correcting their data 
#           requests
def Error_2Str(error):

    global LARGE_ERR, SMALL_ERR, DUR_ERR, LD_ERR, SD_ERR, DICT_ERR, NO_ERR
    
    if error == LARGE_ERR:
        return "Error: Flow rate too large for hardware"
    elif error == SMALL_ERR:
        return "Error : Flow rate too small for hardware"
    elif error == DUR_ERR:
        return "Error : Run time will ruin machine"
    elif error == LD_ERR:
        return "Error : Too large flow rate + time overflow"
    elif error == SD_ERR:
        return "Error : Too small of Q + time overflow"
    elif error == DICT_ERR:
        return "Error : Ard got lost...GULP"
    else:
        return f"Your error code is unique... : {error}"

    

# transmit_block()
# SIGNATURE : int, int, int -> 
# PURPOSE : Transmits a dictating code +  data to an address
#           
def transmit_block(addr, dict_code, int_data):

    # Code for ard to prep for a block 
    BLOCK_DATA = 10 

    #       [block, dict_code, factor1, remainder1, factor2, remainder2]
    block = [BLOCK_DATA, dict_code, 0, int_data, 0, 0]
    
    #(can only handle 256 ^3)...increase for larger data set
    i = 2  

    while int_data > 255:
        block[i] = int(min(int_data / 256, 256))    # store factor of 256
        block[i+1] = int(int_data % 256)            # store remainder of 256
        int_data /= 256                             # break down data
        i += 2                                      
        
    #transmit data
    with SMBus(1) as bus:
        for i in range(6):
            bus.write_byte(addr, block[i])
            sleep(0.15)


# deviceRollcall()
# SIGNATURE : int_array -> int_array
# Purpose : Sends byte to each device using I2C and 
#           Stores 1 in array for a sucessful transmission (device active)
#           Stores 0 in array for unsuccessful transmission (device not active)
def deviceRollcall(myaddress):

    dev_status = [0, 0, 0]

    with SMBus(1) as bus: # write to BUS

        for i in range(3):

            # Try to contact each device
            try:
                bus.write_byte(myaddress[i], 0)
                dev_status[i] = 1
                #print(f"Device {i} on bus")
            except:
                #print(f"Device {i} not on bus")
                dev_status[i] = 0
                #pass
            
    sleep(0.1)    
    return dev_status


# GUI 
def curs(stdscr):

    global FRAME, POLL, DATA, RUN, ERROR, USER_CONFIRM, USER_ESTOP
    global reqGUI
    global rollcall, poll_user, q_user, radius_user, cap_user
    global hour_user, min_user, sec_user, dir_user
    global estop_index, user_code
    

    # Color combinations (ID, foreground, background)
    curses.init_pair(1, curses.COLOR_RED, curses.COLOR_WHITE)
    RED_AND_WHITE = curses.color_pair(1)

    curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_WHITE)
    BLACK_AND_WHITE = curses.color_pair(2)

    curses.init_pair(3, curses.COLOR_BLACK, curses.COLOR_GREEN)
    BLACK_AND_GREEN = curses.color_pair(3)

    curses.init_pair(4, curses.COLOR_YELLOW, curses.COLOR_CYAN)
    YELLOW_AND_CYAN = curses.color_pair(4)

    curses.init_pair(5, curses.COLOR_WHITE, curses.COLOR_MAGENTA)
    WHITE_AND_MAGENTA = curses.color_pair(5)

    curses.init_pair(6, curses.COLOR_BLACK, curses.COLOR_RED)
    BLACK_AND_RED = curses.color_pair(6)



    # Display background frame
    if reqGUI == FRAME:

        stdscr.clear()
        stdscr.attron(YELLOW_AND_CYAN)
        stdscr.resize(28,100)
        stdscr.move(0,0)

        # Fill square main in 
        for y in range(1,28):
            for x in range(1, 100-1):
                stdscr.addch(" ")
            stdscr.move(y,1)
        stdscr.border()

        #Title block
        stdscr.move(2,30)
        stdscr.addstr("        Smart Motor Syringe Pump        ", BLACK_AND_WHITE)


        
        stdscr.attroff(YELLOW_AND_CYAN)
        stdscr.attron(BLACK_AND_WHITE)

        #Device one block
        stdscr.move(5,10)
        stdscr.addstr("Device 1 :")
        stdscr.move(5,30)

        for y in range(5,11):
            for x in range(30, 90-1):
                stdscr.addch(" ")
            stdscr.move(y,30)

        # Device two block
        stdscr.move(12,10)
        stdscr.addstr("Device 2 :")
        stdscr.move(12,30)

        for y in range(12,18):
            for x in range(30, 90-1):
                stdscr.addch(" ")
            stdscr.move(y,30)

        # Device three block
        stdscr.move(19,10)
        stdscr.addstr("Device 3 :")
        stdscr.move(19,30)

        for y in range(19,25):
            for x in range(30, 90-1):
                stdscr.addch(" ")
            stdscr.move(y,30)


        #Re-center and refresh
        stdscr.move(21,40)
        stdscr.addstr(25, 35, "                                             ")
        stdscr.move(5,30)
        stdscr.refresh()

        
    # Poll user for device run modes
    elif reqGUI == POLL:

        global poll_user
        #avoiding a global index here people...forgive me coding gods
        index_gui = int((column-5)/7)


        if rollcall[index_gui] > 0:
            # display window
            winPoll1 = curses.newwin(5, 60, column, 30)
            curses.noecho()
            winPoll1.nodelay(True)
            winPoll1.clear()
            winPoll1.attron(BLACK_AND_GREEN)
            winPoll1.addstr(1, 5,f"Device {index_gui + 1} Mode :      | OFF | JOG | RUN |")
            winPoll1.refresh()
            
            # collect data
            q = 0
            key = ""

            while key != 'w':

                winPoll1.attroff(BLACK_AND_WHITE)     
                winPoll1.attron(BLACK_AND_GREEN)
                winPoll1.addstr(1, 5, f"Device {index_gui + 1} Mode :      | OFF | JOG | RUN |")

                try:
                    key = winPoll1.getkey()
                except:
                    key = None

                if key == 'a':
                    if q > 0:
                        q -= 1
                        
                elif key == 'd':
                    if q < 2:
                        q += 1


                if q == 0:
                    winPoll1.attroff(BLACK_AND_GREEN)
                    winPoll1.attron(BLACK_AND_WHITE)
                    winPoll1.addstr(1, 26, "| OFF |")
                    poll_user[index_gui] = 0                    # disable the device
                    
                elif q == 1:
                    winPoll1.attroff(BLACK_AND_GREEN)
                    winPoll1.attron(BLACK_AND_WHITE)
                    winPoll1.addstr(1, 32, "| JOG |")
                    poll_user[index_gui] = 1                    # jog the device

                elif q == 2:
                    winPoll1.attroff(BLACK_AND_GREEN)
                    winPoll1.attron(BLACK_AND_WHITE)
                    winPoll1.addstr(1, 38, "| RUN |")
                    poll_user[index_gui] = 2                    # run the device
                        
               
                winPoll1.refresh()
                sleep(0.5)


    # Simple enter button
    elif reqGUI == USER_CONFIRM:

        # display window
        winPoll4 = curses.newwin(1, 46, 25, 35)
        curses.noecho()                                 # dont repeat user input
        winPoll4.nodelay(True)                          # dont wait user input
        winPoll4.clear()
        winPoll4.attron(curses.color_pair(random.randint(1,6))) #randomly change color
        winPoll4.addstr(0, 18, "| CONFIRM? |")
        winPoll4.refresh()
            
        key = ""

        while key != 'w':

            try:
                key = winPoll4.getkey()
            except:
                key = None
                
        winPoll4.refresh()
        sleep(0.5)
        
    # Poll metrics for running in START
    elif reqGUI == DATA:

            #avoiding a global index here people...forgive me coding gods
            index_gui = int((column-5)/7)

            # User wants to disable the device (leave in standby - no update)
            if poll_user[index_gui] == 0:
                pass

            # User selected JOG function
            elif poll_user[index_gui] == 1:

                winData1 = curses.newwin(5, 60, column, 30)
                winData1.attron(WHITE_AND_MAGENTA)


                winData1.addstr(0, 0, "Volumetric Flow Rate Q [nL/s] in [70n, 10u] : ")
                winData1.refresh()
                win_data_ret = curses.newwin(1, 10, column, 76)
                curses.echo()
                box = Textbox(win_data_ret)
                box.edit()
                q_user[index_gui] = box.gather().strip().replace("\n", "")


                winData1.addstr(1, 0, "Syringe Radius in [mm] : ")
                win_data_ret.clear()
                win_data_ret.mvwin(column + 1,55)
                winData1.refresh()
                box.edit()
                radius_user[index_gui] = box.gather().strip().replace("\n", "")


                winData1.addstr(2, 0, "Direction ?        | FWD | REV | REDO | ? ")
                win_data_ret.clear()
                winData1.refresh()

                winData1.attroff(WHITE_AND_MAGENTA)     
                winData1.attron(BLACK_AND_GREEN)

                            # collect data
                curses.noecho()
                winData1.nodelay(True)
                q = 0
                key = ""
                while key != 'w':
                    try:
                        key = winData1.getkey()
                    except:
                        key = None

                    if key == 'a':
                        if q > 0:
                            q -= 1
                        
                    elif key == 'd':
                        if q < 2:
                            q += 1

                    winData1.attroff(BLACK_AND_GREEN)     
                    winData1.attron(WHITE_AND_MAGENTA)
                    winData1.addstr(2, 0, "Direction ?        | FWD | REV | REDO | ? ")
                    winData1.attroff(WHITE_AND_MAGENTA)     
                    winData1.attron(BLACK_AND_GREEN)

                    if q == 0:
                        winData1.addstr(2, 19, "| FWD |")
                        dir_user[index_gui] = 1
                        back_flag[index_gui] = False
                    
                    elif q == 1:
                        winData1.addstr(2, 25, "| REV |")
                        dir_user[index_gui] = 0
                        back_flag[index_gui] = False
                        
                    elif q == 2:
                        winData1.addstr(2, 31, "| REDO |")
                        back_flag[index_gui] = True

                    winData1.refresh()
                    sleep(0.5)

            # User sel RUN function
            elif poll_user[index_gui] == 2:

                winData1 = curses.newwin(5, 60, column, 30)
                winData1.attron(WHITE_AND_MAGENTA)


                winData1.addstr(0, 0, "Volumetric Flow Rate Q [nL/s] in [70n, 10u] : ")
                winData1.refresh()
                win_data_ret = curses.newwin(1, 10, column, 76)
                curses.echo()
                box = Textbox(win_data_ret)
                box.edit()
                q_user[index_gui] = box.gather().strip().replace("\n", "")


                winData1.addstr(1, 0, "Syringe Radius in [mm] : ")
                win_data_ret.clear()
                win_data_ret.mvwin(column + 1,55)
                winData1.refresh()
                box.edit()
                radius_user[index_gui] = box.gather().strip().replace("\n", "")

                winData1.addstr(2, 0, "Syringe Capacity in [mL] : ")
                win_data_ret.clear()
                win_data_ret.mvwin(column + 2,58)
                winData1.refresh()
                box.edit()
                cap_user[index_gui] = box.gather().strip().replace("\n", "")

                winData1.addstr(3, 0, "Hour # : ")
                win_data_ret.clear()
                win_data_ret.mvwin(column + 3,40)
                winData1.refresh()
                box.edit()
                hour_user[index_gui] = box.gather().strip().replace("\n", "")

                winData1.addstr(3, 16, "Minute # : ")
                win_data_ret.clear()
                win_data_ret.mvwin(column + 3,58)
                winData1.refresh()
                box.edit()
                min_user[index_gui] = box.gather().strip().replace("\n", "")

                winData1.addstr(3, 35, "Second # : ")
                win_data_ret.clear()
                win_data_ret.mvwin(column + 3,77)
                winData1.refresh()
                box.edit()
                sec_user[index_gui] = box.gather().strip().replace("\n", "")


                winData1.addstr(4, 0, "Direction ?        | FWD | REV | REDO | ? ")
                win_data_ret.clear()
                winData1.refresh()

                winData1.attroff(WHITE_AND_MAGENTA)     
                winData1.attron(BLACK_AND_GREEN)

                            # collect data
                curses.noecho()
                winData1.nodelay(True)
                q = 0
                key = ""
                while key != 'w':
                    try:
                        key = winData1.getkey()
                    except:
                        key = None

                    if key == 'a':
                        if q > 0:
                            q -= 1
                        
                    elif key == 'd':
                        if q < 2:
                            q += 1

                    winData1.attroff(BLACK_AND_GREEN)     
                    winData1.attron(WHITE_AND_MAGENTA)
                    winData1.addstr(4, 0, "Direction ?        | FWD | REV | REDO | ? ")
                    winData1.attroff(WHITE_AND_MAGENTA)     
                    winData1.attron(BLACK_AND_GREEN)

                    if q == 0:
                        winData1.addstr(4, 19, "| FWD |")
                        dir_user[index_gui] = 1
                        back_flag[index_gui] = False
                    
                    elif q == 1:
                        winData1.addstr(4, 25, "| REV |")
                        dir_user[index_gui] = 0
                        back_flag[index_gui] = False
                        
                    elif q == 2:
                        winData1.addstr(4, 31, "| REDO |")
                        back_flag[index_gui] = True

                    winData1.refresh()
                    sleep(0.5)
                

    # JOG-RUN metrics
    elif reqGUI == RUN:

            #avoiding a global index here people...forgive me coding gods
            index_gui = int((column-5)/7)

            # User wants to disable the device (leave in standby - no update)
            if poll_user[index_gui] == 0:
                pass

                
             # User sel JOG function
            elif poll_user[index_gui] == 1:

                winRun1 = curses.newwin(5, 60, column, 30)
                winRun1.attron(BLACK_AND_RED)

                winRun1.addstr(0, 0, f" Q [nL/s] : {q_user[index_gui]}    Syringe Radius in [mm] : {radius_user[index_gui]}")
                #winRun1.addstr(4, 18, "| ESTOP |")
                winRun1.refresh()

             # User selected RUN function
            elif poll_user[index_gui] == 2:
                winRun1 = curses.newwin(5, 60, column, 30)
                winRun1.attron(BLACK_AND_RED)

                winRun1.addstr(0, 0, f" Q [nL/s] : {q_user[index_gui]}    Syringe Radius in [mm] : {radius_user[index_gui]}")
                winRun1.addstr(1, 0, f" Requested - Hour : {hour_user[index_gui]}    Minute : {min_user[index_gui]}   Second : {sec_user[index_gui]}")
                winRun1.addstr(2, 0, f" Elapsed - Hour : {ard_duration[index_gui][0]}       Minute :  {ard_duration[index_gui][1]}     Second : {ard_duration[index_gui][2]}    ")
                #winRun1.addstr(4, 18, "| ESTOP |")
                winRun1.refresh()
               
            #sleep(1)


    # Simple ESTOP button
    elif reqGUI == USER_ESTOP:

        COL_CONST = 7
        COL_OFFSET = 5

        global estop_flag

        # display window
        winStop = curses.newwin(1, 60, 25 , 30)
        curses.noecho()
        winStop.nodelay(True)
        winStop.clear()
        winStop.refresh()
            
        key = ""
        try:
            key = winStop.getkey()
        except:
            key = None
                
        if key == 'w':
            if estop_index > 0:
                estop_index -= 1
                
        elif key == 's':
            if estop_index < 3:
                estop_index += 1


        if estop_index < 3 and poll_user[estop_index] > 0:
            winStop.mvwin(estop_index * COL_CONST + 9, 30)
            winStop.attron(curses.color_pair(estop_index))
            winStop.addstr(0,18, "| ESTOP? |")
            winStop.refresh()
        
        elif estop_index == 3:
            winStop.mvwin(25, 30)
            winStop.attron(BLACK_AND_RED)
            winStop.addstr(0,18, "| ESTOP ALL? |")
            winStop.refresh()

                
        if key == 'd':

            # stop indiv device
            if estop_index != 3:
                estop_flag[estop_index] = True
                winStop.attron(BLACK_AND_GREEN)
                winStop.addstr(0,18, "| STOP'D |")
                winStop.refresh()

            # stop all devices
            elif estop_index == 3:
                estop_flag[0] = True
                estop_flag[1] = True
                estop_flag[2] = True
                estop_index = 0
            

    # Presents error string in device window
    elif reqGUI == ERROR:
        #avoiding a global index here people...forgive me coding gods
        index_gui = int((column-5)/7)

        # display window
        winPoll4 = curses.newwin(5, 60, column, 30)
        curses.noecho()
        winPoll4.nodelay(True)
        winPoll4.clear()
        winPoll4.attron(RED_AND_WHITE)
        winPoll4.addstr(2, 10, Error_2Str(user_code))
        winPoll4.refresh()
            

        sleep(8)
        

    else:
        print("Requested GUI Error")

    


wrapper(curs)



if __name__ == "__main__":
    main()
