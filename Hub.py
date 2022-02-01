# Connor Wilson
# Winter 2022
# SMSS Hub Controller V1.0


from smbus2 import SMBus, i2c_msg        # I2C library
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


global FRAME, POLL, START, JOG, RUN_J, RUN_S
global reqGUI
reqGUI = 0x0
reqGUI = 0x0
FRAME = 0x0
POLL = 0x1
START = 0x2
JOG = 0x3
RUN_J = 0x4
RUN_S = 0x5
    
def main():
    
    # I2C Addresses
    PI_ADD = 0x01                       
    ARD_ADD_1 = 0x14
    ARD_ADD_2 = 0x28
    ARD_ADD_3 = 0x42
    address = [ARD_ADD_1, ARD_ADD_2, ARD_ADD_3]

    #availability of arduino deivces 0 = false, 1 = true
    rollcall = [0,0,0]

    # GUI command codes
    global FRAME, POLL, START, JOG, RUN_J, RUN_S
    global reqGUI

    # PI to Arduino command codes
    global CONTACT                # First contact for devices on bus 
    CONTACT = 0x4

    #curses window
    stdscr = curses.initscr()
    
    

    #state machine init
    Hub = HubMachine("Wilson")

    while True:

        if Hub.state == 'START' :

            #Identify devices on the buffer
            rollcall = deviceRollcall(address)
            # TEST print(rollcall)
            sleep(1)

            #Display GUI frame and windows (based on rollcall)
            curs(stdscr)

            # Poll for user input on GUI

            # Wait for confirm

            # Send requested states to devices

        elif Hub.state == 'DATAPULL' :
            pass

        elif Hub.state == 'RUN' :
            pass

        elif Hub.state == 'RESET' : 
            pass

        elif Hub.state == 'REBOOT' : 
            pass

        else: 
            pass


    

        

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


    

def curs(stdscr):

    global FRAME, POLL, START, JOG, RUN_J, RUN_S
    global reqGUI
    
    # Color combinations (ID, foreground, background)
    curses.init_pair(1, curses.COLOR_RED, curses.COLOR_WHITE)
    RED_AND_WHITE = curses.color_pair(1)

    curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_WHITE)
    BLACK_AND_WHITE = curses.color_pair(2)

    curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_GREEN)
    WHITE_AND_GREEN = curses.color_pair(3)

    curses.init_pair(4, curses.COLOR_YELLOW, curses.COLOR_CYAN)
    YELLOW_AND_CYAN = curses.color_pair(4)
    

    # Display selection
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


        #Fill in device squares
        stdscr.attroff(YELLOW_AND_CYAN)
        stdscr.attron(BLACK_AND_WHITE)
        
        stdscr.move(5,10)
        stdscr.addstr("Device 1 :")
        stdscr.move(5,30)

        for y in range(5,11):
            for x in range(30, 90-1):
                stdscr.addch(" ")
            stdscr.move(y,30)

        stdscr.move(12,10)
        stdscr.addstr("Device 2 :")
        stdscr.move(12,30)

        for y in range(12,18):
            for x in range(30, 90-1):
                stdscr.addch(" ")
            stdscr.move(y,30)

        stdscr.move(19,10)
        stdscr.addstr("Device 3 :")
        stdscr.move(19,30)

        for y in range(19,25):
            for x in range(30, 90-1):
                stdscr.addch(" ")
            stdscr.move(y,30)


        stdscr.move(21,40)

        rectangle(stdscr, 25, 35, 26, 80)
            
        stdscr.move(5,30)






            


        
        stdscr.refresh()
        
        
        stdscr.getch()
    
    elif reqGUI == POLL:
        pass

    elif reqGUI == START:
        pass

    elif reqGUI == JOG:
        pass

    elif reqGUI == RUN_J:
        pass

    elif reqGUI == RUN_S:
        pass

    else:
        print("Requested GUI Error")

    
    

wrapper(curs)



if __name__ == "__main__":
    main()
