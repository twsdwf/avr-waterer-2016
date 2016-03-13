#include <Wire.h>
#include <RTClib.h>

#include <MCP23017.h>
// #include <ShiftOut.h>

#include "config_defines.h"
#include "awtypes.h"
#include "configuration.h"
#include "waterdosersystem.h"
#include "i2cexpander.h"
#include<EEPROM.h>
#include "wateringcontroller.h"
// #include "mmc.h"
// #include "stat.h"
// #include "errlogger.h"

extern RTC_DS1307 clock;
extern Configuration g_cfg;
// extern ShiftOut hc595;
extern WaterDoserSystem water_doser;
extern I2CExpander i2cExpander;
extern uint32_t last_check_time;

// extern ErrLogger logger;

#define POTS_AT_ONCE	24
uint16_t WateringController::readDayML(uint8_t index)
{
	return EEPROM.read(index * 2) | (EEPROM.read(index*2+1)<<8);
}

void WateringController::writeDayML(uint8_t index, uint16_t val)
{
	EEPROM.write(index*2, val &0xFF);
	EEPROM.write(index*2+1, val>>8);
}
void WateringController::incDayML(uint8_t index, uint16_t inc)
{
	writeDayML(index, readDayML(index)+inc);
}

WateringController::WateringController(I2CExpander* _exp) {
	init(_exp);
	sv=NULL;
}

WateringController::~WateringController()
{

}

void WateringController::init(I2CExpander* _exp)
{
	i2cexp = _exp;
}

int WateringController::run_checks()
{
// 	Serial1.println("run checks;");
	clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_READ);
 	for (int i = RAM_POT_STATE_ADDRESS_BEGIN; i < RAM_POT_STATE_ADDRESS_END; ++i) {
 		clock.writeRAMbyte(i, 0);
 	}//for i
 	if (sv == NULL) {
		uint8_t bits = 0, cp;
		sv_count = 0;
		/*
		 * search how many expander device we have. var bits used for repeat tests.
		 */
		for (int i = 0; i < g_cfg.config.pots_count; ++i) {
			potConfig pot = g_cfg.readPot(i);
			if ( !(bits & 1<<(pot.sensor.dev - 32))) {
				bits |= 1<<(pot.sensor.dev - 32);
				++sv_count;
			}
		}
// 		Serial1.print("devices:");
// 		Serial1.println(sv_count, DEC);
		sv = (sensorValues*)malloc(sizeof(sensorValues)*sv_count);
		memset(sv, 0, sizeof(sensorValues)*sv_count);
		for (uint8_t i = 0; i < 8; ++i) {
			if (bits & (1<<i)) {
				sv[cp++].address = 32 + i;
				Serial1.println(32 + i, DEC);
			}
		}
		sv_count = cp;
// 		Serial1.print("sv_count=");
// 		Serial1.println(sv_count, DEC);
	}

	i2cexp->i2c_on();
	
	for (uint8_t i = 0; i < sv_count; ++i) {
		if (!i2cexp->readSensorValues(sv + i)) {
			Serial1.print("ERROR while read sensors at address ");
			Serial1.print((sv+i)->address, DEC);
			Serial1.println(';');
		}
	}
	i2cexp->i2c_off();
	int watering_plants = 0;
// 	water_doser.prepareWatering();
	for (int i = 0; i < g_cfg.config.pots_count; ++i) {
		int8_t ret = check_pot_state(i, true);
		if (ret <= 0) continue;
		watering_plants += ret;
	}//for i
	return watering_plants;
}

void WateringController::doPotService(bool check_and_watering)
{
	uint8_t state = clock.readRAMbyte(RAM_CUR_STATE);
	uint8_t sw;
	DateTime now = clock.now();
// 	Serial1.print("state=");
// 	Serial1.print(state, DEC);
// 	Serial1.println(';');
	
	if (state == CUR_STATE_IDLE) {
		sw = run_checks();
// 		Serial1.print("sw = ");
// 		Serial1.print(sw, DEC);
// 		Serial1.println(';');
	}

	run_watering(check_and_watering);
	
	DateTime now2 = clock.now();
	Serial1.print("watering time: ");
	Serial1.print(now2.secondstime() - now.secondstime(), DEC);
	Serial1.println("s;");
	last_check_time = now2.secondstime();
	clock.writeRAMbyte(LAST_CHECK_TS_1, last_check_time >> 24);
	clock.writeRAMbyte(LAST_CHECK_TS_2, last_check_time >> 16);
	clock.writeRAMbyte(LAST_CHECK_TS_3, last_check_time >> 8);
	clock.writeRAMbyte(LAST_CHECK_TS_4, last_check_time & 0xFF);
	clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
	Serial1.println("watering end;");
}

int8_t WateringController::check_pot_state(int8_t index, bool save_result)
{
	potConfig pot = g_cfg.readPot(index);
	Serial1.print("checking pot ");
	Serial1.print(index, DEC);
	Serial1.print(' ');
	Serial1.print(pot.name);
	Serial1.println(';');
	
	if ( 0 == pot.wc.enabled ) {
		Serial1.println(" is off;");
		return -1;
	}

	int16_t cur_value = -1;
	for (uint8_t i = 0; i < sv_count; ++i) {
// 		Serial1.print(i);
// 		Serial1.print(' ');
// 		Serial1.print(sv[i].address, DEC);
// 		Serial1.print('/');
// 		Serial1.println(pot.sensor.dev, DEC);
		if ( sv[i].address == pot.sensor.dev) {
			cur_value = sv[i].pin_values[ pot.sensor.pin ];
			break;
		}
	}

	bool should_water = false;
	
	if (cur_value == -1) {
		Serial1.print("ERROR: dev ");
		Serial1.print(pot.sensor.dev, DEC);
		Serial1.print("/");
		Serial1.print(pot.sensor.pin, DEC);
		Serial1.println("is not found in data!;");
		return -1;
	}
	pot.sensor.noise_delta = 10;
	Serial1.print(" program: ");
	Serial1.print(pot.wc.pgm, DEC);
	if (pot.wc.pgm == 1) {
		if (pot.pgm.const_hum.max_ml <= readDayML(index)) {
			Serial1.println("day limit is out!");
		} else {
			Serial1.print(" barrier value:");
			Serial1.print(pot.pgm.const_hum.value, DEC);
			Serial1.print(" cur value:");
			Serial1.print(cur_value, DEC);
			if ( abs(cur_value - pot.pgm.const_hum.value) <= pot.sensor.noise_delta) {
				Serial1.println(" wet enough;");
				should_water = false;
			} else {
				should_water = (cur_value < pot.pgm.const_hum.value);
				Serial1.print(" should_water:");
				Serial1.print(should_water, DEC);
				Serial1.println(';');
			}
		}
//  		return should_water;
	} else if (pot.wc.pgm == 2) {
		Serial1.print(" range ");
		Serial1.print(pot.pgm.hum_and_dry.min_value, DEC);
		Serial1.print("..");
		Serial1.print(pot.pgm.hum_and_dry.max_value, DEC);
		Serial1.print(" cur_value:");
		Serial1.print(cur_value, DEC);
		if (pot.pgm.hum_and_dry.max_ml <= readDayML(index)) {
			Serial1.println("day limit is out!");
		} else {
			if( abs(cur_value - pot.pgm.hum_and_dry.min_value)<= pot.sensor.noise_delta || abs(cur_value - pot.pgm.hum_and_dry.max_value) <= pot.sensor.noise_delta) {
				Serial1.print(" out of range, no actions");
				should_water = false;
			}

			if ( pot.wc.state == 1) {
				Serial.print(" drying ");
				if ( cur_value < pot.pgm.hum_and_dry.min_value +  pot.sensor.noise_delta) {
					pot.wc.state = 0;
					g_cfg.savePot(index, pot);
					Serial1.println(" end;");
					should_water = true;
				} else {
					Serial1.println(" again;");
					should_water = false;
				}
			} else {
				Serial1.print(" wetting ");
				if ( cur_value > pot.pgm.hum_and_dry.max_value - pot.sensor.noise_delta) {
					Serial1.println(" wet enough. start drying;");
					pot.wc.state = 1;
					g_cfg.savePot(index, pot);
					should_water = false;
				} else {
					Serial1.println(" again;");
					should_water = true;
				}
			}
		}//if day limit is not reached
	} else {//pgm == 2
		Serial1.println("not implemented pgm;");
	}

	Serial1.print("val=");
	Serial1.print(cur_value, DEC);
	Serial1.print(" ");
	Serial1.print(should_water, DEC);
	Serial1.println(';');

	if (should_water) {
		uint8_t was = clock.readRAMbyte(RAM_POT_STATE_ADDRESS_BEGIN + index/8);
		was |= (1<<(index%8));
		clock.writeRAMbyte(RAM_POT_STATE_ADDRESS_BEGIN + index/8, was);
	}

	return should_water;
}

// void WateringController::read_watering_program(uint8_t pot_index, wateringProgram& wpgm)
// {
// 	g_cfg.read_watering_program(pot_index, wpgm);
// }
// 
// void WateringController::write_watering_program(uint8_t pot_index, wateringProgram& wpgm)
// {
// 	g_cfg.write_watering_program(pot_index, wpgm);
// }
// 
void WateringController::run_watering(bool real)
{
	uint8_t data = 0;
	clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_WATER);
// 	water_doser.prepareWatering();
	Serial1.print("real watering:");
	Serial1.println(real, DEC);
	uint8_t i = 0;
	for (uint8_t addr = RAM_POT_STATE_ADDRESS_BEGIN; addr < RAM_POT_STATE_ADDRESS_END; ++addr) {
		data = clock.readRAMbyte(addr);
 		Serial1.print("read byte ");
 		Serial1.print(addr, DEC);
 		Serial1.print(" ");
 		Serial1.print(data, BIN);
		Serial1.println(';');
		int j = 0;
		for (j = 0; j < 8; ++j) {
			if (data & (1<<j)) {
				potConfig pc = g_cfg.readPot( i * 8 + j);
 				Serial1.print("watering to ");
				Serial1.print(pc.name);
				Serial1.print(' ');
 				Serial1.print(pc.wc.x, DEC);
 				Serial1.print(',');
 				Serial1.print(pc.wc.y, DEC);
 				Serial1.println(';');
				if (real) {
					uint16_t ml = water_doser.pipi(pc.wc.x, pc.wc.y, pc.wc.ml);
					incDayML(i * 8 + j, ml);
				}
				data &= ~(1<<j);
				clock.writeRAMbyte(addr, data);
			}//if needs watering
		}
		++i;
	}
}

void WateringController::cleanDayStat()
{
	for (int i = 0; i < g_cfg.config.pots_count; ++i) {
		writeDayML(i, 0);
	}
}

void WateringController::printDayStat()
{
	for (int i = 0; i < g_cfg.config.pots_count; ++i) {
		potConfig pot = g_cfg.readPot(i);
		Serial1.print(pot.name);
		Serial1.print(":");
		Serial1.print(readDayML(i), DEC);
		Serial1.print("/");
		if (pot.wc.pgm == 1) {
			Serial1.print(pot.pgm.const_hum.max_ml, DEC);
		} else if (pot.wc.pgm == 2) {
			Serial1.print(pot.pgm.hum_and_dry.max_ml, DEC);
		}
		Serial1.println(";");
	}
}