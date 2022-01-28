
# Purpose : Communicate with the arduino as a main device
# Context : Pi will give Ard a number, and output its response to serial

# NOTES : Pi can only assume main (good? bad?) and arduino can only onRequest/onReceive

from smbus2 import SMBus, i2c_msg
from time import sleep
# Serial input

    
def main():
    
    # I2C Addresses
    PI_ADD = 0x01
    ARD_ADD_1 = 0x14
    ARD_ADD_2 = 0x28
    ARD_ADD_3 = 0x42
    OFFSET = 0

    # Data Handling Variables
    userData = 0
    userDataByte = 1
    RXData = 0
    i2c_error = 0

    userData = input(" Give me a number betwee 0-9 :      ")

    while(RXData == False ):
        
        with SMBus(1) as bus: # write to BUS
            bus.write_byte(ARD_ADD_2, int(userData))
            sleep(0.2)
            RXData = bus.read_byte(ARD_ADD_2, OFFSET)

            
            
        print("Your number was : ", userData, " and can be divisable by 2? :", RXData)
            
        userData = input(" Give me a number betwee 0-9 :    ")



if __name__ == "__main__":
    main()
    


    

