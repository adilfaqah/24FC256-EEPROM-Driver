# 24FC256-EEPROM-Driver
The included device driver provides an interface between the Galileo and Microchip 24FC256 EEPROM over I2C. Work queues are utilized in order to implement non-blocking functionality. The functions implemented are:

- Read 
- Write
- IO Control
	- FLASHGETS  Get the current state (busy/not busy) of the EEPROM
	- FLASHGETP  Get the current address pointer location
	- FLASHSETP  Set the address pointer to the given value
	- FLASHERASE Erase the EEPROM i.e. write 0xFF to all the locations.	
	
The wiring of the 24FC256 EEPROM IC is as follows:
PIN 1 - GROUND
PIN 2 - GROUND
PIN 3 - VDD
PIN 4 - GROUND

PIN 5 - SDA
PIN 6 - SCL
PIN 7 - GROUND
PIN 8 - VDD

The operating voltage is between 1.7/5.5 and hence VDD can be connected to any suitable voltage source. The Galileo board has on-board 5v and 3.3v supply.

The wiring of the LED is as follows:
+ve leg - VDD
-ve leg (in series with a resistor) - IO8

----

A user-space test program is also included to test and verify the functionality of the driver. Use the included makefile to build the driver and the userpace test program. Transfer the files to Galileo. After installing the module, run the user-space test program by entering: ./test
