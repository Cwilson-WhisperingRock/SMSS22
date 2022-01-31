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




    
def main():
    
    # I2C Addresses
    PI_ADD = 0x01                       
    ARD_ADD_1 = 0x14
    ARD_ADD_2 = 0x28
    ARD_ADD_3 = 0x42
    address = [ARD_ADD_1, ARD_ADD_2, ARD_ADD_3]

    #availability of arduino deivces 0 = false, 1 = true
    rollcall = [0,0,0]                   

    # PI to Arduino commands
    global CONTACT 
    CONTACT = 0x4
    
    

    #state machine init
    Hub = HubMachine("Wilson")

    while True:

        if Hub.state == 'START' :

            #Identify devices on the buffer
            rollcall = deviceRollcall(address)
            print(rollcall)

            #Display GUI frame and windows (based on rollcall)

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
            bus.write_byte(ARD_ADD_1, CONTACT)
            dev_status[0] = 1
        except:
            print("Device 1 not on bus")

        try:
            bus.write_byte(ARD_ADD_2, CONTACT)
            dev_status[1] = 1
        except:
            print("Device 2 not on bus")

        try:
            bus.write_byte(ARD_ADD_3, CONTACT)
            dev_status[2] = 1
        except:
            print("Device 3 not on bus")


    return dev_status


    

def curs(stdscr):
    pass

wrapper(curs)



if __name__ == "__main__":
    main()