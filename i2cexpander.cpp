
#include "Arduino.h"
#include <Wire.h>
#include <AT24Cxxx.h>
#include "config_defines.h"
#include "configuration.h"

#define PCF_PIN_A		7
#define PCF_PIN_B		3
#define PCF_PIN_C		1
#define PCF_PIN_D		0
#define PCF_PIN_INH		2	//should be default HIGH 


#include "i2cexpander.h"
extern "C" {
	#include "utility/twi.h"  // from Wire library, so we can do bus scanning
}

static volatile uint32_t ticks = 0;

extern Configuration g_cfg;

I2CExpander::I2CExpander()
{
	i2c_is_on = false;
// 	mode = 0;
}

I2CExpander::~I2CExpander(){

}

// uint8_t I2CExpander::get_mode()
// {
// 	return this->mode;
// }

// void I2CExpander::set_mode(uint8_t m)
// {
// 	this->mode = m;
// 	Serial1.print("mode=");
// 	Serial1.println(m);
// }

bool I2CExpander::i2c_on()
{
	if (this->i2c_is_on) return true;
	DDRE |= (1<<I2C_EXPANDERS_EN_PIN);
	I2C_EXPANDERS_EN_PORT |= (1<<I2C_EXPANDERS_EN_PIN);
// 	Serial1.print("on. wait ");
// 	Serial1.println(g_cfg.config.i2c_pwron_timeout, DEC);
	delay(g_cfg.config.i2c_pwron_timeout);
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


int16_t I2CExpander::read_pin(uint8_t addr, uint8_t pin)
{
	uint32_t n = 0;
	bool should_off = false;
	if(!i2c_is_on) {
		i2c_on();
		should_off = true;
	}
		this->begin(addr);
		delay(g_cfg.config.sensor_init_time);
		this->write8(1<<PCF_PIN_INH);
		this->write(PCF_PIN_INH, HIGH);
		this->write(PCF_PIN_A, pin & 1);
		this->write(PCF_PIN_B, pin & 2);
		this->write(PCF_PIN_C, pin & 4);
		this->write(PCF_PIN_D, pin & 8);
		this->write(PCF_PIN_INH, LOW);
		
		for (uint8_t i=0; i < g_cfg.config.sensor_measures; ++i) {
			if (g_cfg.config.sensor_read_time > 2) {
				delay(g_cfg.config.sensor_read_time);
			}
			n += analogRead(AIN_PIN);
		}
		n /= g_cfg.config.sensor_measures;
		
		this->write(PCF_PIN_INH, HIGH);
	if (should_off) {
		i2c_off();
	}
	return n;
}

bool I2CExpander::readSensorValues(sensorValues*data)
{
	uint32_t n = 0;
	bool should_off = false;
	if(!i2c_is_on) {
		i2c_on();
		should_off = true;
	}

	this->begin(data->address);
	
// 	if (!ping(false)) {
// 		return false;
// 	}
	
	delay(g_cfg.config.sensor_init_time);
// 	Serial1.print(F("read dev #"));
// 	Serial1.println(data->address, DEC);
	
	for (uint8_t pin = 0; pin < 16; ++pin) {
		this->write8(1<<PCF_PIN_INH);
		this->write(PCF_PIN_INH, HIGH);
		this->write(PCF_PIN_A, pin & 1);
		this->write(PCF_PIN_B, pin & 2);
		this->write(PCF_PIN_C, pin & 4);
		this->write(PCF_PIN_D, pin & 8);
		this->write(PCF_PIN_INH, LOW);
		data->pin_values[pin] = 0;
		for(uint8_t i = 0; i < g_cfg.config.sensor_measures; ++i) {
			delay(g_cfg.config.sensor_read_time);
			data->pin_values[pin] += analogRead(AIN_PIN);
		}
		data->pin_values[pin] /= g_cfg.config.sensor_measures;
//  		Serial1.print(data->address, DEC);
//  		Serial1.print(F("/"));
//  		Serial1.print(pin, DEC);
//  		Serial1.print(F(":"));
//  		Serial1.println(data->pin_values[pin], DEC);
		this->write(PCF_PIN_INH, HIGH);
	}
	if (should_off) {
		i2c_off();
	}
	return true;
}

bool I2CExpander::ping(bool print_out)
{
	uint8_t dummy;
	uint8_t rc = twi_writeTo(_address, &dummy, 0, 1, 0);
	if (print_out) {
		Serial1.print(F("expander "));
		Serial1.print(_address, DEC);
	}
	if (rc) {
		if (print_out) {
			Serial1.println(F("is unavail"));
		}
		return false;
	} else {
		if (print_out) {
			Serial1.println(F(" is OK"));
		}
		return true;
	}
}
