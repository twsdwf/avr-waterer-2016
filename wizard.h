#ifndef __WIZARD_H_INCLUDED_
#define __WIZARD_H_INCLUDED_

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
	void hello();
	void exit(int pi);
	
	int wd_pos_is_free(int x, int y);
	int dev_pin_is_free(int dev, int pin);
	
	char ask_char(const __FlashStringHelper* promt, char*values);
	int ask_int(const __FlashStringHelper* promt, int min, int max);
	void ask_str(const __FlashStringHelper* promt, char*buf, int maxlen);
	
};

#endif