#include <Wire.h>
#include <RTClib.h>

#include <MCP23017.h>
// #include <ShiftOut.h>

#include "config_defines.h"
#include "awtypes.h"
#include "configuration.h"
#include "waterdosersystem.h"
#include "i2cexpander.h"

#include "wateringcontroller.h"
// #include "mmc.h"
// #include "stat.h"
// #include "errlogger.h"

extern RTC_DS1307 clock;
extern Configuration g_cfg;
// extern ShiftOut hc595;
extern WaterDoserSystem water_doser;
extern I2CExpander i2cExpander;

// extern ErrLogger logger;

#define POTS_AT_ONCE	24

void WateringController::__sort(int8_t* dosers, int8_t* pots, int8_t sz)
{
	bool changed = false;
	int8_t tmp;
// 	Serial1.println("before sorting");
// 	for (int8_t i=0;i < sz; ++i) {
// 		Serial1.print(pots[i], DEC);
// 		Serial1.print("=>");
// 		Serial1.print(dosers[i], DEC);
// 		Serial1.print("; ");
// 	}
// 	Serial1.println();
	do {
		changed = false;
		for (int8_t i=0;i < sz-1; ++i) {
			for (int8_t j = i + 1; j < sz; ++j) {
				if (dosers[i] > dosers[j]) {
					tmp = dosers[i];
					dosers[i] = dosers[j];
					dosers[j] = tmp;
					tmp = pots[i];
					pots[i] = pots[j];
					pots[j] = tmp;
					changed = true;
				}
			}
		}
	} while (changed);
// 	Serial1.println("after sorting");
// 	for (int8_t i=0;i < sz; ++i) {
// 		Serial1.print(pots[i], DEC);
// 		Serial1.print("=>");
// 		Serial1.print(dosers[i], DEC);
// 		Serial1.print("; ");
// 	}
// 	Serial1.println();
}


WateringController::WateringController(I2CExpander* _exp) {
	init(_exp);
	log = &Serial1;
	memset(this->pot_states, 0, sizeof(this->pot_states));
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
	i2cexp->i2c_on();
	memset(pot_states, 0, sizeof(pot_states));
// 	for (int i = DS1307_POT_STATE_ADDRESS_BEGIN; i < DS1307_POT_STATE_ADDRESS_END; ++i) {
// 		clock.writeRAMbyte(i, 0);
// 	}//for i

	int watering_plants = 0;
	water_doser.prepareWatering();
	for (int i = 0; i < g_cfg.config.pots_count; ++i) {
		int8_t ret = check_pot_state(i, true);
		if (ret < 0) continue;
			watering_plants += ret;
	}//for i
	i2cexp->i2c_off();
	return watering_plants;
}

void WateringController::doPotService(bool check_and_watering)
{
	uint8_t sw = run_checks();
	if (log) {
		log->print("sw = ");
		log->println(sw, DEC);
	}
	if (sw > 0 && check_and_watering) {
		delay(5000);
		log->print("\b");
		run_watering();
	}
	log->println("watering end");
}

int8_t WateringController::check_pot_state(int8_t index, bool save_result)
{
	bool should_off = false;
	if (!i2cexp->i2c_on()) {
		should_off = true;
	}

	potConfig pot = g_cfg.readPot(index);
	if (log) {
		log->println(pot.name);
	}
	if ( 0 == pot.wc.enabled ) {
		if (save_result) {
			if (log) {
				log->println(" is off");
			}
			return -1;
		} else {
			return 0;
		}
	}

	uint16_t val = i2cexp->read_pin(pot.sensor.dev_addr, pot.sensor.pin);
	bool should_water = check_watering_program(index, pot, val);

// 	if (log) {
// // 		log->print("sensor:");
// 		log->print(val, DEC);
// 		log->print(" ");
// 		log->println(should_water, DEC);
// 	}

	if (/*should_water && */save_result) {

// 		Stat stat;
// 		StatFile *file = stat.open(index);
// 		stat.saveSensorValue(file, val);
// 		stat.close(file);
		if (should_water) {
			pot_states[index / 8] |= (1 << (index % 8) );
		} else {
			pot_states[index / 8] &= ~(1 << (index % 8) );
		}
// 		uint8_t ramval = clock.readRAMbyte(DS1307_POT_STATE_ADDRESS_BEGIN + index / 8);
// 		ramval |= ( 1 << (index % 8) );
// 		clock.writeRAMbyte(DS1307_POT_STATE_ADDRESS_BEGIN + index / 8, ramval);
	}

	if(should_off) {
		i2cexp->i2c_off();
	}
	return should_water ? 1 : 0;
}

bool WateringController::check_watering_program(uint8_t pot_index, potConfig& pot, uint32_t cur_value)
{
	uint8_t pgm = pot.wc.pgm_id;
	int32_t should_water = 0;
	wateringProgram wpdata;
	read_watering_program(pot_index, wpdata);
// 	Serial1.print("watering program: ");
// 	Serial1.println(pgm, DEC);
	if (pot.sensor.noise_delta < 1) {
		pot.sensor.noise_delta = round(cur_value * 0.05);//default value.
	}
	if (log) {
// 		log->print("program: ");
		log->print(pgm, DEC);
		log->print(" ");
		log->print(cur_value, DEC);
		log->print(" ");
	}
	if (pgm == 1) {
// 		if (pot.sensor.no_soil_freq > 0 &&  (abs(cur_value -  pot.sensor.no_soil_freq) < pot.sensor.noise_delta) ) {
// // 			Serial1.print("pot ");
// // 			Serial1.print(pot_index, DEC);
// // 			Serial1.println(" sensor is out of soil");
// 			return false;
// 		}
		/*влажность равна нужной в пределах погрешности*/
		if (log) {
			log->print(wpdata.prog.const_hum.value, DEC);
			log->print(" ");
		}
 		if ( abs(cur_value - wpdata.prog.const_hum.value) <= pot.sensor.noise_delta) {
			if (log) {
				log->println("wet enough");
			}
 			return false;
 		}
// 		/**
// 		частота больше -- влажность меньше.
// 		частота меньше -влажность больше
// 		*/
 		should_water = (cur_value - wpdata.prog.const_hum.value);
		if (log) {
// 			log->print("pgm val:");
// 			log->print(wpdata.prog.const_hum.value, DEC);
			log->print(should_water, DEC);
 			log->print(" ");
			log->println(should_water > 0, DEC);
		}
 		return (should_water > 0);//мокрее нужного -- ничего не делаем.
	} else if (pgm == 2) {
		if( abs(cur_value - wpdata.prog.hum_and_dry.min_value)<= pot.sensor.noise_delta || abs(cur_value - wpdata.prog.hum_and_dry.max_value) <= pot.sensor.noise_delta) {
			return false;
		}

		if ( pot.wc.state == 1/*is_pot_drying(pot)*/) {
			if ( (cur_value - wpdata.prog.hum_and_dry.min_value) > pot.sensor.noise_delta) {
				pot.wc.state = 0;
// 				set_pot_watering(pot, true);
				g_cfg.savePot(pot_index, pot);
				return true;
			} else {
				return true;
			}
		} else {
			if ( cur_value - wpdata.prog.hum_and_dry.max_value <=0) {
// 				set_pot_watering(pot, false);
				pot.wc.state = 1;
				g_cfg.savePot(pot_index, pot);
				return false;
			} else {
				return true;
			}
		}
	}//pgm == 2
	return false;
}

void WateringController::read_watering_program(uint8_t pot_index, wateringProgram& wpgm)
{
	g_cfg.read_watering_program(pot_index, wpgm);
}

void WateringController::write_watering_program(uint8_t pot_index, wateringProgram& wpgm)
{
	g_cfg.write_watering_program(pot_index, wpgm);
}

void WateringController::__processData(int8_t* pots, int8_t sz)
{
	int8_t water_pots[ POTS_AT_ONCE ] = {-1};
	potConfig pc;
//  	Serial1.println("__processData");
//! 	water_doser.rewind(-1);
//  	Serial1.println("__processData 2");
	//16 горшков превратить в 16 чашек +
	//сортировка по возрастанию. +
	// полить каждую.
	//сбросить массив в изначальное состояние.
	for (int index = 0; index < sz; ++index) {
		if (pots[ index ] < 0) continue;
		pc = g_cfg.readPot(pots[ index ]);
//  		Serial1.print(index, DEC);
//  		Serial1.print(": ");
//  		Serial1.println(pc.wc.y, DEC);
		water_pots[ index ] = (pc.wc.y << 4) | (pc.wc.x);
	}

	this->__sort(water_pots, pots, sz);

// 	Stat stat;

	for (int index = 0; index < sz; ++index) {
		if (pots[ index ] < 0) continue;
//  		Serial1.print("water to pot ");
//  		Serial1.print(pots[ index ], DEC);
		pc = g_cfg.readPot(pots[ index ]);
		if (log) {
// 			log->print("watering ");
			log->println(pc.name);
			log->print(pc.wc.x, DEC);
			log->print(" ");
			log->print(pc.wc.y, DEC);
			log->print(" ");
			log->println(pc.wc.ml, DEC);
		}
 		Serial1.println(pc.name);
		water_doser.init_measure();
		Serial1.print(pc.wc.x, DEC);
		Serial1.print(" ");
		Serial1.print(pc.wc.y, DEC);
		Serial1.print(" ");
		Serial1.println(pc.wc.ml, DEC);

// 		if (water_doser.moveToPos(pc.wc.x, pc.wc.y)) {
			uint16_t ml = water_doser.pipi(pc.wc.x, pc.wc.y, pc.wc.ml);
			if (log) {
				Serial1.print("watered: ");
				Serial1.println(ml, DEC);

				log->print("watered: ");
				log->println(ml, DEC);
			}
			pc.wc.watered += ml;
			g_cfg.savePot(pots[ index ], pc);

// 			StatFile*file = stat.open(pots[ index ]);
// 			stat.saveMlValue(file, ml);
// 			stat.close(file);

// 			g_cfg.day_stat_save_ml(pots[ index ], ml);

// 		} else {
// 			Serial1.println("ERROR: can not move to bowl");
// 		}
	}
}//sub


void WateringController::run_watering()
{
	uint8_t data = 0;
	int8_t pots[ POTS_AT_ONCE ] = {-1}, di = 0;
	water_doser.prepareWatering();
	for(int i = 0; i < 10; ++i) {
		data = pot_states[i];
// 		Serial1.print("read byte ");
// 		Serial1.print(i, DEC);
// 		Serial1.print(" ");
// 		Serial1.println(data, DEC);
		int j = 0;
		while (data) {
			if (data & 1) {
// 				Serial1.print("plant #");
// 				Serial1.println( (i - DS1307_POT_STATE_ADDRESS_BEGIN) * 8 + j, DEC);
				pots[ di++ ] = i * 8 + j;
				if (POTS_AT_ONCE == di) {
// 					Serial1.println("data is full.watering");
					__processData(pots, di);
					memset(pots, -1, sizeof(pots));
					di = 0;
				}//if POTS_AT_ONCE filled
			}//if needs watering
			data >>= 1;
			++j;
		}
	}
//  	Serial1.print("di at end:");
//  	Serial1.println(di, DEC);
	if (di) {
		__processData(pots, di);
	}
}

void WateringController::midnightTasks()
{
	potConfig pc;
// 	wateringProgram wp;
	for (int i = 0; i < g_cfg.config.pots_count; ++i) {
		pc = g_cfg.readPot(i);
		uint8_t pgm = pc.wc.pgm_id;
		if (pgm == 1 || pgm == 2) {
			pc.wc.watered = 0;
			g_cfg.savePot(i, pc);
		}
	}
}

void WateringController::setLog(Print*_log)
{
	log = _log;
}
