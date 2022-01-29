# Connor Wilson
# Winter 2022
# SMSS Hub Controller V1.0


from smbus2 import SMBus, i2c_msg
from time import sleep

    
def main():
    
    # I2C Addresses
    PI_ADD = 0x01                       
    ARD_ADD_1 = 0x14
    ARD_ADD_2 = 0x28
    ARD_ADD_3 = 0x42
    rollcall = [0,0,0]                   #availability of arduino deivces

    # Data Handling Variables
    userData = 0
    userDataByte = 1
    RXData = 0
    i2c_error = 0

    # main request codes/storage
    STATE = 0x01
    DISABLED = 0x09
    state_status = [0,0,0]
    

        

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
        

if __name__ == "__main__":
    main(




        
        
        
        
