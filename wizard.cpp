#include "Arduino.h"
#include "wizard.h"
#include "configuration.h"
#include "i2cexpander.h"
#include "waterdosersystem.h"
#include "esp8266.h"
#include "wateringcontroller.h"

extern Configuration g_cfg;
extern WaterDoserSystem water_doser;
extern I2CExpander i2cExpander;
extern ESP8266 esp8266;
extern WateringController wctl;

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

void Wizard::cfg_run()
{
	Serial1.println(F("=== CONFIG WIZARD ==="));
	uint16_t h,m;
	if ('Y' == ask_char(F("Change sensor options(Y/N)?"), "YN")) {
		ask_uint16(F("I2C power-on time, ms"), 0, 60000, g_cfg.config.i2c_pwron_timeout);
		ask_uint16(F("Sensor pre-read time, ms"), 0, 60000, g_cfg.config.sensor_init_time);
		ask_uint16(F("Sensor pause between read, ms"), 0, 60000, g_cfg.config.sensor_read_time);
		h = g_cfg.config.sensor_measures;
		g_cfg.config.sensor_measures = ask_uint16(F("Sensor measures"), 0, 20, h);
	}
	
	h = g_cfg.config.pots_count;
	g_cfg.config.pots_count = ask_uint16(F("Pots count"), 0, 100, h);
	if ('Y' == ask_char(F("Change watering time options(Y/N)?"), "YN")) {
		h=g_cfg.config.water_start_time/100, m=g_cfg.config.water_start_time%100;
		ask_uint16(F("Monday-Friday watering start, hour"), 0, 24, h);
		ask_uint16(F("Monday-Friday watering start, minute"), 0, 60, m);
		g_cfg.config.water_start_time = h * 100 + m;
		h = g_cfg.config.water_end_time/100;
		m = g_cfg.config.water_end_time%100;
		ask_uint16(F("Monday-Friday watering end, hour"), 0, 24, h);
		ask_uint16(F("Monday-Friday watering end, minute"), 0, 60, m);
		g_cfg.config.water_end_time = h * 100 + m;

		h = g_cfg.config.water_start_time_we / 100, m = g_cfg.config.water_start_time_we % 100;
		ask_uint16(F("Weekend watering start, hour"), 0, 24, h);
		ask_uint16(F("Weekend watering start, minute"), 0, 60, m);
		g_cfg.config.water_start_time = h * 100 + m;
		h = g_cfg.config.water_end_time_we / 100;
		m = g_cfg.config.water_end_time_we % 100;
		ask_uint16(F("Weekend watering end, hour"), 0, 24, h);
		ask_uint16(F("Weekend watering end, minute"), 0, 60, m);
		g_cfg.config.water_end_time_we = h * 100 + m;
		
	}
	h = g_cfg.config.test_interval;
	g_cfg.config.test_interval = ask_uint16(F("Test interval, minutes"), 0, 180, h);
	
	if ('Y' == ask_char(F("Change water doser Z-axe servo angles(Y/N)?"), "YN")) {
		
		h = g_cfg.config.wdz_ddc;
		do {
			ask_uint16(F("Z-axe down DC:"), 0, 180, h);
			water_doser.servoMove(h);
		} while ('N' == ask_char(F("Is this OK(Y/N)?"), "YN"));
		g_cfg.config.wdz_ddc = h;

		h = g_cfg.config.wdz_tdc;
		do {
			ask_uint16(F("Z-axe top DC:"), 0, 180, h);
			water_doser.servoMove(g_cfg.config.wdz_ddc);
			delay(500);
			water_doser.servoMove(h);
		} while ('N' == ask_char(F("Is this OK(Y/N)?"), "YN"));
		g_cfg.config.wdz_tdc = h;
		
		h = g_cfg.config.wdz_top;
		do {
			ask_uint16(F("Z-axe top default pos:"), 0, 180, h);
			water_doser.servoMove(g_cfg.config.wdz_tdc);
			delay(500);
			water_doser.servoMove(h);
		} while ('N' == ask_char(F("Is this OK(Y/N)?"), "YN"));
		g_cfg.config.wdz_top = h;
	}
	
	g_cfg.config.enabled = (ask_char(F("Enable watering(Y/N)?"), "YN") == 'Y');
	
	h = (g_cfg.config.flags & F_ESP_USING)? 1 : 0;
	
	m = (ask_char(F("Use esp8266(Y/N)?"), "YN") == 'Y') ? 1 : 0;
	if ( m > h ) {
		esp8266.begin(38400, ESP_RST_PIN);
		esp8266.connect();
	}
	g_cfg.config.esp_en = m;
	
	g_cfg.writeGlobalConfig();
	Serial1.println(F("saved"));
}

char Wizard::ask_char(const __FlashStringHelper* promt, char*values)
{
	delay(500);
	while(Serial1.available()) Serial1.read();
	Serial1.print(promt);
	Serial1.flush();
	while (1) {
		if (Serial1.available()) {
			char ch = toupper(Serial1.read());
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


uint16_t Wizard::ask_uint16(const __FlashStringHelper* promt, uint16_t min, uint16_t max, uint16_t& curval)
{
	delay(500);
	while(Serial1.available()) Serial1.read();
	Serial1.print(promt);
	Serial1.print("(current value ");
	Serial1.print(curval, DEC);
	Serial1.print("): ");
// 	Serial1.flush();
	uint16_t ret = 0, nch=0;
	while (1) {
		if (Serial1.available()) {
			char ch = Serial1.read();
			if (isdigit(ch)) {
				Serial1.write(ch);
				ret *= 10;
				ret += (ch - '0');
				++nch;
			} else if (ch == 10 || ch == 13) {
				if (nch > 0 && ret >= min && ret <= max || nch ==0) {
					if (nch) {
						curval = ret;
					} else {
						Serial1.print(curval, DEC);
						ret = curval;
					}
					Serial1.println();
					return ret;
				} else {
					Serial1.println(F("\r\nERROR: value is out of range"));
					return ask_uint16(promt, min, max, curval);
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
			} else if (toupper(ch) == 'X') {
				Serial1.println();
				return -1;
			}
		}//if avail
	}//while 1
	Serial1.println();
	return -1;
}
	
void Wizard::ask_str(const __FlashStringHelper* promt, char*buf, int maxlen,bool buf_has_curval)
{
	delay(500);
	while(Serial1.available()) Serial1.read();
	Serial1.print(promt);
	if (buf_has_curval) {
		Serial1.print(F(" (current value:"));
		Serial1.print(buf);
		Serial1.println(F(")"));
	}
// 	Serial1.flush();
	char*pb = buf;
	while (1) {
		if (Serial1.available()) {
			char ch = Serial1.read();
			Serial1.write(ch);
			Serial1.flush();
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
	
	for (uint8_t i = 0; i < g_cfg.config.pots_count; ++i) {
		potConfig pc = g_cfg.readPot(i);
		if (pc.wc.x == x && pc.wc.y == y) {
			return i;
		}
	}//for i
	return -1;
}//sub

int Wizard::dev_pin_is_free(int dev, int pin)
{
	for (uint8_t i = 0; i < g_cfg.config.pots_count; ++i) {
		potConfig pc = g_cfg.readPot(i);
		if (pc.sensor.dev == dev && pc.sensor.pin == pin) {
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
		b1 |= 1<<(addr - 32);
		++addr;
		++this->n_iic;
	}
	
	addr = 56;
	while (i2cExpander.findNext(addr, 56 + 7, &addr)) {
		b2 |= 1<<(addr - 56);
		++addr;
		++this->n_iic;
	}

	this->i2c_devs = new uint8_t[this->n_iic];
	
	uint8_t i = 0;
	
	for (uint8_t j=0; j <8; ++j) {
		if (b1 & (1<<j)) {
			this->i2c_devs[i++] = 32 + j;
		}
	}//for j
	
	for (uint8_t j=0; j <8; ++j) {
		if (b2 & (1<<j)) {
			this->i2c_devs[i++] = 56 + j;
		}
	}//for j
	i2cExpander.i2c_off();
}

void Wizard::edit_pot(uint8_t index)
{
	if (index >= g_cfg.config.pots_count) {
		Serial1.println(F("Index out of bounds"));
		return;
	}
	potConfig pc = g_cfg.readPot(index);
	char reply;
	uint16_t x, y;
	
	ask_str(F("plant name"), pc.name, POT_NAME_LENGTH - 1, true);
	x = pc.wc.x;
	y = pc.wc.y;
	ask_uint16(F("water doser X pos"), 0, WD_SIZE_Y - 1, x);
	pc.wc.x = x;
	ask_uint16(F("water doser Y pos"), 0, WD_SIZE_Y - 1, y);
	pc.wc.y = y;
	x = pc.sensor.dev;
	y = pc.sensor.pin;
	ask_uint16(F("Chip address"), 32, 56+8, x);
	ask_uint16(F("Pin index on chip"), 0, 15, y);
	pc.sensor.dev = x;
	pc.sensor.pin = y;
	
	int val = i2cExpander.read_pin(pc.sensor.dev, pc.sensor.pin);
	
	while (true) {
		val = i2cExpander.read_pin(pc.sensor.dev, pc.sensor.pin);
		Serial1.print(F("sensor value:"));
		Serial1.println(val, DEC);
		if (val < 10) {
			reply = ask_char(F("Sensor value possibly bad. [R]e-read,[I]gnore and continue, e[X]it?"), "RIX");
			if (reply == 'X') {
				this->exit(index);
				return;
			} else if (reply == 'I') {
				break;
			}
		} else {
			break;
		}
	}//while

	x = pc.wc.airTime;
	ask_uint16(F("Air time(1-short, 2-medium, 3-long)"), 1, 3, x);
	pc.wc.airTime = x;
	pc.wc.enabled = ask_char(F("Enable watering(Y/N)"), "YN") == 'Y';
	x = pc.wc.ml;
	ask_uint16(F("Portion, ml(5..100):"), 5, 100, x);
	pc.wc.ml = x;

	x = pc.wc.pgm;
	ask_uint16(F("Watering program (1 - const humidity, 2 - interval humidity)"), 1, 2, x);
	pc.wc.pgm = x;
	if (pc.wc.pgm  == 1) {
		x = pc.pgm.const_hum.value;
		pc.pgm.const_hum.value = ask_uint16(F("barrier value:"), 10, 1023, x);
		x = pc.pgm.const_hum.max_ml;
		pc.pgm.const_hum.max_ml = ask_uint16(F("daymax, ml:"), 1, 1023, x);
	} else if (pc.wc.pgm == 2) {
		x = pc.pgm.hum_and_dry.min_value;
		pc.pgm.hum_and_dry.min_value = ask_uint16(F("Dry barrier value:"), 10, 1023, x);
		x =pc.pgm.hum_and_dry.max_value;
		pc.pgm.hum_and_dry.max_value = ask_uint16(F("Water barrier value:"), pc.pgm.hum_and_dry.min_value + 5, 1023, x);
		x = pc.pgm.hum_and_dry.max_ml;
		pc.pgm.hum_and_dry.max_ml = ask_uint16(F("daymax, ml:"), 1, 1023, x);
	}
	
	if (ask_char(F("Write pot config?"), "YN") == 'Y') {
		g_cfg.savePot(index, pc);
	}
	if (ask_char(F("Clear day stat for plant?"), "YN") == 'Y') {
		wctl.writeDayML(index, 0);
	}
	
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
				this->edit_pot(pi);
				this->exit(pi);
				return;
			}
		}
	}
	if ('Y' == ask_char(F("Show all free water doser positions(Y/N)?"), "YN")) {
		uint16_t busy[WD_SIZE_X]={0};
		for (uint8_t i = 0; i < g_cfg.config.pots_count; ++i) {
			potConfig pc = g_cfg.readPot(i);
			busy[pc.wc.x] |= (1 << pc.wc.y);
		}
		for (uint8_t i =0; i< WD_SIZE_X; ++i) {
			if (busy[i] == 0) {
				continue;
			}
			Serial1.print(F("x="));
			Serial1.print(i);
			Serial1.print(" y:");
			for (uint8_t j = 0; j < WD_SIZE_Y; ++j) {
				if (0 == (busy[i] & (1<<j))) {
					Serial1.print(j, DEC);
					Serial1.print(",");
				}
			}//for j
			Serial1.println();
		}//for i
	}//if show all wd pos
	
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
		
		_pc.wc.ml = ask_int(F("Portion, ml(5..100):"), 5, 100);
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

