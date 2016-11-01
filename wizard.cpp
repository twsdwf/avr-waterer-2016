#include "Arduino.h"
#include "wizard.h"
#include "configuration.h"
#include "i2cexpander.h"
#include "waterdosersystem.h"

extern Configuration g_cfg;
extern WaterDoserSystem water_doser;
extern I2CExpander i2cExpander;

Wizard::Wizard()
{
	this->n_iic = 0;
	this->i2c_devs = NULL;
}

Wizard::~Wizard()
{
	if(this->i2c_devs) {
		delete[] i2c_devs;
	}
}

char Wizard::ask_char(const __FlashStringHelper* promt, char*values)
{
	delay(500);
	while(Serial1.available()) Serial1.read();
	Serial1.print(promt);
	Serial1.flush();
	while (1) {
		if (Serial1.available()) {
			char ch = Serial1.read();
			Serial1.write(ch);Serial1.flush();
			char*ptr = values;
			while (*ptr) {
				if (*ptr == ch) {
					Serial1.println();
					return toupper(ch);
				}
				++ptr;
			}//while
		}//if avail
	}//while 1
	Serial1.println();
	return 'X';
}//sub

int Wizard::ask_int(const __FlashStringHelper* promt, int min, int max)
{
	delay(500);
	while(Serial1.available()) Serial1.read();
	Serial1.print(promt);
	Serial1.flush();
	int ret = 0;
	while (1) {
		if (Serial1.available()) {
			char ch = Serial1.read();
			Serial1.write(ch);Serial1.flush();
			if(isdigit(ch)) {
				ret *= 10;
				ret += (ch - '0');
			} else if (ch == 10 || ch == 13) {
				if (ret >= min && ret <= max) {
					Serial1.println();
					return ret;
				} else {
					Serial1.println(F("\r\nERROR: value is out of range"));
					return ask_int(promt, min, max);
				}
			} else if (ch == 'X') {
				Serial1.println();
				return -1;
			}
		}//if avail
	}//while 1
	Serial1.println();
	return -1;
}
	
void Wizard::ask_str(const __FlashStringHelper* promt, char*buf, int maxlen)
{
	delay(500);
	while(Serial1.available()) Serial1.read();
	Serial1.print(promt);
	Serial1.flush();
	char*pb = buf;
	while (1) {
		if (Serial1.available()) {
			char ch = Serial1.read();
			Serial1.write(ch);Serial1.flush();
			if (ch == 10 || ch == 13) {
				Serial1.println();
				return;
			}
			*pb++ = ch;
			if (pb - buf >= maxlen - 1) {
				Serial1.println();
				*pb = 0;
				return;
			}
		}
	}//while
}

void Wizard::hello()
{
	Serial1.println(F("=== WIZARD started ==="));
}

void Wizard::exit(int pi)
{
	if (pi > g_cfg.config.pots_count) {
		if ('Y' == ask_char(F("Update pots count(Y/N)?"), "YN")) {
			g_cfg.config.pots_count = pi;
			g_cfg.writeGlobalConfig();
		}
	}
	Serial1.println(F("=== exit from WIZARD ==="));
}

int Wizard::wd_pos_is_free(int x, int y)
{
	
	for(uint8_t i = 0; i < g_cfg.config.pots_count; ++i) {
		potConfig pc = g_cfg.readPot(i);
		if(pc.wc.x == x && pc.wc.y == y) {
			return i;
		}
	}//for i
	return -1;
}//sub

int Wizard::dev_pin_is_free(int dev, int pin)
{
	for (uint8_t i = 0; i < g_cfg.config.pots_count; ++i) {
		potConfig pc = g_cfg.readPot(i);
		if(pc.sensor.dev == dev && pc.sensor.pin == pin) {
			return i;
		}
	}//for i
	return -1;
}

void Wizard::__list_i2c()
{
	uint8_t addr = 32, b1=0, b2=0;
	this->n_iic = 0;
	
	i2cExpander.i2c_on();
	
	while (i2cExpander.findNext(addr, 32 + 7, &addr)) {
// 		Serial1.print(addr,DEC);
// 		Serial1.print(F(","));
		b1 |= 1<<(addr - 32);
		++addr;
		++this->n_iic;
	}
	
	addr = 56;
	while (i2cExpander.findNext(addr, 56 + 7, &addr)) {
// 		Serial1.print(addr,DEC);
// 		Serial1.print(F(","));
		b2 |= 1<<(addr - 56);
		++addr;
		++this->n_iic;
	}
/*	
	Serial1.println();
	Serial1.print("devs:");
	Serial1.println(this->n_iic);
	Serial1.print(b1, HEX);
	Serial1.print(" ");
	Serial1.println(b2, HEX);*/
	
	this->i2c_devs = new uint8_t[this->n_iic];
	
	uint8_t i = 0;
	
	for (uint8_t j=0; j <8; ++j) {
		if (b1 & (1<<j)) {
			this->i2c_devs[i++] = 32 + j;
// 			Serial1.print(32 + j);
// 			Serial1.print(" ");
		}
	}//for j
	
	for (uint8_t j=0; j <8; ++j) {
		if (b2 & (1<<j)) {
			this->i2c_devs[i++] = 56 + j;
// 			Serial1.print(56 + j);
// 			Serial1.print(" ");
		}
	}//for j
// 	Serial1.println();
	i2cExpander.i2c_off();
}


void Wizard::run(uint8_t show_hello)
{
	uint8_t pi = 0, mode = 1;
	char reply;
	potConfig _pc;
	
	if (this->i2c_devs == NULL) {
		this->__list_i2c();
	}
	if (show_hello) {
		this->hello();
	}
	
	if (g_cfg.config.pots_count > 0) {
		reply = ask_char(F("pots > 0. [A]ppend,[O]verwrite,[E]dit pot, e[X]it?"), "AOEX");
		if (reply == 'A') {
			pi = g_cfg.config.pots_count;
		} else if (reply == 'O') {
			pi = 0;
		} else if (reply == 'X') {
			this->exit(pi);
			return;
		} else if (reply == 'E') {
			mode = 0;
			for (uint8_t i = 0; i < g_cfg.config.pots_count; ++i) {
				_pc = g_cfg.readPot(i);
				Serial1.print(i);
				Serial1.print(": ");
				Serial1.print(_pc.name);
				Serial1.print(" (");
				Serial1.print(_pc.wc.x, DEC);
				Serial1.print(",");
				Serial1.print(_pc.wc.y, DEC);
				Serial1.print(") ");
				Serial1.print(_pc.sensor.dev, DEC);
				Serial1.print("/");
				Serial1.println(_pc.sensor.pin, DEC);
			}//for i
			pi = ask_int(F("Pot index:"), 0, g_cfg.config.pots_count - 1); 
			if ( pi < 0) {
				this->exit(pi);
				return;
			} else {
				_pc = g_cfg.readPot(pi);
			}
		}
	}
	
	int x = ask_int(F("Start X from:"), 0, WD_SIZE_X - 1);
	if (x < 0) {
		this->exit(pi);
		return;
	}
	
	int y = ask_int(F("Start Y from:"), 0, WD_SIZE_Y - 1);
	if (y < 0) {
		this->exit(pi);
		return;
	}
	
	int test;
	while(true) {
		test = wd_pos_is_free(x, y);
		if (test > -1) {
			Serial1.print(F("slot ("));
			Serial1.print(x, DEC);
			Serial1.print(",");
			Serial1.print(y, DEC);
			Serial1.print(") is busy by cfg line");
			Serial1.println(test, DEC);
			reply = ask_char(F("[O]verwrite,[I]gnore,[N]ext pos?"), "OIN");
			if(reply == 'N') {
				++y;
				if (y >= WD_SIZE_Y) {
					y = 0;
					++x;
				}
				if (x >= WD_SIZE_X) {
					Serial1.println(F("doser pos is out of range"));
					this->exit(pi);
					return;
				}
				continue;
			} else if ('O' == reply) {
				pi = test;
				_pc = g_cfg.readPot(test);
			}
		}
		
		Serial1.print(F("move to ("));
		Serial1.print(x, DEC);
		Serial1.print(",");
		Serial1.print(y, DEC);
		Serial1.println(")");
		if (water_doser.pipi(x, y, 10) == 0) {
			Serial1.println(F("Error occured"));
			this->exit(pi);
			return;
		}
		_pc.wc.x = x;
		_pc.wc.y = y;
		char reply = ask_char(F("[R]epeat watering,[N]ext step,[S]kip this pos,e[X]it?"), "RNSX");
		if(reply == 'X') {
			this->exit(pi);
			return;
		} else if(reply == 'R') {
			continue;
		} else if (reply == 'S') {
			++y;
			if (y >= WD_SIZE_Y) {
				y = 0;
				++x;
			}
			if (x >= WD_SIZE_X) {
				Serial1.println(F("doser pos is out of range"));
				this->exit(pi);
				return;
			}
			continue;
		}
		
		if (mode) {
			memset(_pc.name, 0, POT_NAME_LENGTH);
			ask_str(F("pot name:"), _pc.name, POT_NAME_LENGTH - 1);
		} else {
			Serial1.print(F("pot name:"));
			Serial1.println(_pc.name);
		}
		
		Serial1.print(F("available chips:"));
		
		for (uint8_t k = 0; k < this->n_iic; ++k) {
			Serial1.print(this->i2c_devs[k], DEC);
			Serial1.print(F(","));
		}
		Serial1.println();
		
		while (1) {
			_pc.sensor.dev = ask_int(F("chip:"), 32, 100);
			
			if (_pc.sensor.dev < 32) {
				this->exit(pi);
				return;
			}
			
			_pc.sensor.pin = ask_int(F("pin:"), 0, 15);
			
			if(_pc.sensor.pin < 0) {
				this->exit(pi);
				return;
			}
			
			test = dev_pin_is_free(_pc.sensor.dev, _pc.sensor.pin);
			if (test > -1) {
				Serial1.print(F("Sensor found at cfg line "));
				Serial1.println(test, DEC);
				reply = ask_char(F("[I]gnore and continue,[C]hange dev & pin, e[X]it from wizard?"), "ICX");
				if ('I' == reply) {
					break;
				} else if ('X' == reply) {
					this->exit(pi);
					return;
				}
			} else {
				break;
			}
		}//while 1
		
		int val = i2cExpander.read_pin(_pc.sensor.dev, _pc.sensor.pin);
		while (true) {
			val = i2cExpander.read_pin(_pc.sensor.dev, _pc.sensor.pin);
			Serial1.print(F("sensor value="));
			Serial1.println(val, DEC);
			if (val < 10) {
				reply = ask_char(F("Sensor value possibly bad. [R]e-read,[I]gnore and continue, e[X]it?"), "RIX");
				if (reply == 'X') {
					this->exit(pi);
					return;
				} else if (reply == 'I') {
					break;
				}
			} else {
				break;
			}
		}//while
		
		_pc.wc.ml = ask_int(F("Dose, ml(5..100):"), 5, 100);
		if (_pc.wc.ml < 5) {
			this->exit(pi);
			return;
		}
		
		int pgm = ask_int(F("program: [1] - const humidity, [2] - interval humidity,[0] - exit"), 0, 2);
		
		if (pgm == 0) {
			this->exit(pi);
			return;
		}
		
		_pc.wc.pgm = pgm;
		
		if (pgm == 1) {
			_pc.pgm.const_hum.value = ask_int(F("barrier value:"), 10, 1023);
			_pc.pgm.const_hum.max_ml = ask_int(F("daymax, ml:"), 1, 1023);
		} else if (pgm == 2) {
			_pc.pgm.hum_and_dry.min_value = ask_int(F("Dry barrier value:"), 10, 1023);
			_pc.pgm.hum_and_dry.max_value = ask_int(F("Water barrier value:"), _pc.pgm.hum_and_dry.min_value + 5, 1023);
			_pc.pgm.hum_and_dry.max_ml = ask_int(F("daymax, ml:"), 1, 1023);
		}
		if (ask_char(F("Write pot config?"), "YN") == 'Y') {
			_pc.wc.airTime = 3;
			_pc.wc.state = 1;
			_pc.wc.enabled = 1;
			g_cfg.savePot(pi, _pc);
			if (mode) {
				if ('X' == ask_char(F("[N]ext position or e[X]it?"), "NX")) {
					this->exit(-1);
					return;
				}
				++pi;
				++y;
				if (y >= WD_SIZE_Y) {
					y = 0;
					++x;
				}
				if (x >= WD_SIZE_X) {
					Serial1.println(F("doser pos is out of range"));
					this->exit(pi);
					return;
				}
			} else {
				run(false);
				return;
			}
		}
	}
	this->exit(-1);
}

