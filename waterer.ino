#include <Servo.h>
/*
 * надо сделать:
 = насос,счётчик жидкости
 = опрос датчиков
 = конфиг полива(растения,горки,датчики,х,у,итд)
 = полив по программам

 */
#define TWI_BUFFER_LENGTH	16
#define BUFFER_LENGTH		16

#include <Wire.h>

#include "RTClib.h"
#include <AT24Cxxx.h>
#include <math.h>
#include <PCF8574.h>

#include "config_defines.h"
#include "awtypes.h"

#ifdef USE_1WIRE
	#include <OneWire.h>
#endif

#define USE_CLI

#include "messages_en.h"

#include "configuration.h"
#include "waterdosersystem.h"
#include "i2cexpander.h"
// #include "wateringcontroller.h"
// #include "waterstorage.h"

extern "C" {
#include "utility/twi.h"  // from Wire library, so we can do bus scanning
}

#define BT_BAUD		38400
/** ************************************************************************************************
 * 			GLOBAL VARIABLES
***************************************************************************************************/

volatile uint32_t last_check_time = 0;

RTC_DS1307 clock;

AT24Cxxx mem(I2C_MEMORY_ADDRESS);

extern Configuration g_cfg;

// ShiftOut hc595(PUMP_HC595_LATCH, PUMP_HC595_CLOCK, PUMP_HC595_DATA, 1);

extern WaterDoserSystem water_doser;

I2CExpander i2cExpander;
// WateringController wctl;

int8_t iForceWatering = 0;

// ErrLogger logger;
extern char*str;

/** ************************************************************************************************
 * 			functions
***************************************************************************************************/
void scanI2CBus(byte from_addr, byte to_addr, void(*callback)(byte address, byte result))
{
	byte rc;
	byte data = 0; // not used, just an address to feed to twi_writeTo()
	for ( byte addr = from_addr; addr <= to_addr; addr++ ) {
		rc = twi_writeTo(addr, &data, 0, 1, 0);
		callback( addr, rc );
	}
}

void scanFunc( byte addr, byte result )
{
	if (result!=0) return;
	Serial1.print("addr: ");
	Serial1.print(addr, DEC);
	if (addr == I2C_CLOCK_ADDRESS) {
		Serial1.println(" DS1307 clock");
	} else if ( (addr & 0xF8) == (0xF8 & I2C_MEMORY_ADDRESS) ) {
		Serial1.println(" memory chip");
	} else if ( (addr & 0xF8) == MCP23017_ADDRESS) {
		Serial1.println(" MCP23017/PCF8574");
	} else {
		Serial1.println("unknown device");
	}
// 	Serial1.print( (result==0) ? " found!":"       ");
}

void print_now()
{
	char*week_days[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
	DateTime ts = clock.now();
	Serial1.print(ts.day(), DEC);
	Serial1.print(".");
	Serial1.print(ts.month(), DEC);
	Serial1.print(".");
	Serial1.print(ts.year(), DEC);
	Serial1.print(" ");
	Serial1.print(week_days[ts.dayOfWeek()-1]);
	Serial1.print(" ");
	Serial1.print(ts.hour(), DEC);
	Serial1.print(":");
	Serial1.print(ts.minute(), DEC);
	Serial1.print(":");
	Serial1.println(ts.second(), DEC);
}

/*
	set time hh:mm:ss d.m dow
	set pots N
	set pot INDEX s dev_addr,pin,flags,dry,wet,nosoil,noise
	set pot INDEX w doser,bowl,flags,ml
	set pot INDEX name stringVALUE
	set pot INDEX p prog_num,prog_param1,prog_param2,...,prog_paramN
	set exps N
	set exp INDEX addr,type,flags
	set gcfg
 */

bool execSetCommand(char*cmd)
{
	static const char time_read_fmt[] /*PROGMEM*/ = "%d:%d:%d %d.%d.%d %d";
	if (IS(cmd, "wds", 3)) {
		cmd += 4;
		//set wds index,flags,pump_pin,wfs_pin,wfs_tick_plt,doser_move_pin,servo_pin,servo_pin2,opto_pin
		//set wds 0,3,1,5,3900,15,12,42,49,3,14;
		WaterDoserSystemConfig wdsc;
		uint8_t index;
		set_field<uint8_t>(index, &cmd);
		set_field<uint8_t>(wdsc.flags, &cmd);
		set_field<uint8_t>(wdsc.pump_pin, &cmd);
		set_field<uint8_t>(wdsc.wfs_pin, &cmd);
		set_field<uint16_t>(wdsc.wfs_ticks_per_liter, &cmd);
		set_field<uint8_t>(wdsc.doser_move_pin, &cmd);
		set_field<uint8_t>(wdsc.doser_dir_pin, &cmd);
		set_field<uint8_t>(wdsc.servo_pin, &cmd);
		set_field<uint8_t>(wdsc.servo2_pin, &cmd);
		set_field<uint8_t>(wdsc.optocoupler_pin, &cmd);
		set_field<uint8_t>(wdsc.end_switch_pin, &cmd);
// 		water_doser.setConfigItem(index, wdsc);
	} else if (IS(cmd, "gcfg", 4)) {
		cmd += 5;
		//set gcfg 0,32,2000,2000,1000,3,240,1260,300,1260
		set_field<uint16_t>(g_cfg.gcfg.flags, &cmd);
		set_field<uint8_t>(g_cfg.gcfg.pots_count, &cmd);
		set_field<uint16_t>(g_cfg.gcfg.i2c_pwron_timeout, &cmd);
		set_field<uint16_t>(g_cfg.gcfg.sensor_init_time, &cmd);
		set_field<uint16_t>(g_cfg.gcfg.sensor_read_time, &cmd);
		set_field<uint8_t>(g_cfg.gcfg.sensor_measures, &cmd);
		set_field<uint16_t>(g_cfg.gcfg.water_start_time, &cmd);
		set_field<uint16_t>(g_cfg.gcfg.water_end_time, &cmd);
		set_field<uint16_t>(g_cfg.gcfg.water_start_time_we, &cmd);
		set_field<uint16_t>(g_cfg.gcfg.water_end_time_we, &cmd);
		g_cfg.writeGlobalConfig();
	} else if (IS(cmd, "time", 4)) {
		int dow, d, m, y, h, mi, s;
		sscanf(cmd + 5, time_read_fmt,  &h, &mi, &s, &d, &m, &y, &dow);
		Serial1.println(y, DEC);
		Serial1.println(dow, DEC);
		DateTime td(y, m, d, h, mi, s, dow);
		clock.adjust(td);
		delay(1000);
		print_now();
	} else if (IS("pots", cmd, 4)) {
		cmd += 5;
		uint8_t old_pc = g_cfg.gcfg.pots_count;
		set_field<uint8_t>(g_cfg.gcfg.pots_count, &cmd);
		g_cfg.writeGlobalConfig();
// 		if (old_pc < g_cfg.gcfg.pots_count) {
// 			potConfig pc;
// 			for (;old_pc < g_cfg.gcfg.pots_count; ++old_pc) {
// 				Serial1.print("erase ");
// 				Serial1.println(old_pc);
// 				memset(&pc, 0, sizeof(potConfig));
// 				g_cfg.savePot(old_pc, pc);
// 			}
// 		}
	} else if (IS("pot", cmd, 3)) {
		int8_t index = 0, ok = 0;
		char *ptr = cmd + 4;
		while (isdigit(*ptr)) {ok = 1;index*=10;index+=*ptr-'0';++ptr;}

		potConfig pot = g_cfg.readPot(index);
		Serial1.print("index:");
		Serial1.println(index, DEC);
// 		Serial1.println(ptr);
		while (*ptr && isspace(*ptr)) ++ptr;
		if ( IS(ptr, "on", 2)) {
// 			WCF_SET_EN(pot, 1);
			pot.wc.enabled=1;
		} else if (IS(ptr, "off", 3)) {
// 			WCF_SET_EN(pot, 0);
			pot.wc.enabled=0;
		} else if (IS(ptr, "s", 1)) {//set pot INDEX s dev_addr,pin,flags,dry,wet,nosoil
			while (*ptr && !isdigit(*ptr))++ptr;
			set_field<uint8_t>(pot.sensor.dev_addr, &ptr);
			set_field<uint8_t>(pot.sensor.pin, &ptr);
			set_field<uint8_t>(pot.sensor.flags, &ptr);
// 			set_field<uint32_t>(pot.sensor.dry_freq, &ptr);
// 			set_field<uint32_t>(pot.sensor.wet_freq, &ptr);
// 			set_field<uint32_t>(pot.sensor.no_soil_freq, &ptr);
// 			set_field<uint16_t>(pot.sensor.noise_delta, &ptr);
		} else if (IS(ptr, "w", 1)) {
			//set pot INDEX w doser,bowl,flags,ml
			while (*ptr && !isdigit(*ptr))++ptr;
// 			set_field<uint8_t>(pot.wc.doser, &ptr);
// 			set_field<uint8_t>(pot.wc.bowl, &ptr);
// 			set_field<uint8_t>(pot.wc.flags, &ptr);
// 			set_field<uint8_t>(pot.wc.ml, &ptr);
		} else if (IS(ptr, "name", 4)) {
			strncpy(pot.name, ptr + 4 + 1, POT_NAME_LENGTH);
		} else if (IS(ptr, "p", 1)) {
			while (*ptr && !isdigit(*ptr))++ptr;
			uint8_t p_id = 0;
			set_field<uint8_t>(p_id, &ptr);
			wateringProgram wp;
// 			wctl.read_watering_program(index, wp);
			if (p_id == 1) {
// 				set_field<uint32_t>(wp.const_hum.value, &ptr);
// 				set_field<uint16_t>(wp.const_hum.max_ml, &ptr);
			} else if (p_id == 2) {
// 				set_field<uint32_t>(wp.hum_and_dry.min_value, &ptr);
// 				set_field<uint32_t>(wp.hum_and_dry.max_value, &ptr);
// 				set_field<uint16_t>(wp.hum_and_dry.max_ml, &ptr);
			} else if (p_id == 3) {
// 				set_field<uint32_t>(wp.by_time.min_value, &ptr);
// 				set_field<uint16_t>(wp.by_time.interval, &ptr);
// 				set_field<uint16_t>(wp.by_time.ml, &ptr);
			}else if (p_id == 4) {
// 				set_field<uint16_t>(wp.shrinked_time.from_time, &ptr);
// 				set_field<uint16_t>(wp.shrinked_time.to_time, &ptr);
// 				set_field<uint8_t>(wp.shrinked_time.program, &ptr);
			}
// 			wctl.write_watering_program(index, wp);
			pot.wc.pgm_id = p_id;
			
// 			WCF_SET_PROGRAM_ID(pot, p_id);
		} else if (IS("d", ptr, 1)) {
			while (*ptr && !isdigit(*ptr))++ptr;
			Serial1.print("cfg dump:");
			Serial1.println(ptr);
			set_field<uint8_t>(pot.sensor.dev_addr, &ptr);
			set_field<uint8_t>(pot.sensor.pin, &ptr);
			set_field<uint8_t>(pot.sensor.flags, &ptr);
// 			set_field<uint32_t>(pot.sensor.dry_freq, &ptr);
// 			set_field<uint32_t>(pot.sensor.wet_freq, &ptr);
// 			set_field<uint32_t>(pot.sensor.no_soil_freq, &ptr);
// 			set_field<uint16_t>(pot.sensor.noise_delta, &ptr);
// 			set_field<uint8_t>(pot.wc.doser, &ptr);
// 			set_field<uint8_t>(pot.wc.bowl, &ptr);
// 			set_field<uint8_t>(pot.wc.flags, &ptr);
			set_field<uint8_t>(pot.wc.ml, &ptr);

			while (*ptr && !isalpha(*ptr)) ++ptr;
			char*pn = pot.name;
			while (*ptr && *ptr != ';' && ((pn - pot.name) < (POT_NAME_LENGTH - 1) )) {
				*pn = *ptr;
				++pn;
				++ptr;
			}
			*pn = 0;
// 			strncpy(pot.name, ptr, POT_NAME_LENGTH);
		}
		g_cfg.savePot(index, pot);
// 		dumpPotConfig(index);
	}
	return true;
}

void dumpPotConfig(uint8_t index)
{
	potConfig pot = g_cfg.readPot(index);
	print_field<uint8_t>(pot.sensor.dev_addr);
	print_field<uint8_t>(pot.sensor.pin);
	print_field<uint8_t>(pot.sensor.flags);
// 	print_field<uint32_t>(pot.sensor.dry_freq);
// 	print_field<uint32_t>(pot.sensor.wet_freq);
// 	print_field<uint32_t>(pot.sensor.no_soil_freq);
// 	print_field<uint16_t>(pot.sensor.noise_delta);
// 	print_field<uint8_t>(pot.wc.doser);
// 	print_field<uint8_t>(pot.wc.bowl);
// 	print_field<uint8_t>(pot.wc.flags);
	print_field<uint8_t>(pot.wc.ml);
	Serial1.print(pot.name);
	Serial1.println(";");
}

bool execGetCommand(char*cmd)
{
	if (IS("wstat", cmd, 5)) {
		for (int i =0 ; i < g_cfg.gcfg.pots_count; ++i) {
			potConfig pot = g_cfg.readPot(i);
			Serial1.print("plant ");
			Serial1.println(pot.name);
// 			if (!is_pot_watering_en(pot)) {
// 				Serial1.println(" inactive\n");
// 			} else {
// 				Serial1.print("watered today: ");
// 				Serial1.print(pot.wc.watered, DEC);
// 				Serial1.println("ml\n");
// 			}
		}
	} else if (IS("pgm", cmd, 3)) {
		/*int8_t index = -1;
		cmd += 4;
		set_field<int8_t>(index, &cmd);
		potConfig pc = g_cfg.readPot(index);
		Serial1.print("pot ");
		Serial1.print(index, DEC);
		Serial1.print(" ");
		Serial1.print(pc.name);
// 		Serial1.println(WCF_GET_EN(pc)?" is on":" is off");
// 		uint8_t pgm = WCF_GET_PROGRAM_ID(pc);
		wateringProgram wp;
		Serial1.print("program:");
		Serial1.println(pgm, DEC);
// 		wctl.read_watering_program(index, wp);
		Serial1.println("params:");
		if (pgm == 0) {
			Serial1.println("program is not set");
		} else if (pgm == 1) {
			Serial1.print("value:");
// 			Serial1.println(wp.const_hum.value, DEC);
			Serial1.print("max ml per day:");
// 			Serial1.println(wp.const_hum.max_ml, DEC);
		} else if (pgm == 2) {
			Serial1.print("min value: ");
// 			Serial1.println(wp.hum_and_dry.min_value, DEC);
			Serial1.print("max value: ");
// 			Serial1.println(wp.hum_and_dry.max_value, DEC);
			Serial1.print("max ml per day:");
// 			Serial1.println(wp.hum_and_dry.max_ml, DEC);
		}*/
	} else if (IS("wds", cmd, 3)) {
// 		Serial1.println("Not yet implemented");
// set wds index,flags,pump_pin,wfs_pin,wfs_tick_plt,doser_move_pin,servo_pin,opto_pin
		cmd += 4;
		int8_t index = -1;
		set_field<int8_t>(index, &cmd);
// 		water_doser.dumpCfg(index);
	} else if (IS("time",cmd, 4)) {
		print_now();
	} else if (IS("gcfg", cmd, 4)) {
		print_field<uint16_t>(g_cfg.gcfg.flags);
// 		print_field<uint8_t>(g_cfg.gcfg.i2c_expanders_count);
		print_field<uint8_t>(g_cfg.gcfg.pots_count);
// 		print_field<uint8_t>(g_cfg.gcfg.dosers_count);
		print_field<uint16_t>(g_cfg.gcfg.i2c_pwron_timeout);
		print_field<uint16_t>(g_cfg.gcfg.sensor_init_time);
		print_field<uint16_t>(g_cfg.gcfg.sensor_read_time);
		print_field<uint8_t>(g_cfg.gcfg.sensor_measures);
		print_field<uint16_t>(g_cfg.gcfg.water_start_time);//+
		print_field<uint16_t>(g_cfg.gcfg.water_end_time);//+
		print_field<uint16_t>(g_cfg.gcfg.water_start_time_we);//+
		print_field<uint16_t>(g_cfg.gcfg.water_end_time_we, ';');//+
	} else if (IS("pot", cmd, 3)) {
		uint8_t index = 0;
		cmd+=4;
		set_field<uint8_t>(index, &cmd);
// 		dumpPotConfig(index);
	}/* else if (IS("exp", cmd, 3)) {
		uint8_t index = 0;
		cmd+=4;
		set_field<uint8_t>(index, &cmd);
		expanderConfig ec = g_cfg.readExpander(index);
		print_field<uint8_t>(ec.address);
		print_field<uint8_t>(ec.type);
		print_field<uint8_t>(ec.flags, ';');
	} */else if (IS("pcfg", cmd, 6)) {
		Serial1.print("{");
			Serial1.print(g_cfg.gcfg.pots_count, DEC);
			Serial1.print(";");
			for (uint8_t i = 0; i < g_cfg.gcfg.pots_count; ++i) {
// 				dumpPotConfig(i);
				Serial1.println();
			}//for i
		Serial1.println("}");
	}
	return true;
}

void doWaterDoserCommand(char*cmd)
{
	if(IS(cmd, "rew", 3)) {
// 		water_doser.rewind(0);
	} else if (IS(cmd, "pos", 3)) {
		cmd += 4;
		int8_t pos = -1;
		set_field<int8_t>(pos, &cmd);
		if (pos>=0) {
// 			water_doser.moveToPos(0, pos);
		}
	} else if(IS(cmd, "test", 4)) {
		cmd += 5;
		uint8_t ml = 0, index=0;
		set_field<uint8_t>(index, &cmd);
		set_field<uint8_t>(ml, &cmd);
		if (ml > 0) {
			Serial1.println("STUB");
// 			water_doser.init_measure(index);
// 			Serial1.println(water_doser.measure(index, ml, 5000), DEC);
		}
	}/* else if (IS("nodl", cmd, 4)) {
		water_doser.nod(0, true);
	} else if (IS("nodr", cmd, 4)) {
		water_doser.nod(0, false);
	} */else if (IS("pipi", cmd, 4)) {
		int8_t index=-1;
		uint8_t pos=0, ml=0;
		cmd += 5;
		set_field<int8_t>(index, &cmd);
		set_field<uint8_t>(pos, &cmd);
// 		set_field<int8_t>(is_left, &cmd);
		set_field<uint8_t>(ml, &cmd);
		if (ml > 0) {
// 			water_doser.pipi(index, pos, ml);
		}
	}
}

/*
cal <DNW> dev_addr [pin]
 */
bool doCommand(char*cmd)
{
	if (IS(cmd, "ptest", 5)) {
		int8_t index = -1;
		cmd += 6;
		set_field<int8_t>(index, &cmd);
		if (index > -1) {
// 			wctl.check_pot_state(index, false);
		}
	} else if (IS(cmd, "sst", 3)) {
		i2cExpander.set_mode(0);
		i2cExpander.i2c_on();
		Wire.begin();
		for (int i=0; i < g_cfg.gcfg.pots_count; ++i) {
			potConfig pc = g_cfg.readPot(i);
			int32_t val = i2cExpander.read_pin(pc.sensor.dev_addr, pc.sensor.pin);
			Serial1.print("dev ");
			Serial1.print(pc.sensor.dev_addr, DEC);
			Serial1.print(" pin ");
			Serial1.println(pc.sensor.pin, DEC);
			Serial1.println(pc.name);
			Serial1.print("cur value=");
			Serial1.println(val, DEC);
// 			if (abs(val - pc.sensor.no_soil_freq) <= pc.sensor.noise_delta) {
// 				Serial1.println("Sensor is out from soil");
// 			} else if (abs(val - pc.sensor.wet_freq) <= pc.sensor.noise_delta) {
// 				Serial1.println("Clear water detected");
// 			}
		}
		i2cExpander.i2c_off();
	} else if (IS("wd", cmd, 2)) {
		cmd +=3;
		doWaterDoserCommand(cmd);
	} else if (IS("ping", cmd, 4)) {
		Serial1.println("pong");
		print_now();
	} else if (IS("set", cmd, 3)) {
		execSetCommand(cmd + 4);
	} else if (IS("get", cmd, 3)) {
		execGetCommand(cmd+4);
	} else if (IS("test", cmd, 4)) {
		//test addr pin
		cmd += 5;
		i2cExpander.set_mode(0);
		int8_t addr, pin=-1;
		set_field<int8_t>(addr, &cmd);
		set_field<int8_t>(pin, &cmd);
		if(pin >=0) {
			Serial1.println(i2cExpander.calibrate_pin(addr, pin, g_cfg.gcfg.sensor_measures), DEC);
		} else {
			i2cExpander.calibrate_dev(addr, g_cfg.gcfg.sensor_measures);
		}
	} else if (IS("cal", cmd, 3)) {
		//cal <DNW> addr pin
		cmd += 4;
		Serial1.println(*cmd);
		if(*cmd == 'N' || *cmd == 'n') {
			i2cExpander.set_mode(1);
		} else if(*cmd == 'D' || *cmd == 'd') {
			i2cExpander.set_mode(2);
		} else if(*cmd == 'W' || *cmd == 'w') {
			i2cExpander.set_mode(3);
		} else {
			i2cExpander.set_mode(0);
		}
		int8_t addr=-1, pin=-1;
		cmd+=2;
		Serial1.println(cmd);
		set_field<int8_t>(addr, &cmd);
 		set_field<int8_t>(pin, &cmd);
		Serial1.print(addr, DEC);
		Serial1.print("/");
 		Serial1.println(pin, DEC);
		if(pin >=0) {
			i2cExpander.calibrate_pin(addr, pin, g_cfg.gcfg.sensor_measures);
		} else {
			i2cExpander.calibrate_dev(addr, g_cfg.gcfg.sensor_measures);
		}
	} else if (IS("i2c", cmd, 3)) {
		if (IS(cmd+4, "scan", 4)) {
			i2cExpander.i2c_on();
				scanI2CBus(1, 127, scanFunc );
			i2cExpander.i2c_off();
		}
	} else if (IS("start", cmd, 5)) {
		g_cfg.gcfg.flags |= 0x01;
		g_cfg.writeGlobalConfig();
		Serial1.println("started");
	} else if (IS("stop", cmd, 4)) {
		g_cfg.gcfg.flags &= 0xFE;
		g_cfg.writeGlobalConfig();
		Serial1.println("halted");
	} else if (IS("restart", cmd, 7)) {
		last_check_time = 0;
		clock.writeRAMbyte(LAST_CHECK_TS_1, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_2, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_3, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_4, 0);
		g_cfg.gcfg.flags |= 0x01;
		g_cfg.writeGlobalConfig();
		Serial1.println("started");
	}
	return true;
}

void checkCommand()
{
	if (!Serial1.available()) return;
	static int i = 0;
	static char cmdbuf[64]={0}, ch;
	while (Serial1.available()) {
		ch = Serial1.read();
		if(ch == 7) {
			i=0;
			continue;
		}
		if (ch ==10 || ch ==13) {
			if ( i == 0 ) continue;
			cmdbuf[ i ] = 0;
			doCommand(cmdbuf);
			i = 0;
		} else {
			cmdbuf[i++] = ch;
		}
	}//while

}//sub

static const char s_step1[] PROGMEM = "IIC";
static const char s_step2[] PROGMEM = "RTC";
static const char s_step3[] PROGMEM = "config read";
static const char s_step4[] PROGMEM = "doser setupped";
static const char s_step5[] PROGMEM = "water CTL inited";

void setup()
{
	Serial1.begin(BT_BAUD);
	Serial1.println("HELLO");
	Wire.begin();
 	clock.begin();
	
//  	g_cfg.begin();
   	water_doser.begin();
// 	wctl.init(&i2cExpander);
// 	last_check_time = ((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_1) << 24) | ((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_2) << 16) |((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_3) << 8) | (uint32_t)clock.readRAMbyte(LAST_CHECK_TS_4);
}

/**
	TODO: программные кнопки включения насосов. с парковкой соотв. дозатора в первую занятую дырку(чтоб не лило мимо).
*/

bool midnight_skip = false;

void loop()
{

	PORTE = PINE ^ (1<<2);
	return;
	checkCommand();

	DateTime now = clock.now();
	uint16_t now_m = now.hour() * 60 + now.minute();

	if ((g_cfg.gcfg.flags & 0x01)
			&& (
					(now.dayOfWeek() < 6 && now_m > g_cfg.gcfg.water_start_time && now_m < g_cfg.gcfg.water_end_time)
				||
					(now.dayOfWeek() >= 6 && now_m > g_cfg.gcfg.water_start_time_we && now_m < g_cfg.gcfg.water_end_time_we)
			) || iForceWatering) {
		midnight_skip = true;
   		Serial1.print("times: now: ");
   		Serial1.print(now.secondstime(), DEC);
   		Serial1.print(" prev: ");
   		Serial1.print(last_check_time, DEC);
   		Serial1.print(" ");
  		Serial1.println(now.secondstime() - last_check_time, DEC);

		if ((now.secondstime() - last_check_time > 30 * 60) || iForceWatering) {

// 			wctl.doPotService(iForceWatering != 1);

			last_check_time = now.secondstime();
			clock.writeRAMbyte(LAST_CHECK_TS_1, last_check_time >> 24);
			clock.writeRAMbyte(LAST_CHECK_TS_2, last_check_time >> 16);
			clock.writeRAMbyte(LAST_CHECK_TS_3, last_check_time >> 8);
			clock.writeRAMbyte(LAST_CHECK_TS_4, last_check_time & 0xFF);
			iForceWatering = 0;
		}
	} else if (now_m < 2 && !midnight_skip) {//midnight
//  		Serial1.println("midnight jobs");
		last_check_time = 0;
		clock.writeRAMbyte(LAST_CHECK_TS_1, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_2, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_3, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_4, 0);
// 		Serial1.println("wctl.midnightTasks()");
// 		Serial1.flush();
// 		wctl.midnightTasks();
// 		Serial1.println("wctl.midnightTasks() ended");
		Serial1.flush();
		g_cfg.midnight_tasks();
// 		Serial1.println("g_cfg.midnight_tasks(); ended");
// 		Serial1.flush();
		midnight_skip = true;
	}
// 		- last_check_time > g_cfg.gcfg.
//  	Serial1.println("ping");
// 	Serial1.println(freeMemory(), DEC);
	delay(100);
}
