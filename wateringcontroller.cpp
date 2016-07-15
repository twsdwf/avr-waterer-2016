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

uint8_t WateringController::getStatDay()
{
	return EEPROM.read(EEPROM_DAYINDEX_ADDR);
}

void WateringController::setStatDay(uint8_t day)
{
	EEPROM.write(EEPROM_DAYINDEX_ADDR, day);
}

void WateringController::writeDayML(uint8_t index, uint16_t val)
{
	EEPROM.write(index * 2, val & 0xFF);
	EEPROM.write(index * 2 + 1, val>>8);
}
void WateringController::incDayML(uint8_t index, uint16_t inc)
{
	writeDayML(index, readDayML(index)+inc);
}

WateringController::WateringController(I2CExpander* _exp) {
// 	init(_exp);
// 	_sensor_values=NULL;
}

WateringController::~WateringController()
{
// 	if (_sensor_values) delete [] (_sensor_values);
}

void WateringController::init(I2CExpander* _exp)
{
	i2cexp = _exp;
// 	_sensor_values = NULL;
		uint8_t bits = 0, bitsa=0, cp=0;
		sv_count = 0;

		// search how many expander device we have. var bits used for repeat tests.
		for (int i = 0; i < g_cfg.config.pots_count; ++i) {
			potConfig pot = g_cfg.readPot(i);
			if (pot.sensor.dev >=32 && pot.sensor.dev <=39) {
				if ( !(bits & 1<<(pot.sensor.dev - 32))) {
					bits |= 1<<(pot.sensor.dev - 32);
					++sv_count;
				}
			} else if (pot.sensor.dev >=56 && pot.sensor.dev <=56+7) {
				if ( !(bitsa & 1<<(pot.sensor.dev - 56))) {
					bitsa |= 1<<(pot.sensor.dev - 56);
					++sv_count;
				}
			}
		}
   		Serial1.print(F("devices:"));
   		Serial1.println(sv_count, DEC);
		_sensor_values = (sensorValues*)malloc(sizeof(sensorValues)*sv_count);
		if (_sensor_values == 0) {
			Serial1.println(F("FATAL ERROR: malloc() failed;"));
			return;
		}

		memset(_sensor_values, 0, sizeof(sensorValues)*sv_count);

		uint8_t index = 0;
		
		for (uint8_t i = 0; i < 8; ++i) {
			if (index < sv_count && _sensor_values[index].address!=0) {
				Serial1.println(F("ERROR: possible stack roof broken!;"));
			}
			if (bits & (1<<i)) {
				_sensor_values[ index ].address = 32 + i;
				++index;
			}
			if (bitsa & (1<<i)) {
				_sensor_values[ index ].address = 56 + i;
				++index;
			}
		}
}

void WateringController::dumpSensorValues(HardwareSerial*output)
{
	i2cexp->i2c_on();
	Serial1.print(" addr=");
	Serial1.print((int)_sensor_values, DEC);

	for (uint8_t i = 0; i < sv_count; ++i) {
		Serial1.print("addr:");
		Serial1.println(_sensor_values[i].address, DEC);

		if (!i2cexp->readSensorValues( &_sensor_values[i]) ) {
			Serial1.print(F("ERROR while read sensors at address "));
			Serial1.print((_sensor_values+i)->address, DEC);
			Serial1.println(';');
		}
	}
	i2cexp->i2c_off();
	for (uint8_t i = 0; i < sv_count; ++i) {
		output->print(_sensor_values[i].address, DEC);
 		output->print(':');
		for(uint8_t j=0;j<16;++j) {
			output->print(_sensor_values[i].pin_values[j], DEC);
			if (j < 15) {
				output->print(',');
			}
		}
		output->print(';');
		output->flush();
	}

 	output->print(';');
}

int WateringController::run_checks(HardwareSerial* output)
{
// 	Serial1.println("run checks;");
	clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_READ);
 	for (int i = RAM_POT_STATE_ADDRESS_BEGIN; i < RAM_POT_STATE_ADDRESS_END; ++i) {
 		clock.writeRAMbyte(i, 0);
 	}//for i

	i2cexp->i2c_on();
	Serial1.print(" addr=");
	Serial1.print((int)_sensor_values, DEC);
	
	for (uint8_t i = 0; i < sv_count; ++i) {
		Serial1.print("addr:");
		Serial1.println(_sensor_values[i].address, DEC);
		
		if (!i2cexp->readSensorValues( &_sensor_values[i]) ) {
			output->print(F("ERROR while read sensors at address "));
			output->print((_sensor_values+i)->address, DEC);
			output->println(';');
		}
	}
	i2cexp->i2c_off();
	int watering_plants = 0;
// 	water_doser.prepareWatering();
	for (int i = 0; i < g_cfg.config.pots_count; ++i) {
		int8_t ret = check_pot_state(i, true, output);
		if (ret <= 0) continue;
		watering_plants += ret;
	}//for i
	return watering_plants;
}

void WateringController::doPotService(bool check_and_watering, HardwareSerial*output)
{
	uint8_t state = clock.readRAMbyte(RAM_CUR_STATE);
	uint8_t sw;
	DateTime now = clock.now();
// 	Serial1.print("state=");
// 	Serial1.print(state, DEC);
// 	Serial1.println(';');
	
	if (state == CUR_STATE_IDLE) {
		sw = run_checks(output);
// 		Serial1.print("sw = ");
// 		Serial1.print(sw, DEC);
// 		Serial1.println(';');
	}

	run_watering(check_and_watering, output);
	
	DateTime now2 = clock.now();
 	output->print(F("watering time: "));
 	output->print(now2.secondstime() - now.secondstime(), DEC);
 	output->println(F("s;"));
	last_check_time = now2.secondstime();
	clock.writeRAMbyte(LAST_CHECK_TS_1, last_check_time >> 24);
	clock.writeRAMbyte(LAST_CHECK_TS_2, last_check_time >> 16);
	clock.writeRAMbyte(LAST_CHECK_TS_3, last_check_time >> 8);
	clock.writeRAMbyte(LAST_CHECK_TS_4, last_check_time & 0xFF);
	clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
 	output->println(F("watering end;"));
}

int8_t WateringController::check_pot_state(int8_t index, bool save_result, HardwareSerial* output)
{
	potConfig pot = g_cfg.readPot(index);
 	output->print(F("checking pot "));
 	output->print(index, DEC);
 	output->print(F(" "));
 	output->print(pot.name);
	
	if ( 0 == pot.wc.enabled ) {
		output->println(F(" is off;"));
		return -1;
	}

	int16_t cur_value = -1;
	for (uint8_t i = 0; i < sv_count; ++i) {
		Serial1.print(i);
		Serial1.print(' ');
		Serial1.print(_sensor_values[i].address, DEC);
		Serial1.print('/');
		Serial1.println(pot.sensor.dev, DEC);
		if ( _sensor_values[i].address == pot.sensor.dev) {
			cur_value = _sensor_values[i].pin_values[ pot.sensor.pin ];
			break;
		}
	}

	bool should_water = false;
	
	if (cur_value == -1) {
		output->print(F("ERROR: dev "));
		output->print(pot.sensor.dev, DEC);
		output->print(F("/"));
		output->print(pot.sensor.pin, DEC);
		output->println(F("is not found in data!;"));
		return -1;
	}
	pot.sensor.noise_delta = 10;
	output->print(F(" program: "));
	output->print(pot.wc.pgm, DEC);
	if (pot.wc.pgm == 1) {
		if (pot.pgm.const_hum.max_ml <= readDayML(index)) {
			output->println(F("day limit is out!"));
		} else {
			output->print(F(" barrier value:"));
			output->print(pot.pgm.const_hum.value, DEC);
			output->print(F(" cur value:"));
			output->print(cur_value, DEC);
			if ( abs(cur_value - pot.pgm.const_hum.value) <= pot.sensor.noise_delta) {
				output->println(F(" wet enough;"));
				should_water = false;
			} else {
				should_water = (cur_value < pot.pgm.const_hum.value);
				output->print(F(" should_water:"));
				output->print(should_water, DEC);
				output->println(';');
			}
		}
//  		return should_water;
	} else if (pot.wc.pgm == 2) {
		output->print(F(" range "));
		output->print(pot.pgm.hum_and_dry.min_value, DEC);
		output->print(F(".."));
		output->print(pot.pgm.hum_and_dry.max_value, DEC);
		output->print(F(" cur_value:"));
		output->print(cur_value, DEC);
		if (pot.pgm.hum_and_dry.max_ml <= readDayML(index)) {
			output->println(F("day limit is out!"));
		} else {
			if( abs(cur_value - pot.pgm.hum_and_dry.min_value)<= pot.sensor.noise_delta || abs(cur_value - pot.pgm.hum_and_dry.max_value) <= pot.sensor.noise_delta) {
				output->print(F(" out of range, no actions"));
				should_water = false;
			}

			if ( pot.wc.state == 1) {
				output->print(F(" drying "));
				if ( cur_value < pot.pgm.hum_and_dry.min_value +  pot.sensor.noise_delta) {
					pot.wc.state = 0;
					g_cfg.savePot(index, pot);
					output->println(F(" end;"));
					should_water = true;
				} else {
					output->println(F(" again;"));
					should_water = false;
				}
			} else {
				output->print(F(" wetting "));
				if ( cur_value > pot.pgm.hum_and_dry.max_value - pot.sensor.noise_delta) {
					output->println(F(" wet enough. start drying;"));
					pot.wc.state = 1;
					g_cfg.savePot(index, pot);
					should_water = false;
				} else {
					output->println(F(" again;"));
					should_water = true;
				}
			}
		}//if day limit is not reached
	} else {//pgm == 2
		output->println(F("not implemented pgm;"));
	}

	output->print(F("val="));
	output->print(cur_value, DEC);
	output->print(F(" "));
	output->print(should_water, DEC);
	output->println(';');

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
float distance(const coords& a, const coords& b)
{
	return sqrt((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y));
}

void WateringController::run_watering(bool real, HardwareSerial* output)
{
	uint8_t data = 0;
	clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_WATER);
// 	water_doser.prepareWatering();
	output->print("real watering:");
	output->println(real, DEC);
	uint8_t i = 0, ip = 0;
	
	coords pts[32];
	for (uint8_t addr = RAM_POT_STATE_ADDRESS_BEGIN; addr < RAM_POT_STATE_ADDRESS_END; ++addr, ++i) {
		data = clock.readRAMbyte(addr);
		int j = 0;
		for (j = 0; j < 8; ++j) {
			if (data & (1<<j)) {
				potConfig pc = g_cfg.readPot( i * 8 + j);
				if (pc.wc.enabled == 0) {
					continue;
				}
				pts[ip].x = pc.wc.x;
				pts[ip].y = pc.wc.y;
				pts[ip].index = i * 8 + j;
				Serial1.print("[");
				Serial1.print(pts[ip].index, DEC);
				Serial1.print(":");
				Serial1.print(pts[ip].x, DEC);
				Serial1.print(",");
				Serial1.print(pts[ip].y, DEC);
				Serial1.println("]");
				++ip;
			}
		}
	}
 	Serial1.print(F("pots to water:"));
 	Serial1.println(ip, DEC);
	
	if (ip <= 0) {
		return;
	}

	coords cur = water_doser.curPos();
	int cd = 1000,ci=-1;
// 	uint32_t start = millis();
			Serial1.print(F("cur ("));
			Serial1.print(cur.index, DEC);
			Serial1.print(F(","));
			Serial1.print(cur.x, DEC);
			Serial1.print(F(","));
			Serial1.print(cur.y, DEC);
			Serial1.print(F(")"));
	do {
		cd = 1000;
		ci = -1;
		float d;
		for (int i=0; i < ip; ++i) {
			d = distance(pts[i], cur);
// 			Serial1.print(F("distance btw ("));
// 			Serial1.print(cur.x, DEC);
// 			Serial1.print(F(","));
// 			Serial1.print(cur.y, DEC);
// 			Serial1.print(F(") and ("));
// 			Serial1.print(pts[i].x, DEC);
// 			Serial1.print(F(","));
// 			Serial1.print(pts[i].y, DEC);
// 			Serial1.print(F(") = "));
// 			Serial1.println(d);
			if ( d < cd) {
				cd = d;
				ci = i;
			}
		}
 		Serial1.print(F("ci="));
 		Serial1.println(ci, DEC);
		if ( ci > -1) {
			Serial1.print(" (");
			Serial1.print(pts[ci].index);
			Serial1.print(":");
			Serial1.print(pts[ci].x);
			Serial1.print(",");
			Serial1.print(pts[ci].y);
			Serial1.print(") ");
			potConfig pc = g_cfg.readPot(pts[ci].index);
			
			if (pc.wc.enabled) {
// 				continue;
				output->print("watering to ");
				output->print(pc.name);
				output->print(' ');
				output->print(pts[ci].x, DEC);
				output->print(',');
				output->print(pts[ci].y, DEC);
				output->println(';');
				if (real) {
					uint16_t ml = water_doser.pipi(pts[ci].x, pts[ci].y, pc.wc.ml);
					Serial1.println(F("pipi is OK, saving"));
					incDayML(pts[ci].index, ml);
				} else {
 					if(water_doser.moveToPos(pts[ci].x, pts[ci].y)) {
						water_doser.servoDown();
						water_doser.servoUp();
					} else {
						Serial1.println(F("ERROR: move fault"));
					}
				}
			}
// 			data = clock.readRAMbyte(RAM_POT_STATE_ADDRESS_BEGIN + (pts[ci].index/8));
// 			data &= ~(1<<(pts[ci].index % 8));
// 			clock.writeRAMbyte(RAM_POT_STATE_ADDRESS_BEGIN + (pts[ci].index/8), data);
			Serial1.print("points {");
			for (uint8_t p = 0;p < ip;++p) {
				Serial1.print(" (");
				Serial1.print(pts[p].index);
				Serial1.print(":");
				Serial1.print(pts[p].x);
				Serial1.print(",");
				Serial1.print(pts[p].y);
				Serial1.print(") ");
			}
			Serial1.println("}");
			cur = pts[ ci ];
			Serial1.print("new cur (");
			Serial1.print(cur.index);
			Serial1.print(":");
			Serial1.print(cur.x);
			Serial1.print(",");
			Serial1.print(cur.y);
			Serial1.println(") ");
			Serial1.print(F("ip="));
			Serial1.println(ip, DEC);
			if (ip > 1) {
				Serial1.print("move pt (");
				Serial1.print(pts[ip-1].index);
				Serial1.print(":");
				Serial1.print(pts[ip-1].x);
				Serial1.print(",");
				Serial1.print(pts[ip-1].y);
				Serial1.print(") ");
				pts[ ci ].x = pts[ip - 1].x;
				pts[ ci ].y = pts[ip - 1].y;
				pts[ ci ].index = pts[ip - 1].index;
				Serial1.print("new ci pt (");
				Serial1.print(pts[ci].index);
				Serial1.print(":");
				Serial1.print(pts[ci].x);
				Serial1.print(",");
				Serial1.print(pts[ci].y);
				Serial1.print(")\r\n{");
			} else {
				Serial1.println(F("    ip=1"));
			}
			--ip;
			for (uint8_t p=0;p<ip;++p) {
				Serial1.print(" (");
				Serial1.print(pts[p].index);
				Serial1.print(":");
				Serial1.print(pts[p].x);
				Serial1.print(",");
				Serial1.print(pts[p].y);
				Serial1.print(") ");
			}
			Serial1.println("}");
		} else {
			Serial1.println(F("next pos not found"));
			Serial1.println(ip, DEC);
			break;
		}
	}  while (ip > 0);
 	Serial1.println(F("end of watering"));
// 	Serial1.println(millis()-start);
	
// 	qsort(pts, ip, sizeof(uint8_t), ptsCmp);
/*
	for (uint8_t addr = RAM_POT_STATE_ADDRESS_BEGIN; addr < RAM_POT_STATE_ADDRESS_END; ++addr) {
		data = clock.readRAMbyte(addr);
 		output->print("read byte ");
 		output->print(addr, DEC);
 		output->print(" ");
 		output->print(data, BIN);
		output->println(';');
		int j = 0;
		for (j = 0; j < 8; ++j) {
			if (data & (1<<j)) {
				potConfig pc = g_cfg.readPot( i * 8 + j);
				if (pc.wc.enabled == 0) {
					continue;
				}
 				output->print("watering to ");
				output->print(pc.name);
				output->print(' ');
 				output->print(pc.wc.x, DEC);
 				output->print(',');
 				output->print(pc.wc.y, DEC);
 				output->println(';');
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
	*/
}

void WateringController::cleanDayStat()
{
	for (int i = 0; i < g_cfg.config.pots_count; ++i) {
		writeDayML(i, 0);
	}
}

void WateringController::printDayStat(HardwareSerial* output)
{
	for (int i = 0; i < g_cfg.config.pots_count; ++i) {
		potConfig pot = g_cfg.readPot(i);
		output->print(i, DEC);
		output->print(F(","));
		output->print(pot.name);
		output->print(F(","));
		output->print(readDayML(i), DEC);
		output->print(F(","));
		if (pot.wc.pgm == 1) {
			output->print(pot.pgm.const_hum.max_ml, DEC);
		} else if (pot.wc.pgm == 2) {
			output->print(pot.pgm.hum_and_dry.max_ml, DEC);
		}
		output->println(F(";"));
	}
}