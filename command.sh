rm ./cswitch.o
//TODO
avr-gcc -c -O2 -mmcu=atmega2560 -Wa,--gstabs -o cswitch.o cswitch.s
avr-gcc -Os -DF_CPU=16000000 -mmcu=atmega2560 -c os.c -o os.o
avr-gcc -mmcu=atmega2560 active.o cswitch.o os.o -o os.elf 
avr-objcopy -O ihex -R .eeprom os.elf os.hex
avrdude -v -p atmega2560 -c wiring -P /dev/cu.usbmodem1451 -b 115200 -D -U flash:w:os.hex