#ifndef __I2CEXPANDER_INC_
#define __I2CEXPANDER_INC_

#include <PCF8574.h>

class I2CExpander:public PCF8574{

protected:
	bool i2c_is_on;
// 	uint8_t mode;//0 -- print only, 1 -- write no-soil freq, 2 -- dry freq, 3 -- water freq
public:
	I2CExpander();
	int16_t read_pin(uint8_t addr, uint8_t pin);
	virtual ~I2CExpander();
// 	uint8_t get_mode();
// 	void set_mode(uint8_t m);
	bool i2c_on();
	void i2c_off();
	bool findNext(uint8_t from_addr, uint8_t to_addr, uint8_t* founded_dev_addr);
	bool ping(bool print_out=true);
};


#endif
