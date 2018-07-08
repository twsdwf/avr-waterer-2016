#ifndef __WIZARD_H_INCLUDED_
#define __WIZARD_H_INCLUDED_

#include "awtypes.h"

class Wizard
{
protected:
	uint8_t*i2c_devs;
	uint8_t n_iic;
	void __list_i2c();
public:
	Wizard();
	~Wizard();
	void run(uint8_t show_hello=1);
	void cfg_run();
	void hello();
	void exit(int pi);
	void edit_pot(uint8_t index);
	int wd_pos_is_free(int x, int y);
	int dev_pin_is_free(int dev, int pin);
	void getPgm(potConfig &pc, uint8_t pot_index);
	char ask_char(const __FlashStringHelper* promt, char*values);
	int ask_int(const __FlashStringHelper* promt, int min, int max);
	uint16_t ask_uint16(const __FlashStringHelper* promt, uint16_t min, uint16_t max, uint16_t& curval);
	void ask_str(const __FlashStringHelper* promt, char*buf, int maxlen, bool buf_has_curval=false);
	
};

#endif