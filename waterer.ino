// #include <Servo.h>
/*
 * надо сделать:
 = насос,счётчик жидкости
 = опрос датчиков		+
 = конфиг полива(растения,горшки,датчики,х,у,итд)
 = полив по программам
 */

#define TWI_BUFFER_LENGTH	16
#define BUFFER_LENGTH		16

#include <Wire.h>

#include "RTClib.h"
#include <AT24Cxxx.h>
#include "freemem.h"
#include <math.h>
#include <PCF8574.h>
#ifdef MY_ROOM
	#define PLANT_LIGHT_PIN 12
	#include <BH1750.h>
#endif

#include<EEPROM.h>

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
#include "wateringcontroller.h"
// #include "waterstorage.h"
#include "esp8266.h"

extern "C" {
#include "utility/twi.h"  // from Wire library, so we can do bus scanning
}

#define BT_BAUD		38400
/** ************************************************************************************************
 * 			GLOBAL VARIABLES
***************************************************************************************************/

#ifdef MY_ROOM
	BH1750 lightMeter;
	ESP8266 esp8266(&Serial);
#endif
volatile uint32_t last_check_time = 0;

RTC_DS1307 clock;

AT24Cxxx mem(I2C_MEMORY_ADDRESS);

extern Configuration g_cfg;

// ShiftOut hc595(PUMP_HC595_LATCH, PUMP_HC595_CLOCK, PUMP_HC595_DATA, 1);

extern WaterDoserSystem water_doser;

I2CExpander i2cExpander;
WateringController wctl;

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
	Serial1.print(F("addr: "));
	Serial1.print(addr, DEC);
	if (addr == I2C_CLOCK_ADDRESS) {
		Serial1.println(F(" DS1307 clock;"));
	} else if ( (addr & 0xF8) == (0xF8 & I2C_MEMORY_ADDRESS) ) {
		Serial1.println(F(" memory chip;"));
	} else if ( (addr & 0xF8) == PCF8574_ADDRESS) {
		Serial1.println(F(" MCP23017/PCF8574;"));
	} else {
		Serial1.println(F("unknown device;"));
	}
// 	Serial1.print( (result==0) ? " found!":"       ");
}

void print_now()
{
	char*week_days[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
	DateTime ts = clock.now();
	Serial1.print(ts.day(), DEC);
	Serial1.print(F("."));
	Serial1.print(ts.month(), DEC);
	Serial1.print(F("."));
	Serial1.print(ts.year(), DEC);
	Serial1.print(F(" "));
	Serial1.print(week_days[ts.dayOfWeek()-1]);
	Serial1.print(F(" "));
	Serial1.print(ts.hour(), DEC);
	Serial1.print(F(":"));
	Serial1.print(ts.minute(), DEC);
	Serial1.print(F(":"));
	Serial1.print(ts.second(), DEC);
	Serial1.println(';');
}

void dumpPotConfig(uint8_t index, char*output)
{
	//pot set <index>,<flags>,<dev>,<pin>,<x>,<y>,<name>,<airTime>,<state>,<enabled>,<ml>,<pgm>,<param1>[,param2][,param3]...;
	potConfig pot = g_cfg.readPot(index);
	char*ptr = output;
	ptr += sprintf(output, "%d,%d,%d,%d,%d,\"%s\",%d,%d,%d,%d,%d,",
				index,
				pot.sensor.dev,
				pot.sensor.pin,
				pot.wc.x,
				pot.wc.y,
				pot.name,
				pot.wc.airTime,
				pot.wc.state,
		 pot.wc.enabled,
		 pot.wc.ml,
		 pot.wc.pgm);
// 	print_field<uint8_t>(index);
// 	print_field<uint8_t>(pot.sensor.dev);
// 	print_field<uint8_t>(pot.sensor.pin);
//  	print_field<uint8_t>((uint8_t)pot.wc.x);
//  	print_field<uint8_t>((uint8_t)pot.wc.y);
// 	Serial1.print(pot.name);
// 	Serial1.print(',');
// 	print_field<uint8_t>(pot.wc.airTime);
// 	print_field<uint8_t>(pot.wc.state);
// 	print_field<uint8_t>(pot.wc.enabled);
// 	print_field<uint8_t>(pot.wc.ml);
// 	print_field<uint8_t>(pot.wc.pgm);
	if (pot.wc.pgm == 1) {
		sprintf(ptr, "%d,%d;\r\n", pot.pgm.const_hum.value, pot.pgm.const_hum.max_ml);
// 		print_field<uint16_t>(pot.pgm.const_hum.value);
// 		print_field<uint16_t>(pot.pgm.const_hum.max_ml,';');
	} else if (pot.wc.pgm == 2) {
		sprintf(ptr, "%d,%d,%d;\r\n", pot.pgm.hum_and_dry.min_value, pot.pgm.hum_and_dry.min_value, pot.pgm.hum_and_dry.max_ml);
// 		print_field<uint16_t>(pot.pgm.hum_and_dry.min_value);
// 		print_field<uint16_t>(pot.pgm.hum_and_dry.max_value);
// 		print_field<uint16_t>(pot.pgm.hum_and_dry.max_ml,';');
	} else {
		Serial1.print(F("???"));
	}// 	print_field<uint32_t>(pot.sensor.dry_freq);
// 	print_field<uint32_t>(pot.sensor.wet_freq);
// 	print_field<uint32_t>(pot.sensor.no_soil_freq);
// 	print_field<uint16_t>(pot.sensor.noise_delta);
// 	print_field<uint8_t>(pot.wc.doser);
// 	print_field<uint8_t>(pot.wc.bowl);
// 	print_field<uint8_t>(pot.wc.flags);
// 	print_field<uint8_t>(pot.wc.ml);
// 	Serial1.print(pot.name);
// 	Serial1.println(';');
}


char* printGcfg(char*ptr)
{
	char*cp = ptr;
	uint16_t val = (uint16_t)g_cfg.config.enabled;
// 	print_field<uint16_t>(val);
	val = (uint16_t)g_cfg.config.flags;
	cp += sprintf(cp, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;",val,
					g_cfg.config.pots_count,
					g_cfg.config.i2c_pwron_timeout,
					g_cfg.config.sensor_init_time,
					g_cfg.config.sensor_read_time,
					g_cfg.config.water_start_time,
					g_cfg.config.water_end_time,
					g_cfg.config.water_start_time_we,
					g_cfg.config.water_end_time_we,
					g_cfg.config.sensor_measures,
					g_cfg.config.test_interval
				);
	return cp;
/*			set_field<uint16_t>(val, &ptr);
			g_cfg.config.flags = val;
			g_cfg.config.enabled = val & 1;
			set_field<uint8_t>(g_cfg.config.pots_count, &ptr);
			set_field<uint16_t>(g_cfg.config.i2c_pwron_timeout, &ptr);
			set_field<uint16_t>(g_cfg.config.sensor_init_time, &ptr);
			set_field<uint16_t>(g_cfg.config.sensor_read_time, &ptr);
			set_field<uint16_t>(g_cfg.config.water_start_time, &ptr);
			set_field<uint16_t>(g_cfg.config.water_end_time, &ptr);
			set_field<uint16_t>(g_cfg.config.water_start_time_we, &ptr);
			set_field<uint16_t>(g_cfg.config.water_end_time_we, &ptr);
			set_field<uint8_t>(g_cfg.config.sensor_measures, &ptr);
*/
}

bool doCommand(char*cmd)
{
	static const char time_read_fmt[] /*PROGMEM*/ = "%d:%d:%d %d.%d.%d %d";
	/*if (IS(cmd,"er", 2)) {
		Serial1.println(EEPROM.read(33));
		
	} else if (IS(cmd,"ew", 2)) {
		EEPROM.write(33, 55);
		Serial1.println("OK;");
	} else if (IS(cmd,"et")) {
		uint8_t tmp;
		for (uint16_t i=0;i<4096;++i) {
			EEPROM.write(i, 0x55);
			tmp = EEPROM.read(i);
			if (tmp!=0x55) {
				Serial1.print("error 55 at ");
				Serial1.print(i, DEC);
				Serial1.print(" ");
				Serial1.println(tmp, HEX);
			}
			EEPROM.write(i, 0xAA);
			tmp = EEPROM.read(i);
			if (tmp!=0xAA) {
				Serial1.print("error AA at ");
				Serial1.print(i, DEC);
				Serial1.print(" ");
				Serial1.println(tmp, HEX);
			}
		}
		Serial1.println("done;");
	} else */
	if (IS_P(cmd, PSTR("sdump"), 5)) {
// 		wctl.run_checks();
		wctl.dumpSensorValues(str);
	} /*else if (IS_P(cmd, PSTR("pcf"), 3)) {
		char*ptr = cmd + 4;
		int c,p,v;
		set_field<int>(c, &ptr);
		set_field<int>(p, &ptr);
		set_field<int>(v, &ptr);
		Serial1.print(F("pcf set "));
		Serial1.print(c, DEC);
		Serial1.print(F("/"));
		Serial1.print(p, DEC);
		Serial1.print(F("="));
		Serial1.print(v, DEC);
		i2cExpander.i2c_on();
		i2cExpander.begin(c);
		i2cExpander.write(p,v);
	} */else if (IS_P(cmd, PSTR("stat"), 4)) {
		if (IS_P(cmd+5, PSTR("clr"), 3)) {
			wctl.cleanDayStat();
		} else if (IS_P(cmd + 5, PSTR("get"), 3)) {
			wctl.printDayStat();
		}
	} else if (IS_P(cmd, PSTR("lux"), 3)) {
#ifdef MY_ROOM
		if (cmd[4]=='?') {
			Serial1.print(F("lux:"));
			Serial1.println(lightMeter.readLightLevel(), DEC);
		} else if (cmd[3] == '=') {
			g_cfg.config.lux_barrier_value = atoi(cmd+5);
			g_cfg.writeGlobalConfig();
			g_cfg.readGlobalConfig();
			Serial1.print(F("new lux value="));
			Serial1.println(g_cfg.config.lux_barrier_value, DEC);
		}
#endif
	} else if (IS_P(cmd, PSTR("A"), 1)) {
		Serial1.println(analogRead(cmd[1]-'0'), DEC);
	} else if (IS_P(cmd, PSTR("+"), 1)) {
		int pin = atoi(cmd+1);
		if(pin > 2) {
			Serial1.print(F("HIGH pin:"));
			Serial1.println(pin, DEC);
			pinMode(pin, OUTPUT);
			digitalWrite(pin, HIGH);
		}
	} else if(IS_P(cmd, PSTR("-"), 1)) {
		int pin = atoi(cmd+1);
		if(pin > 2) {
			pinMode(pin, OUTPUT);
			digitalWrite(pin, LOW);
		}		
	} else if (IS_P(cmd, PSTR("pipi"), 4)) {
		uint8_t x=0xFF, y=0xFF, ml=0xFF;
		char*ptr = cmd+5;
		set_field<uint8_t>(x, &ptr);
		set_field<uint8_t>(y, &ptr);
		set_field<uint8_t>(ml, &ptr);
		water_doser.pipi(x,y,ml, atMedium);
	} else if (IS_P(cmd, PSTR("WSZ"), 3)) {
		Serial1.print(WD_SIZE_X);
		Serial1.print(F(","));
		Serial1.print(WD_SIZE_Y);
		Serial1.println(';');
	} else if (cmd[0]== 'U') {
		water_doser.servoUp();
	}else if (cmd[0]=='D') {
		water_doser.servoDown();
	} else if ('G'==cmd[0]) {
		char *ptr = cmd + 1;
		if (cmd[1] == 'A') {
			water_doser.testAll();
			return true;
		}
		if (*ptr == '!') {
			water_doser.parkX();
			water_doser.parkY();
			++ptr;
		}
		int x,y;
		set_field<int>(x, &ptr);
		set_field<int>(y, &ptr);
		Serial1.print(x, DEC);
		Serial1.print(F(" "));
		Serial1.print(y, DEC);
		water_doser.moveToPos(x, y);
	} else if (IS_P(cmd, PSTR("iic"), 3)) {
		i2cExpander.i2c_on();
		if (IS_P(cmd+4, PSTR("off"), 3)) {
			i2cExpander.i2c_off();
		} else if (IS_P(cmd + 4, PSTR("scan"), 4)) {
			uint8_t addr = 32;
			while (i2cExpander.findNext(addr, 32+7, &addr)) {
				Serial1.print(addr,DEC);
				Serial1.print(F(","));
				++addr;
			}
			addr = 56;
			while (i2cExpander.findNext(addr, 56+7, &addr)) {
				Serial1.print(addr,DEC);
				Serial1.print(F(","));
				++addr;
			}
			Serial1.println(F("0;"));
		} else if (isdigit(*(cmd+4))) {
			int dev=-1,pin=-1;
			char *ptr = cmd + 4;
			set_field<int>(dev, &ptr);
			set_field<int>(pin, &ptr);
			Serial1.print(dev, DEC);
			Serial1.print(F(","));
			Serial1.print(pin, DEC);
			Serial1.print(F(","));
			Serial1.print(i2cExpander.read_pin(dev,pin), DEC);
			Serial1.println(F(";"));
		}
		i2cExpander.i2c_off();
	} else if (IS_P(cmd, PSTR("ping"), 4)) {
		sprintf(str, "pong %s %s %s;\r\n", VERSION_TYPE, __DATE__, __TIME__);
// 		Serial1.println(F("pong;"));
// 		print_now();
// #ifdef MY_ROOM
// 		Serial1.println(F("MY_ROOM ver"));
// #else
// 		Serial1.println(F("BIG_ROOM ver"));
// #endif
// 		Serial1.print(__DATE__);
// 		Serial1.println(F(" "));
// 		Serial1.print(__TIME__);
// 		Serial1.println(F(";"));
	} else if (IS_P(cmd, PSTR("time"), 4)) {
		if (IS_P(cmd+5, PSTR("get"), 3)) {
		} else if (IS_P(cmd+5, PSTR("set"), 3)) {
			//time set dd:dd:dd dd.d.dddd d
			int dow, d, m, y, h, mi, s;
			sscanf(cmd + 9, time_read_fmt,  &h, &mi, &s, &d, &m, &y, &dow);
// 			Serial1.print(y, DEC);
// 			Serial1.println(dow, DEC);
			DateTime td(y, m, d, h, mi, s, dow);
			clock.adjust(td);
			delay(1000);
		}else if(IS_P(cmd+5,PSTR("adj"), 3)) {
			DateTime now = esp8266.getTimeFromNTPServer();
			clock.adjust(now);
			print_now();
		} else {
			return false;
		}
		print_now();
	} else if (IS_P(cmd, PSTR("pot"), 3)) {
		if (IS_P(cmd + 4, PSTR("get"), 3)) {
			/*if (IS_P(cmd+4+4, PSTR("count"), 5)) {
				Serial1.print(g_cfg.config.pots_count, DEC);
				Serial1.println(";");
			} else */if (isdigit(*(cmd+4+4))) {
				int index = -1;
				char *ptr = cmd + 8;
				set_field<int>(index, &ptr);
				if (index >=0 && index < g_cfg.config.pots_count) {
					dumpPotConfig(index, str);
				} else {
					return false;
				}
			}
		} else if (IS_P(cmd + 4 , PSTR("set"), 3)) {
			/*if (IS_P(cmd + 8, PSTR("count"), 5)) {
				uint8_t tmp;
				char*ptr=cmd+8+6;
// 				Serial1.print("str:");
// 				Serial1.print(ptr);
// 				Serial1.println("]");
				set_field<uint8_t>(tmp, &ptr);
// 				Serial1.print("pots count=");
// 				Serial1.println(tmp, DEC);
				g_cfg.config.pots_count = tmp;
				g_cfg.writeGlobalConfig();
				Serial1.println(F("OK;"));
				return true;
			}*/
         //pot set <index:0>,<dev:1>,<pin:2>,<x:3>,<y:4>,<name:5>,<airTime:6>,<state:7>,<enabled:8>,<ml:9>,<pgm:10>,<param1:11>[,param2][,param3]...,<daymax>;
         //pot set 0,34,1,0,0,euc x1,2,0,1,30,1,700,500
			uint8_t tmp, index;
			char*ptr=cmd+8;
			set_field<uint8_t>(index, &ptr);
			potConfig pc = g_cfg.readPot(index);
			set_field<uint8_t>(tmp, &ptr);
			pc.sensor.dev = tmp;
			set_field<uint8_t>(tmp, &ptr);
			pc.sensor.pin = tmp;
			set_field<uint8_t>(tmp, &ptr);
			pc.wc.x = tmp;
			set_field<uint8_t>(tmp, &ptr);
			pc.wc.y = tmp;
			char*dst=pc.name;
			while (*ptr && *ptr!=',') *dst++ = *ptr++;
			*dst = 0;
			++ptr;
			set_field<uint8_t>(tmp, &ptr);
			pc.wc.airTime = tmp & 0x03;
			set_field<uint8_t>(tmp, &ptr);
			pc.wc.state = tmp & 1;
			set_field<uint8_t>(tmp, &ptr);
			pc.wc.enabled = tmp & 1;
			set_field<uint8_t>(tmp, &ptr);
			pc.wc.ml = tmp;
			set_field<uint8_t>(tmp, &ptr);
			pc.wc.pgm = tmp;
// 			Serial1.print(F("pgm="));
// 			Serial1.println(pc.wc.pgm, DEC);
			if (pc.wc.pgm == 1) {
				set_field<uint16_t>(pc.pgm.const_hum.value, &ptr);
				set_field<uint16_t>(pc.pgm.const_hum.max_ml, &ptr);
// 				Serial1.print(pc.pgm.const_hum.value, DEC);
// 				Serial1.print(F(" daymax="));
// 				Serial1.println(pc.pgm.const_hum.max_ml, DEC);
			} else if (pc.wc.pgm == 2) {
				set_field<uint16_t>(pc.pgm.hum_and_dry.min_value, &ptr);
				set_field<uint16_t>(pc.pgm.hum_and_dry.max_value, &ptr);
				set_field<uint16_t>(pc.pgm.hum_and_dry.max_ml, &ptr);
// 				Serial1.print(pc.pgm.hum_and_dry.min_value, DEC);
// 				Serial1.print(F(".."));
// 				Serial1.print(pc.pgm.hum_and_dry.max_value, DEC);
// 				Serial1.print(F(" daymax="));
// 				Serial1.println(pc.pgm.hum_and_dry.max_ml, DEC);
			}
			g_cfg.savePot(index, pc);
			sprintf(str, "OK;\r\n");
// 			Serial1.println(F("OK;"));
			/*
typedef struct wateringConfig{
	uint8_t	x:3;
	uint8_t y:5;
	uint8_t pgm_id:4;
	uint8_t airTime:2;
	uint8_t state:1;
	uint8_t enabled:1;
 	uint8_t flags;
	uint8_t	ml;
	uint16_t watered;
}wateringConfig;//6 bytes*/
			//pot set index,dev, pin,sensor_flags,noise, "name", x, y, airtime,en, flags, ml, pgm_id [, pgm params];
		}
		
	} else if (IS_P(cmd, PSTR("cfg"), 3)) {
		if (IS_P(cmd + 4, PSTR("get"), 3)) {
			printGcfg(str);
		} else if (IS_P(cmd + 4, PSTR("set"), 3)) {
			//print_field<uint16_t>(gcfg.config.enabled);
			char *ptr = cmd + 4 + 4;
			uint16_t val = g_cfg.config.flags;
			set_field<uint16_t>(val, &ptr);
			g_cfg.config.flags = val;
			g_cfg.config.enabled = val & 1;
			set_field<uint8_t>(g_cfg.config.pots_count, &ptr);
			set_field<uint16_t>(g_cfg.config.i2c_pwron_timeout, &ptr);
			set_field<uint16_t>(g_cfg.config.sensor_init_time, &ptr);
			set_field<uint16_t>(g_cfg.config.sensor_read_time, &ptr);
			set_field<uint16_t>(g_cfg.config.water_start_time, &ptr);
			set_field<uint16_t>(g_cfg.config.water_end_time, &ptr);
			set_field<uint16_t>(g_cfg.config.water_start_time_we, &ptr);
			set_field<uint16_t>(g_cfg.config.water_end_time_we, &ptr);
			set_field<uint8_t>(g_cfg.config.sensor_measures, &ptr);
			set_field<uint8_t>(g_cfg.config.test_interval, &ptr);	
			g_cfg.writeGlobalConfig();
			g_cfg.readGlobalConfig();
			printGcfg(str);
		}
	} else if (IS_P(cmd, PSTR("start"), 5)) {
		clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
		g_cfg.config.enabled = 1;
		g_cfg.writeGlobalConfig();
		Serial1.println(F("started;"));
	} else if (IS_P(cmd, PSTR("stop"), 4)) {
		g_cfg.config.enabled = 0;
		g_cfg.writeGlobalConfig();
 		Serial1.println(F("halted;"));
	} else if (IS_P(cmd, PSTR("restart"), 7)) {
		last_check_time = 0;
		clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
		clock.writeRAMbyte(LAST_CHECK_TS_1, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_2, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_3, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_4, 0);
		g_cfg.config.enabled = 1;
		g_cfg.writeGlobalConfig();
 		Serial1.println(F("restarted;"));
	} else if(IS_P(cmd, PSTR("force"), 5)) {
		clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
		wctl.doPotService(true);
	} else if(IS_P(cmd,PSTR("testall") ,7)) {
		clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
		wctl.doPotService(false);
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
		if (ch < 32) continue;
		if(ch == ';') {
			if ( i == 0 ) continue;
			cmdbuf[ i ] = 0;
// 			Serial1.print("cmd=[");
// 			Serial1.print(cmdbuf);
// 			Serial1.println("]");
			doCommand(cmdbuf);
			i = 0;
		} else {
			cmdbuf[i++] = ch;
		}
	}//while

}//sub

#ifdef MY_ROOM
void processPacket(char*cmd)
{
	Serial1.print(F("get cmd from esp ["));
	Serial1.print(cmd);
	Serial1.print(F("]"));
	char*ptr = cmd;
	//,link_id,len:
	while (!isdigit(*ptr)) ++ptr;
	uint8_t link_id = *ptr - '0';
// 	Serial1.print(F("link id:"));
// 	Serial1.println(link_id, DEC);
	while (*ptr && *ptr!=':') ++ptr;
	++ptr;
	if (!*ptr) {
// 		Serial1.println(F("ptr val is 0!"));
		return;
	}
	str[0] = 0;
	uint16_t len = 0;
	if (IS_P(ptr, PSTR("cfg"), 3)) {
		if (IS_P(ptr + 4, PSTR("set"), 3)) {
			
		} else if (IS_P(ptr + 4, PSTR("get"), 3)) {
// 			Serial1.println(F("cfg get cmd"));
			len = sprintf(str, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;\0",
							g_cfg.config.flags,
							g_cfg.config.pots_count,
							g_cfg.config.i2c_pwron_timeout,
							g_cfg.config.sensor_init_time,
							g_cfg.config.sensor_read_time,
		   
							g_cfg.config.water_start_time,
							g_cfg.config.water_end_time,
							g_cfg.config.water_start_time_we,
							g_cfg.config.water_end_time_we,
							g_cfg.config.sensor_measures,
							g_cfg.config.test_interval
				   );
		}
	} else if (IS_P(ptr, PSTR("ping"), 4)) {
// 		Serial1.println(F("ping cmd"));
		len = sprintf(str, "pong %s %s %s", VERSION_TYPE, __DATE__, __TIME__);
// 		esp8266.sendCmd(str, true, "SEND OK", 5000, true, true);
	} else if (IS_P(ptr, PSTR("pot"), 3)) {
		
	}
	
	if (str[0]) {
		char buf[24];
		sprintf(buf, "AT+CIPSEND=%d,%d\r\n", link_id, len);
		if (!esp8266.sendCmd(buf, true, ">", 4000, true)) {
// 			Serial1.println(F("ERROR: cnx fault"));
			return;
		} else {
// 			Serial1.println(F("cnx is OK"));
		}
		esp8266.sendCmd(str, true, "SEND OK", 5000, true);
	}
}
#endif

void setup()
{
	Serial1.begin(BT_BAUD);
 	Serial1.println(F("HELLO;"));
#ifdef MY_ROOM
 	esp8266.begin(38400, ESP_RST_PIN);
 	esp8266.setPacketParser(processPacket);
 	esp8266.connect();
#endif
	Wire.begin();
 	clock.begin();
// 	Serial1.println(freeRam(), DEC);
#ifdef MY_ROOM
 	lightMeter.begin();
	pinMode(PLANT_LIGHT_PIN, OUTPUT);
	digitalWrite(PLANT_LIGHT_PIN, LOW);
#endif

   	g_cfg.begin();
 	g_cfg.readGlobalConfig();
return;
// 	g_cfg.config.enabled = 0;
  	wctl.init(&i2cExpander);

 	water_doser.begin();
 	Serial1.print(getMemoryUsed(), DEC);
 	Serial1.print(F("/"));
 	Serial1.println(getFreeMemory(), DEC);
	Serial1.println(F("setup() end"));
// 	Serial1.println(freeRam(), DEC);
#ifdef MY_ROOM
	pinMode(AQUARIUM_PIN, OUTPUT);
#endif
  	last_check_time = ((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_1) << 24) | ((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_2) << 16) |((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_3) << 8) | (uint32_t)clock.readRAMbyte(LAST_CHECK_TS_4);
}

/**
	TODO: программные кнопки включения насосов. с парковкой соотв. дозатора в первую занятую дырку(чтоб не лило мимо).
*/

bool midnight_skip = false;
#ifdef MY_ROOM
uint32_t last_light_en = 0;
#endif
void loop()
{
	PORTE = PINE ^ (1<<2);
   	checkCommand();
	esp8266.process();
//  	return;
	DateTime now = clock.now();
	uint16_t now_m = now.hour() * 100 + now.minute();
	if (now_m > 2400 || now.year() < 2016) {
		now = esp8266.getTimeFromNTPServer();
		clock.adjust(now);
		print_now();
// 		Serial1.println(F("bad time read"));
		return;
	}
#ifdef MY_ROOM
	if (now_m > 900 && now_m < 2100) {
		digitalWrite(AQUARIUM_PIN, HIGH);
		if (millis()-last_light_en > 60000UL) {
			uint16_t lux = lightMeter.readLightLevel();
			pinMode(PLANT_LIGHT_PIN, OUTPUT);
			if (lux < g_cfg.config.lux_barrier_value) {
				digitalWrite(PLANT_LIGHT_PIN, HIGH);
				last_light_en = millis();
			} else {
				digitalWrite(PLANT_LIGHT_PIN, LOW);
			}
		}
	} else {
// 		Serial1.print(F("time now "));
// 		Serial1.print(now_m, DEC);
		digitalWrite(AQUARIUM_PIN, LOW);
		pinMode(PLANT_LIGHT_PIN, OUTPUT);
		digitalWrite(PLANT_LIGHT_PIN, LOW);
	}
#endif
	if ( g_cfg.config.enabled
			&& (
					(now.dayOfWeek() < 6 && now_m > g_cfg.config.water_start_time && now_m < g_cfg.config.water_end_time)
				||
					(now.dayOfWeek() >= 6 && now_m > g_cfg.config.water_start_time_we && now_m < g_cfg.config.water_end_time_we)
			) || iForceWatering) {
		midnight_skip = false;
  /* 		Serial1.print("times: now: ");
   		Serial1.print(now.secondstime(), DEC);
   		Serial1.print(" prev: ");
   		Serial1.print(last_check_time, DEC);
   		Serial1.print(" ");
  		Serial1.println(now.secondstime() - last_check_time, DEC);
*/
		if ((now.secondstime() - last_check_time > 30 * 60) || iForceWatering) {

 			wctl.doPotService(1);

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
		wctl.cleanDayStat();
// 		Serial1.println("g_cfg.midnight_tasks(); ended");
// 		Serial1.flush();
		midnight_skip = true;
		now = esp8266.getTimeFromNTPServer();
		clock.adjust(now);
	}
// 		- last_check_time > g_cfg.config.
//  	Serial1.println("ping");
// 	Serial1.println(freeMemory(), DEC);
	delay(100);
}
/*
cfg set 1,3,500,100,100,900,2100,1000,2100,3,30;

0	0	euc x1
0	1	баобаб
0	2	euc 34/12
0	3 	эвк. 35/14
0	4	эвк. 34/14
0	5	бао 34/5
0	6	мушмула 34/4
0	7	толстянка портулаковая 
0	8	эвкалипт "8"
1	0	длинный кактус(почти лысый)	34/6
1	1	34/13
1	2	34/15
1	3	34/3
1	4	34/10
1	5	эвк. чёрный круглый горшок
1	6	баобаб1
1	7	алоэ
1	8	альбиция(недофиолетовый)
2	0	34/7
2	1	ладанник
2	2	свободен
2	3	свободен -У
2	4   свободен -У
2	5	свободен -У
2	6	свободен -У
2	7	34/8
2	8	34/9

0,34,1,0,0,euc x1,2,0,1,30,1,670,500;
1,34,0,0,1,baobab,2,0,0,20,2,500,650,300;
2,34,12,0,2,euc 34/12,2,0,1,20,1,650,30;
3,35,14,0,3,euc 35/14,2,0,1,30,1,700,500;
4,34,14,0,4,euc 34/14,2,0,1,10,1,650,100;
5,34,5,0,5,baobab 34/5,2,0,1,10,1,630,50;
6,34,4,0,6,mushmula 34/4,2,0,1,10,1,604,50;
7,35,8,0,7,tolstanka,2,0,1,10,1,400,50;
8,35,9,0,8,euc"8",2,0,1,30,1,500,600;
9,34,6,1,0,cereus 34/6,2,0,1,10,2,550,600,60;
10,34,13,1,1,maklura 34/13,2,0,1,10,1,680,60;
11,34,15,1,2,euc.kust 34/15,2,0,1,40,1,730,600;
12,34,3,1,3,euc. 34/3,2,0,1,40,1,500,600;
13,34,10,1,4,euc 34/10,2,0,1,40,1,600,600;
14,35,13,1,5,euc in black pot ,2,0,1,30,1,600,600;
15,35,11,1,6,baobab 1,2,0,1,30,2,500,600,300;
16,35,15,1,7,aloe,2,0,1,20,2,460,500,80;
17,35,10,1,8,albicia,2,0,1,30,1,900,200;
18,34,7,2,0,euc 34/7,2,0,1,50,1,530,500;
19,34,11,2,1,ladannik,2,0,1,50,1,700,300;
20,34,8,2,7,maklura 34/8,2,0,1,10,1,650,60;
21,34,9,2,8,morkovka 34/9,2,0,1,10,1,650,60;
 */