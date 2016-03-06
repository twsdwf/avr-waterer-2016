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
extern uint32_t last_check_time;

// extern ErrLogger logger;

#define POTS_AT_ONCE	24

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
	Serial1.println("run checks;");
	clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_READ);
	i2cexp->i2c_on();
	memset(pot_states, 0, sizeof(pot_states));
 	for (int i = RAM_POT_STATE_ADDRESS_BEGIN; i < RAM_POT_STATE_ADDRESS_END; ++i) {
 		clock.writeRAMbyte(i, 0);
 	}//for i

	int watering_plants = 0;
// 	water_doser.prepareWatering();
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
	uint8_t state = clock.readRAMbyte(RAM_CUR_STATE);
	uint8_t sw;
	DateTime now = clock.now();
	Serial1.print("state=");
	Serial1.print(state, DEC);
	Serial1.println(';');
	
	if (state == CUR_STATE_IDLE) {
		sw = run_checks();
		if (log) {
			Serial1.print("sw = ");
			Serial1.print(sw, DEC);
			Serial1.println(';');
		}
	}
// 	if (check_and_watering) {
	run_watering();
// 	}
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
	Serial1.print("checking pot ");
	Serial1.print(index, DEC);
	Serial1.println(';');
	bool should_off = false;
	if (!i2cexp->i2c_on()) {
		should_off = true;
	}

	potConfig pot = g_cfg.readPot(index);
	if (log) {
		Serial1.print(pot.name);
		Serial1.println(';');
	}
	if ( 0 == pot.wc.enabled ) {
		if (save_result) {
			if (log) {
				Serial1.println(" is off;");
			}
			return -1;
		} else {
			return 0;
		}
	}

	uint16_t val = i2cexp->read_pin(pot.sensor.dev, pot.sensor.pin);
	bool should_water = check_watering_program(index, pot, val);
	Serial1.print("val=");
	Serial1.print(val, DEC);
	Serial1.print(" ");
	Serial1.print(should_water, DEC);
	Serial1.println(';');
	if (should_water) {
		uint8_t was = clock.readRAMbyte(RAM_POT_STATE_ADDRESS_BEGIN + index/8);
		was |= (1<<(index%8));
		clock.writeRAMbyte(RAM_POT_STATE_ADDRESS_BEGIN + index/8, was);
	}
// 	if (log) {
// // 		Serial1.print("sensor:");
// 		Serial1.print(val, DEC);
// 		Serial1.print(" ");
// 		Serial1.println(should_water, DEC);
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
	uint8_t pgm = pot.wc.pgm;
	int32_t should_water = 0;
// 	wateringProgram wpdata;
// 	read_watering_program(pot_index, wpdata);
 	Serial1.print("watering program: ");
 	Serial1.print(pgm, DEC);
	Serial1.println(';');
// 	if (pot.sensor.noise_delta < 1) {
	pot.sensor.noise_delta = 10;//round(cur_value * 0.05);//default value.
// 	}
// 	Serial1.print("noise: ");
// 	Serial1.print(pot.sensor.noise_delta, DEC);
	Serial1.print(" program: ");
	Serial1.print(pgm, DEC);
	if (pgm == 1) {
		Serial1.print(" barrier value:");
		Serial1.print(pot.pgm.const_hum.value, DEC);
		Serial1.print(" cur value:");
		Serial1.print(cur_value, DEC);
 		if ( abs(cur_value - pot.pgm.const_hum.value) <= pot.sensor.noise_delta) {
			Serial1.println(" wet enough;");
 			return false;
 		}
 		should_water = (cur_value < pot.pgm.const_hum.value);
		Serial1.print(" should_water:");
		Serial1.print(should_water, DEC);
		Serial1.println(';');
 		return should_water;
	} else if (pgm == 2) {
		Serial1.print(" range ");
		Serial1.print(pot.pgm.hum_and_dry.min_value, DEC);
		Serial1.print("..");
		Serial1.print(pot.pgm.hum_and_dry.max_value, DEC);
		Serial1.print(" cur_value:");
		Serial1.print(cur_value, DEC);
		if( abs(cur_value - pot.pgm.hum_and_dry.min_value)<= pot.sensor.noise_delta || abs(cur_value - pot.pgm.hum_and_dry.max_value) <= pot.sensor.noise_delta) {
			Serial1.print(" out of range, no actions");
			return false;
		}

		if ( pot.wc.state == 1) {
			Serial.print(" drying ");
			if ( cur_value > pot.pgm.hum_and_dry.min_value -  pot.sensor.noise_delta) {
				pot.wc.state = 0;
				g_cfg.savePot(pot_index, pot);
				Serial1.println(" end;");
				return true;
			} else {
				Serial1.println(" again;");
				return true;
			}
		} else {
			Serial1.print(" wetting ");
			if ( abs(cur_value - pot.pgm.hum_and_dry.max_value) <= pot.sensor.noise_delta) {
				Serial1.println(" wet enough. start drying;");
				pot.wc.state = 1;
				g_cfg.savePot(pot_index, pot);
				return false;
			} else {
				Serial1.println(" again;");
				return true;
			}
		}
	} else {//pgm == 2
		Serial1.println("not implemented pgm;");
	}
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

void WateringController::run_watering()
{
	uint8_t data = 0;
	clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_WATER);
// 	water_doser.prepareWatering();
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
   				uint16_t ml = water_doser.pipi(pc.wc.x, pc.wc.y, pc.wc.ml);
				data &= ~(1<<j);
				clock.writeRAMbyte(addr, data);
			}//if needs watering
		}
		++i;
	}
}

void WateringController::midnightTasks()
{
	potConfig pc;
// 	wateringProgram wp;
	for (int i = 0; i < g_cfg.config.pots_count; ++i) {
		pc = g_cfg.readPot(i);
		uint8_t pgm = pc.wc.pgm;
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
