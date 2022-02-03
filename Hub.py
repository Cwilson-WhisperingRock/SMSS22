# Connor Wilson
# Winter 2022
# SMSS Hub Controller V1.0


#from smbus2 import SMBus, i2c_msg        # I2C library
from time import sleep                   # python delay

from transitions import Machine          # state machine

import curses                            # GUI
from curses import wrapper
from curses.textpad import Textbox, rectangle



class HubMachine(object):

    states = ['START', 'DATAPULL', 'RUN', 'RESET']   

    def __init__(self,name):
        self.name = name

        # States and their corresponding triggers/transitions
        self.machine = Machine(model= self, states = HubMachine.states, initial = 'START')
        self.machine.add_transition(trigger = 'CONFIRM', source = 'START', dest = 'DATAPULL')
        self.machine.add_transition(trigger = 'EVENTSTART', source = 'DATAPULL', dest = 'RUN')
        self.machine.add_transition(trigger = 'DONE', source = 'RUN', dest = 'RESET')
        self.machine.add_transition(trigger = 'RETURN', source = 'RESET', dest = 'START')
        self.machine.add_transition(trigger = 'REBOOT', source = '*', dest = 'RESET')

# States for GUI
global FRAME, POLL, DATA, RUN
global reqGUI
reqGUI = 0x0
FRAME = 0x0
POLL = 0x1
DATA = 0x2
RUN = 0x3


# Data between user-curses-arduino
global rollcall, poll_user, q_user, radius_user, cap_user
global hour_user, min_user, sec_user, dir_user
rollcall = [0, 0, 0]
poll_user = [0, 0, 0]
q_user = [0,0,0]
radius_user = [0,0,0]
cap_user = [0,0,0]
hour_user = [0,0,0]
min_user = [0,0,0]
sec_user = [0,0,0]
dir_user = [0,0,0]

    
def main():
    
    # I2C Addresses
    PI_ADD = 0x01                       
    ARD_ADD_1 = 0x14
    ARD_ADD_2 = 0x28
    ARD_ADD_3 = 0x42
    address = [ARD_ADD_1, ARD_ADD_2, ARD_ADD_3]

    #availability of arduino deivces 0 = false, 1 = true
    global rollcall
    rollcall = [1,0,1]

    # GUI command codes
    global FRAME, POLL, DATA, RUN
    global reqGUI

    # PI to Arduino command codes
    #global CONTACT                # First contact for devices on bus 
    #CONTACT = 0x4

    #curses window
    stdscr = curses.initscr()
    
    #state machine init
    Hub = HubMachine("SMSS")

    while True:

        if Hub.state == 'START' :

            #Identify devices on the buffer
            #rollcall = deviceRollcall(address)
            sleep(1)

            #Display GUI background frame
            reqGUI = FRAME
            curs(stdscr)
            
            # Poll user for device run modes
            reqGUI = POLL
            curs(stdscr)

            Hub.CONFIRM()


        elif Hub.state == 'DATAPULL' :
            reqGUI = DATA
            curs(stdscr)

        elif Hub.state == 'RUN' :
            pass

        elif Hub.state == 'RESET' : 
            pass

        elif Hub.state == 'REBOOT' : 
            pass

        else: 
            pass


"""    

        

# transmit__all_byte()
# SIGNATURE : bool, int -> 
# PURPOSE : Transmits identical byte of data to all enabled arduinos
#           
def transmit_all_byte(bool_array, int_data):
        
    with SMBus(1) as bus: # write to BUS

        # device one is requested active -> write
        if bool_array[0] > 0 :
            bus.write_byte(ARD_ADD_1, int_data)

        # device two is requested active -> write
        if bool_array[1] > 0 :
            bus.write_byte(ARD_ADD_2, int_data)

        # device three is requested active -> write
        if bool_array[2] > 0 :
            bus.write_byte(ARD_ADD_3, int_data)


# recv_all_byte()
# SIGNATURE : bool -> int
# PURPOSE : Reads identical byte of data from all enabled arduinos
#           and Returns array of resulting byte
def recv_all_byte(bool_array):

    stor = [0,0,0]
        
    with SMBus(1) as bus: # write to BUS

        # device one is requested active -> write
        if bool_array[0] > 0 :
            stor[0] = bus.read_byte(ARD_ADD_1, 0)

        # device two is requested active -> write
        if bool_array[1] > 0 :
            stor[1] = bus.read_byte(ARD_ADD_2, 0)

        # device three is requested active -> write
        if bool_array[2] > 0 :
            stor[2] = bus.read_byte(ARD_ADD_3, 0)

        return stor
        

# deviceRollcall()
# SIGNATURE : int_array -> int_array
# Purpose : Sends byte to each device using I2C and 
#           Stores 1 in array for a sucessful transmission (device active)
#           Stores 0 in array for unsuccessful transmission (device not active)
def deviceRollcall(myaddress):

    dev_status = [0, 0, 0]

    with SMBus(1) as bus: # write to BUS

        # Try to contact each device
        try:
            bus.write_byte(myaddress[0], CONTACT)
            dev_status[0] = 1
        except:
            pass
            #print("Device 1 not on bus")

        try:
            bus.write_byte(myaddress[1], CONTACT)
            dev_status[1] = 1
        except:
            pass
            #print("Device 2 not on bus")

        try:
            bus.write_byte(myaddress[2], CONTACT)
            dev_status[2] = 1
        except:
            pass
            #print("Device 3 not on bus")


    return dev_status


   """ 

def curs(stdscr):

    global FRAME, POLL, DATA, RUN
    global reqGUI
    global rollcall, poll_user, q_user, radius_user, cap_user
    global hour_user, min_user, sec_user, dir_user
    
    # Color combinations (ID, foreground, background)
    curses.init_pair(1, curses.COLOR_RED, curses.COLOR_WHITE)
    RED_AND_WHITE = curses.color_pair(1)

    curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_WHITE)
    BLACK_AND_WHITE = curses.color_pair(2)

    curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_GREEN)
    WHITE_AND_GREEN = curses.color_pair(3)

    curses.init_pair(4, curses.COLOR_YELLOW, curses.COLOR_CYAN)
    YELLOW_AND_CYAN = curses.color_pair(4)

    curses.init_pair(5, curses.COLOR_WHITE, curses.COLOR_MAGENTA)
    WHITE_AND_MAGENTA = curses.color_pair(5)
    

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
        x = 0

        if rollcall[0] > 0:
            # display window
            winPoll1 = curses.newwin(5, 60, 5, 30)
            curses.noecho()
            winPoll1.nodelay(True)
            winPoll1.clear()
            winPoll1.attron(WHITE_AND_GREEN)
            winPoll1.addstr(1, 5, "Device 1 Mode :      | OFF | JOG | RUN |")
            winPoll1.addstr(3, 5, "Error Messages : ")
            winPoll1.refresh()
            
            # collect data
            q = 0
            key = ""

            while key is not 'w':

                winPoll1.attroff(BLACK_AND_WHITE)     
                winPoll1.attron(WHITE_AND_GREEN)
                winPoll1.addstr(1, 5, "Device 1 Mode :      | OFF | JOG | RUN |")

                try:
                    key = winPoll1.getkey()
                except:
                    key = None

                if key is 'a':
                    if q > 0:
                        q -= 1
                        
                elif key is 'd':
                    if q < 2:
                        q += 1


                if q is 0:
                    winPoll1.attroff(WHITE_AND_GREEN)
                    winPoll1.attron(BLACK_AND_WHITE)
                    winPoll1.addstr(1, 26, "| OFF |")
                    poll_user[0] = 0                    # disable the device
                    
                elif q is 1:
                    winPoll1.attroff(WHITE_AND_GREEN)
                    winPoll1.attron(BLACK_AND_WHITE)
                    winPoll1.addstr(1, 32, "| JOG |")
                    poll_user[0] = 1                    # jog the device

                elif q is 2:
                    winPoll1.attroff(WHITE_AND_GREEN)
                    winPoll1.attron(BLACK_AND_WHITE)
                    winPoll1.addstr(1, 38, "| RUN |")
                    poll_user[0] = 2                    # run the device
                        
                
                winPoll1.refresh()
                sleep(0.75)



        if rollcall[1] > 0:
            # display window
            winPoll2 = curses.newwin(5, 60, 12, 30)
            curses.noecho()
            winPoll2.nodelay(True)
            winPoll2.clear()
            winPoll2.attron(WHITE_AND_GREEN)
            winPoll2.addstr(1, 5, "Device 1 Mode :      | OFF | JOG | RUN |")
            winPoll2.addstr(3, 5, "Error Messages : ")
            winPoll2.refresh()
            
            # collect data
            q = 0
            key = ""

            while key is not 'w':

                winPoll2.attroff(BLACK_AND_WHITE)     
                winPoll2.attron(WHITE_AND_GREEN)
                winPoll2.addstr(1, 5, "Device 1 Mode :      | OFF | JOG | RUN |")

                try:
                    key = winPoll2.getkey()
                except:
                    key = None

                if key is 'a':
                    if q > 0:
                        q -= 1
                        
                elif key is 'd':
                    if q < 2:
                        q += 1


                if q is 0:
                    winPoll2.attroff(WHITE_AND_GREEN)
                    winPoll2.attron(BLACK_AND_WHITE)
                    winPoll2.addstr(1, 26, "| OFF |")
                    poll_user[1] = 0                    # disable the device
                    
                elif q is 1:
                    winPoll2.attroff(WHITE_AND_GREEN)
                    winPoll2.attron(BLACK_AND_WHITE)
                    winPoll2.addstr(1, 32, "| JOG |")
                    poll_user[1] = 1                    # jog the device

                elif q is 2:
                    winPoll2.attroff(WHITE_AND_GREEN)
                    winPoll2.attron(BLACK_AND_WHITE)
                    winPoll2.addstr(1, 38, "| RUN |")
                    poll_user[1] = 2                    # run the device
                        
                
                winPoll2.refresh()
                sleep(0.75)

        if rollcall[2] > 0:
            # display window
            winPoll3 = curses.newwin(5, 60, 19, 30)
            curses.noecho()
            winPoll3.nodelay(True)
            winPoll3.clear()
            winPoll3.attron(WHITE_AND_GREEN)
            winPoll3.addstr(1, 5, "Device 1 Mode :      | OFF | JOG | RUN |")
            winPoll3.addstr(3, 5, "Error Messages : ")
            winPoll3.refresh()
            
            # collect data
            q = 0
            key = ""

            while key is not 'w':

                winPoll3.attroff(BLACK_AND_WHITE)     
                winPoll3.attron(WHITE_AND_GREEN)
                winPoll3.addstr(1, 5, "Device 1 Mode :      | OFF | JOG | RUN |")

                try:
                    key = winPoll3.getkey()
                except:
                    key = None

                if key is 'a':
                    if q > 0:
                        q -= 1
                        
                elif key is 'd':
                    if q < 2:
                        q += 1


                if q is 0:
                    winPoll3.attroff(WHITE_AND_GREEN)
                    winPoll3.attron(BLACK_AND_WHITE)
                    winPoll3.addstr(1, 26, "| OFF |")
                    poll_user[2] = 0                    # disable the device
                    
                elif q is 1:
                    winPoll3.attroff(WHITE_AND_GREEN)
                    winPoll3.attron(BLACK_AND_WHITE)
                    winPoll3.addstr(1, 32, "| JOG |")
                    poll_user[2] = 1                    # jog the device

                elif q is 2:
                    winPoll3.attroff(WHITE_AND_GREEN)
                    winPoll3.attron(BLACK_AND_WHITE)
                    winPoll3.addstr(1, 38, "| RUN |")
                    poll_user[2] = 2                    # run the device
                        
                
                winPoll3.refresh()
                sleep(0.75)

        # display window
            winPoll4 = curses.newwin(1, 46, 25, 35)
            curses.noecho()
            winPoll4.nodelay(True)
            winPoll4.clear()
            winPoll4.attron(WHITE_AND_GREEN)
            winPoll4.addstr(0, 18, "| CONFIRM? |")
            winPoll4.refresh()
            
            key = ""

            while key is not 'w':

                try:
                    key = winPoll3.getkey()
                except:
                    key = None
                
            winPoll4.refresh()
            sleep(0.75)
        
    # Poll metrics for running in START
    elif reqGUI == DATA:

        # Act if device one is on the bus
        if rollcall[0] > 0:

            # User wants to diable the device (leave in standby - no update)
            if poll_user[0] == 0:
                pass

            # User selected JOG function
            elif poll_user[0] == 1:
                winData1 = curses.newwin(5, 60, 5, 30)
                winData1.attron(WHITE_AND_MAGENTA)
                winData1.refresh()




        # Act if device two is on the bus
        if rollcall[1] > 0:
            #winData2 = curses.newwin(5, 60, 12, 30)
            pass


        # Act if device three is on the bus
        if rollcall[2] > 0:
            #winData3 = curses.newwin(5, 60, 19, 30)
            pass

        winData4 = curses.newwin(1, 46, 25, 35)

    # JOG-RUN metrics
    elif reqGUI == RUN:
        pass

    else:
        print("Requested GUI Error")

    


wrapper(curs)



if __name__ == "__main__":
    main()
