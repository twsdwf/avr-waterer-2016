
#include "Arduino.h"
#include <Wire.h>
#include <AT24Cxxx.h>
#include "config_defines.h"
#include "configuration.h"

#include "i2cexpander.h"
extern "C" {
	#include "utility/twi.h"  // from Wire library, so we can do bus scanning
}

static volatile uint32_t ticks = 0;

extern Configuration g_cfg;

I2CExpander::I2CExpander(){
	i2c_is_on = false;
	mode = 0;
}
I2CExpander::~I2CExpander(){

}

uint8_t I2CExpander::get_mode()
{
	return this->mode;
}

void I2CExpander::set_mode(uint8_t m)
{
	this->mode = m;
// 	Serial1.print("mode=");
// 	Serial1.println(m);
}

bool I2CExpander::i2c_on()
{
	if (this->i2c_is_on) return true;
	DDRE |= (1<<I2C_EXPANDERS_EN_PIN);
	I2C_EXPANDERS_EN_PORT |= (1<<I2C_EXPANDERS_EN_PIN);
// 	Serial1.print("on. wait ");
// 	Serial1.println(g_cfg.gcfg.i2c_pwron_timeout, DEC);
	delay(g_cfg.gcfg.i2c_pwron_timeout);
	this->i2c_is_on = true;
// 	Serial1.println("Wire::begin();");
// 	Serial1.flush();
// 	delay(1000);
 	Wire.begin();
// 	Serial1.println("i2c is ON now");
	return false;
}

void I2CExpander::i2c_off()
{
	if(!this->i2c_is_on) return;
	I2C_EXPANDERS_EN_PORT &= ~(1<<I2C_EXPANDERS_EN_PIN);
	this->i2c_is_on = false;
}

bool I2CExpander::findNext(uint8_t from_addr, uint8_t to_addr, uint8_t*founded_dev_addr)
{
	if (!i2c_is_on) return false;
	uint8_t rc;
	uint8_t data = 0; // not used, just an address to feed to twi_writeTo()
	for ( uint8_t addr = from_addr; addr <= to_addr; addr++ ) {
		rc = twi_writeTo(addr, &data, 0, 1, 0);
// 			Serial1.print(addr, DEC);
// 			Serial1.print(":");
// 			Serial1.println(rc, DEC);
		if (rc == 0) {
			*founded_dev_addr = addr;
			return true;
		}
	}//for addr
	return false;
}

int32_t I2CExpander::read_pin_value(uint8_t addr, uint8_t pin)
{
	uint32_t n = 0;
	bool should_off = false;
	if(!i2c_is_on) {
		i2c_on();
		should_off = true;
	}
	///TODO: медиана, а не среднее арифметическое
	for (int i = 0 ; i < g_cfg.gcfg.sensor_measures; ++i) {
		n += read_pin(addr, pin);
// 		Serial1.println(n, DEC);
	}

	if (should_off) {
		i2c_off();
	}
	return round( (n / g_cfg.gcfg.sensor_measures)*(1000./g_cfg.gcfg.sensor_read_time) );
}

int32_t I2CExpander::read_pin(uint8_t addr, uint8_t pin)
{
	uint32_t n = 0;
	bool should_off = false;
	if(!i2c_is_on) {
		i2c_on();
		should_off = true;
	}
		this->begin(addr - MCP23017_ADDRESS);
		this->pinMode(pin, OUTPUT);
		this->digitalWrite(pin, HIGH);
		delay(g_cfg.gcfg.sensor_init_time);
// 		DDRE  &= ~(1<<4);
//  		PORTE |= (1<<4);
		pinMode(4, INPUT);
		digitalWrite(4, HIGH);
// pinMode(2, OUTPUT);
// PORTE |= (1<<2);
// delay(1000);
// PORTE = PINE ^ (1<<2);
// delay(1000);

		cli();
			ticks = 0;
			EICRB &= 0xFC;
			EICRB |= 2;
			EIMSK |= (1<<4);
		sei();

		delay(g_cfg.gcfg.sensor_read_time);

		cli();
			n = ticks;
			EIMSK &= ~(1<<4);
		sei();
		this->digitalWrite(pin, LOW);
	if (should_off) {
		i2c_off();
	}
	return n;
}

bool I2CExpander::calibrate_pin(uint8_t addr, uint8_t pin, uint8_t measures)
{
	int32_t vals[ measures ];
	bool should_off = false;
	if (!i2c_is_on) {
		this->i2c_on();
		should_off = true;
	}
// 		if (mode) {
// 		Serial1.print("dev ");
// 		Serial1.print(addr, DEC);
// 		Serial1.print(" pin ");
// 		Serial1.println(pin, DEC);
// 		}
	int8_t i;
		for (i = 0; i < measures; ++i) {
			vals[i] = this->read_pin(addr, pin);
			if(vals[i] < 10) {
// 				Serial1.print("              PIN ");
// 				Serial1.print(pin, DEC);
// 				Serial1.println(" IS DEAD");
				if (should_off) {
					this->i2c_off();
				}
				return false;
			}
// 				Serial1.println(vals[i], DEC);
			delay(g_cfg.gcfg.sensor_init_time);
		}
	if (should_off) {
		this->i2c_off();
	}
	bool nc = true;
	int32_t t;
	for (i = 0; i < measures; ++i) {
		nc = true;
		for (int8_t j = 1; j < measures; ++j) {
			if (vals[j-1] > vals[j]) {
				t = vals[ j-1 ];
				vals[ j-1 ] = vals[ j ];
				vals[  j  ] = t;
				nc = false;
			}
		}//for j
		if (nc) break;
	}//for i
// 	char buf[16] = {0};
// 	for (i = 0; i < measures; ++i) {
// 		Serial1.print(vals[i], DEC);
// 		Serial1.print(" ");
// 	}
// 	Serial1.println();
	int32_t noise = abs(vals[ measures > 3 ? 1 : 0 ] - vals[ (measures > 3) ? (measures - 2) : (measures - 1) ]) >> 1;
// 	Serial1.print("\nnoise=");
// 	Serial1.println(noise, DEC);
	if (mode > 0) {
		bool found = false;
		potConfig pot;
		uint8_t pi = 0;
		for (; pi < g_cfg.gcfg.pots_count; ++pi) {
			pot = g_cfg.readPot(pi);
			if(pot.sensor.dev_addr == addr && pot.sensor.pin == pin) {
				found = true;
				break;
			}
		}//for pi
		if (!found) {
			Serial1.println("sensor not found in pots");
			return false;
		}
		pot.sensor.noise_delta = max(noise, pot.sensor.noise_delta);
		if (mode == 1) {
// 			pot.sensor.no_soil_freq =  vals[ measures>>1 ];
		} else if(mode == 2) {
// 			pot.sensor.dry_freq = vals[ measures>>1 ];
		} else if(mode==3) {
// 			pot.sensor.wet_freq = vals[ measures>>1 ];
		}
		g_cfg.savePot(pi, pot);
	}//if mode
	return true;
}

void I2CExpander::calibrate_dev(uint8_t addr, uint8_t measures_count)
{
	bool should_off = false;
	if (!i2c_is_on) {
		this->i2c_on();
		should_off = true;
	}
		for (uint8_t i = 0; i < 16; ++ i) {
// 			Serial1.print("calibrate pin #");
// 			Serial1.println(i, DEC);
			this->calibrate_pin(addr, i, measures_count);
		}
	if (should_off) {
		this->i2c_off();
	}
}

bool I2CExpander::ping(bool print_out)
{
	uint8_t dummy;
	uint8_t rc = twi_writeTo(MCP23017_ADDRESS | i2caddr, &dummy, 0, 1, 0);
	if (print_out) {
		Serial1.print("expander ");
		Serial1.print(i2caddr, DEC);
	}
	if (rc) {
		if (print_out) {
			Serial1.println("is unavail");
		}
		return false;
	} else {
		if (print_out) {
			Serial1.println(" is OK");
		}
		return true;
	}
}

// static volatile uint8_t state = 0;

ISR(INT4_vect)
{
  ++ticks;
//   state = 1 - state;
//   if(state) {
// 	  PORTE |=4;
//   } else {
// 	  PORTE = PINE & (~(2));
//   }
// 	PORTE = PINE ^ (1<<2);
}
