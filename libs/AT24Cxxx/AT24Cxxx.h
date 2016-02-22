#ifndef _AT24Cxxx
#define _AT24Cxxx

#include <inttypes.h>
#include <Arduino.h>
/*
AT24Cxxx can work at 1MHz at 5V power supply.
But atmega328 not...
 */
#define TWI_FREQ	100000L
#include <Wire.h>

class AT24Cxxx{
	protected:
		int address;
	public:
		AT24Cxxx(int addr = 80);
		~AT24Cxxx();
		void init();
		uint8_t read(uint16_t address);
		void write(uint16_t address, uint8_t byte);
		int writeBuffer(int start, char*buf, int length);
		int readBuffer(int start, char*buf, int length);
};


#endif
