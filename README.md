SMSS22 : Smart Motor Syringe System
Winter 2022
Connor Wilson 

This repository is the lastest installment for the Smart Motor Syringe System.

~ Hub.py is the Hub controller file (raspberry Pi) that controls the three arduino motor controllers
	- The arduino motor controllers use Motor_controller.ino
		> Motor_controller.ino can be toggled to operate with the Hub.py using the #define I2C_COMMS
			or serial comms through the arduino IDE #define SERIAL_comms
			
~ Curses_Practice is a folder for getting familiar with the curses terminal GUI 

~ I2C_Practice is a folder for becoming familiar with I2C comms between the raspberry Pi's SMBus and arduino Wire libraries 


