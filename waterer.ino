#include <Servo.h>
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
#include "wateringcontroller.h"
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
	Serial1.print("addr: ");
	Serial1.print(addr, DEC);
	if (addr == I2C_CLOCK_ADDRESS) {
		Serial1.println(" DS1307 clock");
	} else if ( (addr & 0xF8) == (0xF8 & I2C_MEMORY_ADDRESS) ) {
		Serial1.println(" memory chip");
	} else if ( (addr & 0xF8) == PCF8574_ADDRESS) {
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


void printGcfg()
{
	uint16_t val = (uint16_t)g_cfg.config.enabled;
	print_field<uint16_t>(val);
	val = (uint16_t)g_cfg.config.flags;
	print_field<uint16_t>(val);
	print_field<uint8_t>(g_cfg.config.pots_count);
	print_field<uint16_t>(g_cfg.config.i2c_pwron_timeout);
	print_field<uint16_t>(g_cfg.config.sensor_init_time);
	print_field<uint16_t>(g_cfg.config.sensor_read_time);
	print_field<uint16_t>(g_cfg.config.water_start_time);
	print_field<uint16_t>(g_cfg.config.water_start_time);
	print_field<uint16_t>(g_cfg.config.water_end_time_we);
	print_field<uint8_t>(g_cfg.config.sensor_measures, ';');
}

bool doCommand(char*cmd)
{
	static const char time_read_fmt[] /*PROGMEM*/ = "%d:%d:%d %d.%d.%d %d";
	if (IS(cmd, "+", 1)) {
		int pin = atoi(cmd+1);
		if(pin > 2) {
			pinMode(pin, OUTPUT);
			digitalWrite(pin, HIGH);
		}
	} else if(IS(cmd,"-",1)) {
		int pin = atoi(cmd+1);
		if(pin > 2) {
			pinMode(pin, OUTPUT);
			digitalWrite(pin, LOW);
		}		
	} else if (IS(cmd, "pipi", 4)) {
		uint8_t x=0xFF, y=0xFF, ml=0xFF;
		char*ptr = cmd+5;
		set_field<uint8_t>(x, &ptr);
		set_field<uint8_t>(y, &ptr);
		set_field<uint8_t>(ml, &ptr);
		water_doser.pipi(x,y,ml, atMedium);
	} else if (IS(cmd, "WSZ", 3)) {
		Serial1.print(WD_SIZE_X);
		Serial1.print(",");
		Serial1.println(WD_SIZE_Y);
	} else if(IS(cmd,"U", 1)) {
		water_doser.servoUp();
	}else if(IS(cmd,"D",1)) {
		water_doser.servoDown();
	} else if (IS("G",cmd,1)) {
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
		Serial1.print(" ");
		Serial1.print(y, DEC);
		water_doser.moveToPos(x, y);
	} else if (IS(cmd, "iic", 3)) {
		i2cExpander.i2c_on();
		if (IS(cmd + 4, "scan", 4)) {
			uint8_t addr = PCF8574_ADDRESS;
			while (i2cExpander.findNext(addr, PCF8574_ADDRESS+8, &addr)) {
				Serial1.print(addr,DEC);
				Serial1.print(",");
				++addr;
			}
			Serial1.println("0;");
		} else if (isdigit(*(cmd+4))) {
			int dev=-1,pin=-1;
			char *ptr = cmd + 4;
			set_field<int>(dev, &ptr);
			set_field<int>(pin, &ptr);
			Serial1.print(dev, DEC);
			Serial1.print(",");
			Serial1.print(pin, DEC);
			Serial1.print(",");
			Serial1.print(i2cExpander.read_pin(dev,pin), DEC);
			Serial1.println(";");
		}
		i2cExpander.i2c_off();
	} else if (IS("ping", cmd, 4)) {
		Serial1.println("pong");
		print_now();
		Serial1.println(__DATE__);
	} else if (IS("time", cmd, 4)) {
		if (IS(cmd+5, "get",3)) {
		} else if (IS(cmd+5,"set",3)) {
			//time set dd:dd:dd dd.d.dddd d
			int dow, d, m, y, h, mi, s;
			sscanf(cmd + 9, time_read_fmt,  &h, &mi, &s, &d, &m, &y, &dow);
			Serial1.println(y, DEC);
			Serial1.println(dow, DEC);
			DateTime td(y, m, d, h, mi, s, dow);
			clock.adjust(td);
			delay(1000);
		} else {
			return false;
		}
		print_now();
	} else if (IS("pot", cmd, 3)) {
		if (IS(cmd + 4, "get", 3)) {
			if (IS(cmd+4+4, "count", 5)) {
				Serial1.print(g_cfg.config.pots_count, DEC);
				Serial1.println(";");
			} else if (isdigit(*(cmd+4+4))) {
				int index = -1;
				char *ptr = cmd + 8;
				set_field<int>(index, &ptr);
				if (index >=0 && index < g_cfg.config.pots_count) {
					dumpPotConfig(index);
				} else {
					return false;
				}
			}
		} else if (IS(cmd + 4 ,"set", 3)) {
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
		
	} else if (IS("cfg", cmd, 3)) {
		if (IS(cmd+4,"get",3)) {
			/*typedef struct globalConfig{
	uint16_t enabled:1;
	uint16_t flags;
// 	uint8_t i2c_expanders_count;
	uint8_t pots_count;
// 	uint8_t dosers_count;
	uint16_t i2c_pwron_timeout;
	uint16_t sensor_init_time;
	uint16_t sensor_read_time;
	uint16_t water_start_time;
	uint16_t water_end_time;
	uint16_t water_start_time_we;
	uint16_t water_end_time_we;
	uint8_t sensor_measures;
// 	uint8_t lcd_highlight_value;
}globalConfig;//18 bytes
*/
			//print_field<uint16_t>(gcfg.config.enabled);
			printGcfg();
		} else if (IS(cmd+4, "set", 3)) {
			//print_field<uint16_t>(gcfg.config.enabled);
			char *ptr = cmd + 4 + 4;
			uint16_t val = g_cfg.config.flags;
			set_field<uint16_t>(val, &ptr);
			g_cfg.config.flags = val;
			set_field<uint8_t>(g_cfg.config.pots_count, &ptr);
			set_field<uint16_t>(g_cfg.config.i2c_pwron_timeout, &ptr);
			set_field<uint16_t>(g_cfg.config.sensor_init_time, &ptr);
			set_field<uint16_t>(g_cfg.config.sensor_read_time, &ptr);
			set_field<uint16_t>(g_cfg.config.water_start_time, &ptr);
			set_field<uint16_t>(g_cfg.config.water_end_time, &ptr);
			set_field<uint16_t>(g_cfg.config.water_start_time_we, &ptr);
			set_field<uint16_t>(g_cfg.config.water_end_time_we, &ptr);
			set_field<uint8_t>(g_cfg.config.sensor_measures, &ptr);
			g_cfg.writeGlobalConfig();
			printGcfg();
			g_cfg.readGlobalConfig();
			printGcfg();
		}
	} else if (IS("start", cmd, 5)) {
		g_cfg.config.enabled = 1;
		g_cfg.writeGlobalConfig();
		Serial1.println("started");
	} else if (IS("stop", cmd, 4)) {
		g_cfg.config.enabled = 0;
		g_cfg.writeGlobalConfig();
		Serial1.println("halted");
	} else if (IS("restart", cmd, 7)) {
		last_check_time = 0;
		clock.writeRAMbyte(LAST_CHECK_TS_1, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_2, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_3, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_4, 0);
		g_cfg.config.enabled = 1;
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
		if (ch ==10 || ch ==13 ||ch == ';') {
			if ( i == 0 ) continue;
			cmdbuf[ i ] = 0;
			Serial1.print("cmd=[");
			Serial1.print(cmdbuf);
			Serial1.println("]");
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
	for (uint8_t i = 8; i <0x3F; ++i) {
		clock.writeRAMbyte(i, 0xAA);
		delay(10);
		if (clock.readRAMbyte(i)!=0xAA) {
			Serial1.print("error AA at ");
			Serial1.println(i);
		}
		clock.writeRAMbyte(i, 0x55);
		delay(10);
		if (clock.readRAMbyte(i)!=0x55) {
			Serial1.print("error 55 at ");
			Serial1.println(i);
		}
	}
  	g_cfg.begin();
	g_cfg.readGlobalConfig();
   	water_doser.begin();
 	wctl.init(&i2cExpander);
 	last_check_time = ((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_1) << 24) | ((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_2) << 16) |((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_3) << 8) | (uint32_t)clock.readRAMbyte(LAST_CHECK_TS_4);
}

/**
	TODO: программные кнопки включения насосов. с парковкой соотв. дозатора в первую занятую дырку(чтоб не лило мимо).
*/

bool midnight_skip = false;

void loop()
{

	PORTE = PINE ^ (1<<2);
	
	checkCommand();
	return;
	DateTime now = clock.now();
	uint16_t now_m = now.hour() * 60 + now.minute();

	if ( g_cfg.config.enabled
			&& (
					(now.dayOfWeek() < 6 && now_m > g_cfg.config.water_start_time && now_m < g_cfg.config.water_end_time)
				||
					(now.dayOfWeek() >= 6 && now_m > g_cfg.config.water_start_time_we && now_m < g_cfg.config.water_end_time_we)
			) || iForceWatering) {
		midnight_skip = true;
   		Serial1.print("times: now: ");
   		Serial1.print(now.secondstime(), DEC);
   		Serial1.print(" prev: ");
   		Serial1.print(last_check_time, DEC);
   		Serial1.print(" ");
  		Serial1.println(now.secondstime() - last_check_time, DEC);

		if ((now.secondstime() - last_check_time > 30 * 60) || iForceWatering) {

 			wctl.doPotService(iForceWatering != 1);

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
// 		- last_check_time > g_cfg.config.
//  	Serial1.println("ping");
// 	Serial1.println(freeMemory(), DEC);
	delay(100);
}
