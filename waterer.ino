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
#define LEDS_ADDR			39

#include <Wire.h>
#include <avr/wdt.h>

#include "RTClib.h"
#include <AT24Cxxx.h>
#include "freemem.h"
#include <math.h>
#include <PCF8574.h>
#include <Servo.h>
#include <OneWire.h>
#ifdef MY_ROOM
	#include <BH1750.h>
#endif

#include <MCP23017.h>
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
#include "wizard.h"
extern "C" {
#include "utility/twi.h"  // from Wire library, so we can do bus scanning
}

#define BT_BAUD		38400
/** ************************************************************************************************
 * 			GLOBAL VARIABLES
***************************************************************************************************/

#ifdef MY_ROOM
	OneWire ds(12);
	//28 8F 46 78 6 0 0 C0
	uint8_t addr_out[8] = {0x28,0x8f,0x46,0x78,0x6,0x0,0,0xc0}, addr_in[8] = {0x28,0x1C,0x63,0x78,0x6,0,0,0x02};
// #define USE_ESP8266
// 	BH1750 lightMeter;
// 	LiquidCrystal595Rus lcd();
#endif


ESP8266 esp8266(&Serial);
	
Servo ZS;
volatile uint32_t last_check_time = 0;

RTC_DS1307 clock;

AT24Cxxx mem(I2C_MEMORY_ADDRESS);

// #ifdef MY_ROOM
	MCP23017 leds;
// #endif
	
extern Configuration g_cfg;

// ShiftOut hc595(PUMP_HC595_LATCH, PUMP_HC595_CLOCK, PUMP_HC595_DATA, 1);

extern WaterDoserSystem water_doser;

I2CExpander i2cExpander;

WateringController wctl;

int8_t iForceWatering = 0;

// ErrLogger logger;
extern char*str;

// void run_wizard(uint8_t show_hello=1);

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

void print_now(HardwareSerial* output)
{
	char*week_days[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
	DateTime ts = clock.now();
	output->print(ts.day(), DEC);
	output->print(F("."));
	output->print(ts.month(), DEC);
	output->print(F("."));
	output->print(ts.year(), DEC);
	output->print(F(" "));
	output->print(week_days[ts.dayOfWeek()-1]);
	output->print(F(" "));
	output->print(ts.hour(), DEC);
	output->print(F(":"));
	output->print(ts.minute(), DEC);
	output->print(F(":"));
	output->print(ts.second(), DEC);
	output->println(';');
}

void dumpPotConfig(uint8_t index, HardwareSerial* output)
{
	//pot set <index>,<flags>,<dev>,<pin>,<x>,<y>,<name>,<airTime>,<state>,<enabled>,<ml>,<pgm>,<param1>[,param2][,param3]...;
	potConfig pot = g_cfg.readPot(index);
	print_field<uint8_t>(output, index);
// 	print_field<uint8_t>(pot.sensor.flags);
	print_field<uint8_t>(output, pot.sensor.dev);
	print_field<uint8_t>(output, pot.sensor.pin);
 	print_field<uint8_t>(output, (uint8_t)pot.wc.x);
 	print_field<uint8_t>(output, (uint8_t)pot.wc.y);
	output->print(pot.name);
	output->print(',');
	print_field<uint8_t>(output, pot.wc.airTime);
	print_field<uint8_t>(output, pot.wc.state);
	print_field<uint8_t>(output, pot.wc.enabled);
	print_field<uint8_t>(output, pot.wc.ml);
	print_field<uint8_t>(output, pot.wc.pgm);
	if (pot.wc.pgm == 1) {
		print_field<uint16_t>(output, pot.pgm.const_hum.value);
		print_field<uint16_t>(output, pot.pgm.const_hum.max_ml,';');
	} else if (pot.wc.pgm == 2) {
		print_field<uint16_t>(output, pot.pgm.hum_and_dry.min_value);
		print_field<uint16_t>(output, pot.pgm.hum_and_dry.max_value);
		print_field<uint16_t>(output, pot.pgm.hum_and_dry.max_ml,';');
	} else {
		output->print(F("???"));
	}
}

uint16_t last_temp_read = 0;

float readTemp(uint8_t*addr)
{
	uint8_t data[9];
	ds.reset();
	ds.select(addr);
	ds.write(0x44);
	delay(1000);
	ds.reset();
	ds.select(addr);    
	ds.write(0xBE);
	for (uint8_t i = 0; i < 9; i++) {           // we need 9 bytes
		data[i] = ds.read();
		Serial1.print(data[i], HEX);
		Serial1.print(" ");
	}
	
	int16_t raw = (data[1] << 8) | data[0];
	if (raw == 0xffff) {
		Serial1.println(F("Error while read device"));
		return -255.;
	}
	byte cfg = (data[4] & 0x60);
	// at lower res, the low bits are undefined, so let's zero them
	if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
	else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
	else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
	//// default is 12 bit resolution, 750 ms conversion time
	return (float)raw / 16.0;
}

void readDS18B20(bool send=true)
{
	char udp[16], buf[24];
	float celsius = readTemp(addr_out);
	Serial1.print(F("temp outside:"));
	Serial1.println(celsius);
	if (send && (g_cfg.config.esp_en)) {
		esp8266.sendCmd_P(PSTR("AT+CIPCLOSE=3"), true, s_OK, 1000);
		if (esp8266.sendCmd_P(PSTR("AT+CIPSTART=3,\"UDP\",\"192.168.42.1\",55455"), true, s_OK, 3000)) {
			int len = sprintf(udp, "TO=%d\0", round(celsius*100)); 
			sprintf(buf, "AT+CIPSEND=3,%d", len);
// 			Serial.print(udp);
// 			Serial.write(0);
			esp8266.sendCmd(buf, true, ">", 4000);
			esp8266.sendCmd(udp, true, "OK", 4000);
		}
	}
	celsius = readTemp(addr_in);
	Serial1.print(F("temp indoor:"));
	Serial1.println(celsius);
	if (send && (g_cfg.config.esp_en)) {
		esp8266.sendCmd_P(PSTR("AT+CIPCLOSE=3"), true, s_OK, 1000);
		if (esp8266.sendCmd_P(PSTR("AT+CIPSTART=3,\"UDP\",\"192.168.42.1\",55455"), true, s_OK, 3000)) {
			int len = sprintf(udp, "TI=%d\0", round(celsius*100)); 
			sprintf(buf, "AT+CIPSEND=3,%d", len);
// 			Serial.print(udp);
// 			Serial.write(0);
			esp8266.sendCmd(buf, true, ">", 4000);
			esp8266.sendCmd(udp, true, "OK", 4000);
		}
	}
}

void printGcfg(HardwareSerial*output)
{
	uint16_t val = (uint16_t)g_cfg.config.enabled;
// 	print_field<uint16_t>(val);
	val = (uint16_t)g_cfg.config.flags;
	print_field<uint16_t>(output, val);
	val = (uint16_t)g_cfg.config.esp_en;
	print_field<uint16_t>(output, val);
	print_field<uint8_t>(output, g_cfg.config.pots_count);
	print_field<uint16_t>(output, g_cfg.config.i2c_pwron_timeout);
	print_field<uint16_t>(output, g_cfg.config.sensor_init_time);
	print_field<uint16_t>(output, g_cfg.config.sensor_read_time);
	print_field<uint16_t>(output, g_cfg.config.water_start_time);
	print_field<uint16_t>(output, g_cfg.config.water_end_time);
	print_field<uint16_t>(output, g_cfg.config.water_start_time_we);
	print_field<uint16_t>(output, g_cfg.config.water_end_time_we);
	print_field<uint8_t>(output, g_cfg.config.sensor_measures);
	print_field<uint8_t>(output, g_cfg.config.test_interval, ';');
}

bool doCommand(char*cmd, HardwareSerial*output)
{
	static const char time_read_fmt[] /*PROGMEM*/ = "%d:%d:%d %d.%d.%d %d";
	static int pos = 180;
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
	if (IS_P(cmd, PSTR("temp?"), 5)) {
		readDS18B20(false);
	} else if (IS_P(cmd, PSTR("sdump"), 5)) {
// 		wctl.run_checks();
		wctl.dumpSensorValues(output);
	} else if (IS_P(cmd, PSTR("pcf"), 3)) {
		char*ptr = cmd + 4;
		int c,p,v;
		set_field<int>(c, &ptr);
		set_field<int>(p, &ptr);
		set_field<int>(v, &ptr);
		output->print(F("pcf set "));
		output->print(c, DEC);
		output->print(F("/"));
		output->print(p, DEC);
		output->print(F("="));
		output->print(v, DEC);
		i2cExpander.i2c_on();
		i2cExpander.begin(c);
		i2cExpander.write(p,v);
	} else if (IS_P(cmd, PSTR("stat"), 4)) {
		if (IS_P(cmd+5, PSTR("clr"), 3)) {
			wctl.cleanDayStat();
		} else if (IS_P(cmd + 5, PSTR("get"), 3)) {
			wctl.printDayStat(output);
		}
	} else if (IS_P(cmd, PSTR("lux"), 3)) {
#ifdef MY_ROOM
		if (cmd[4]=='?') {
			output->print(F("lux:"));
// 			output->println(lightMeter.readLightLevel(), DEC);
		} else if (cmd[3] == '=') {
			g_cfg.config.lux_barrier_value = atoi(cmd+5);
			g_cfg.writeGlobalConfig();
			g_cfg.readGlobalConfig();
			output->print(F("new lux value="));
			output->println(g_cfg.config.lux_barrier_value, DEC);
		}
#endif
	} else if (IS_P(cmd, PSTR("A"), 1)) {
		output->println(analogRead(cmd[1]-'0'), DEC);
	} else if (cmd[0] == 'd') {
		pinMode(atoi(cmd+1), INPUT);
		delay(20);
		output->print(atoi(cmd+1), DEC);
		output->print(":");
		output->println(digitalRead(atoi(cmd + 1)), DEC);
	} else if (IS_P(cmd, PSTR("+"), 1)) {
		int pin = atoi(cmd+1);
		if(pin > 2) {
			output->print(F("HIGH pin:"));
			output->println(pin, DEC);
			pinMode(pin, OUTPUT);
			digitalWrite(pin, HIGH);
			delay(1000);
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
		output->print(WD_SIZE_X);
		output->print(F(","));
		output->print(WD_SIZE_Y);
		output->println(';');
	} else if (cmd[0]== 'U') {
		water_doser.servoUp();
	} else if (cmd[0]=='D') {
		if (cmd[1] == 'F') {
		 water_doser.servoDown();
		} else if(cmd[1] == 'M') {
			pos -= 10;
			water_doser.servoMove(pos);
		} else if(cmd[1] == 'R') {
			pos = 180;
			water_doser.servoMove(pos);
		}
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
		output->print(x, DEC);
		output->print(F(" "));
		output->print(y, DEC);
		water_doser.moveToPos(x, y);
	} else if (IS_P(cmd, PSTR("iic"), 3)) {
		i2cExpander.i2c_on();
		if (IS_P(cmd+4, PSTR("off"), 3)) {
			i2cExpander.i2c_off();
		} else if (IS_P(cmd + 4, PSTR("scan"), 4)) {
			uint8_t addr = 32;
			while (i2cExpander.findNext(addr, 32+7, &addr)) {
				output->print(addr,DEC);
				output->print(F(","));
				++addr;
			}
			addr = 56;
			while (i2cExpander.findNext(addr, 56+7, &addr)) {
				output->print(addr,DEC);
				output->print(F(","));
				++addr;
			}
			output->println(F("0;"));
			i2cExpander.i2c_off();
		} else if (isdigit(*(cmd+4))) {
			int dev=-1,pin=-1;
			char *ptr = cmd + 4;
			set_field<int>(dev, &ptr);
			set_field<int>(pin, &ptr);
			output->print(dev, DEC);
			output->print(F(","));
			output->print(pin, DEC);
			output->print(F(","));
			output->print(i2cExpander.read_pin(dev,pin), DEC);
			output->println(F(";"));
			i2cExpander.i2c_off();
		}
	} else if (IS_P(cmd, PSTR("ping"), 4)) {
		output->println(F("pong;"));
		Serial1.println(F("pong;"));
		print_now(output);
#ifdef MY_ROOM
		output->println(F("MY_ROOM ver"));
#else
		output->println(F("BIG_ROOM ver"));
#endif
		output->print(__DATE__);
		output->println(F(" "));
		output->print(__TIME__);
		output->println(F(";"));
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
		}
		else if ( (g_cfg.config.esp_en) && IS_P(cmd+5,PSTR("adj"), 3)) {
			DateTime now = esp8266.getTimeFromNTPServer();
			clock.adjust(now);
			print_now(output);
		}
		else {
			return false;
		}
		print_now(output);
	} else if (IS_P(cmd, PSTR("pot"), 3)) {
		if (IS_P(cmd + 4, PSTR("find"), 4)) {
			int chip=-1, pin=-1;
			char*ptr=cmd+4+5;
			set_field<int>(chip, &ptr);
			set_field<int>(pin, &ptr);
			if (chip > 0 && pin > -1) {
				for (uint8_t i = 0; i < g_cfg.config.pots_count; ++i ) {
					potConfig pc = g_cfg.readPot(i);
					if (pc.sensor.dev == chip && pc.sensor.pin == pin) {
						dumpPotConfig(i, output);
						pin = 33;
						break;
					}
				}
				if(pin != 33) {
					output->println(F("not found;"));
				}
			}
		} else if (IS_P(cmd + 4, PSTR("get"), 3)) {
			if (IS_P(cmd+4+4, PSTR("count"), 5)) {
				output->print(g_cfg.config.pots_count, DEC);
				output->println(";");
			} else if(IS_P(cmd+4+4, PSTR("all"), 3)) {
				for (uint8_t i = 0; i < g_cfg.config.pots_count; ++i ) {
					dumpPotConfig(i, output);
					output->println();
				}
			} else if (isdigit(*(cmd+4+4))) {
				int index = -1;
				char *ptr = cmd + 8;
				set_field<int>(index, &ptr);
				if (index >=0 && index < g_cfg.config.pots_count) {
					dumpPotConfig(index, output);
					output->println();
				} else {
					return false;
				}
			}
		} else if (IS_P(cmd + 4 , PSTR("set"), 3)) {
			if (IS_P(cmd + 8, PSTR("count"), 5)) {
				uint8_t tmp;
				char*ptr=cmd+8+6;
				set_field<uint8_t>(tmp, &ptr);
				g_cfg.config.pots_count = tmp;
				g_cfg.writeGlobalConfig();
				output->println(F("OK;"));
				return true;
			}
			uint8_t tmp, index;
			char*ptr=cmd+8;
			set_field<uint8_t>(index, &ptr);
			Wizard w;
			w.edit_pot(index);
		}
		
	} else if (IS_P(cmd, PSTR("cfg"), 3)) {
		if (IS_P(cmd + 4, PSTR("get"), 3)) {
			printGcfg(output);
		} else if (IS_P(cmd + 4, PSTR("set"), 3)) {
			Wizard w;
			w.cfg_run();
		}
	} else if (IS_P(cmd, PSTR("start"), 5)) {
		clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
		g_cfg.config.enabled = 1;
		g_cfg.writeGlobalConfig();
		output->println(F("started;"));
	} else if (IS_P(cmd, PSTR("stop"), 4)) {
		g_cfg.config.enabled = 0;
		g_cfg.writeGlobalConfig();
 		output->println(F("halted;"));
	} else if (IS_P(cmd, PSTR("restart"), 7)) {
		last_check_time = 0;
		clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
		clock.writeRAMbyte(LAST_CHECK_TS_1, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_2, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_3, 0);
		clock.writeRAMbyte(LAST_CHECK_TS_4, 0);
		g_cfg.config.enabled = 1;
		g_cfg.writeGlobalConfig();
 		output->println(F("restarted;"));
	} else if(IS_P(cmd, PSTR("force"), 5)) {
		clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
		wctl.doPotService(true, output);
	} else if(IS_P(cmd,PSTR("testall") ,7)) {
		clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
		wctl.doPotService(false, output);
	} else if (IS_P(cmd, PSTR("wizard"), 6)) {
		Wizard w;
		w.run();
	}
	return true;
}


static char cmdbuf[80]={0};

void checkCommand()
{
	if (!Serial1.available()) return;
	static int i = 0;
	static char /*cmdbuf[64]={0},*/ ch;
	while (Serial1.available()) {
		ch = Serial1.read();
		if (ch < 32) continue;
		if(ch == ';') {
			if ( i == 0 ) continue;
			cmdbuf[ i ] = 0;
// 			Serial1.print("cmd=[");
// 			Serial1.print(cmdbuf);
// 			Serial1.println("]");
			doCommand(cmdbuf, &Serial1);
			i = 0;
		} else {
			cmdbuf[i++] = ch;
		}
	}//while

}//sub

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
	memset(cmdbuf, 0, sizeof(cmdbuf));
	strncpy(cmdbuf, ptr, min(strlen(ptr), sizeof(cmdbuf)) );
	Serial1.print(F("get cmd ["));
	Serial1.print(cmdbuf);
	Serial1.println(F("]"));
	sprintf(str, "AT+CIPSENDEX=%d,2020\r\n", link_id);
		if (!esp8266.sendCmd(str, true, ">", 4000, true)) {
 			Serial1.println(F("ERROR: cnx fault"));
			return;
		} else {
			doCommand(cmdbuf, &Serial);
			Serial.print("\\0");
			Serial.write(0);
			esp8266.sendCmd_P(PSTR(""), true, PSTR("SEND OK"), 4000);
// 			sprintf(str, "AT+CIPCLOSE=%d\r\n", link_id);
// 			esp8266.sendCmd(str, true, "OK", 4000, true);
		}
}


void setup()
{
	wdt_disable();
	Serial1.begin(BT_BAUD);
 	Serial1.println(F("HELLO;"));
	
 	Wire.begin();
  	clock.begin();
   	
	g_cfg.begin();
 	g_cfg.readGlobalConfig();
	
 	water_doser.begin();
// 	water_doser.testAll();
// #ifdef USE_LEDS
#if 1
	leds.begin(LEDS_ADDR);
	for (uint8_t i = 0; i < 16; ++i) {
		leds.pinMode(i, OUTPUT);
		leds.digitalWrite(i, LOW);
	}
	leds.pinMode(VBTN_WATERTEST, INPUT);
	leds.pullUp(VBTN_WATERTEST, true);
	leds.pinMode(VBTN_WEN, INPUT);
	leds.pullUp(VBTN_WEN, true);
	
	leds.digitalWrite(LED_RED, HIGH);
	leds.digitalWrite(LED_GREEN, HIGH);
	leds.digitalWrite(LED_BLUE, HIGH);
	leds.digitalWrite(LED_YELLOW, HIGH);
	delay(400);
	leds.digitalWrite(LED_RED, LOW);
	delay(400);
	leds.digitalWrite(LED_GREEN, LOW);
	delay(400);
	leds.digitalWrite(LED_BLUE, LOW);
	delay(400);
	leds.digitalWrite(LED_YELLOW, LOW);	
#endif


 	esp8266.setPacketParser(processPacket);
	if (g_cfg.config.esp_en) {
		Serial1.println(F("Init ESP8266..."));
		esp8266.begin(38400, ESP_RST_PIN);
		esp8266.connect();
		Serial1.println(F("Init ESP8266...[done]"));
	}
// 	Wire.begin();
//  	clock.begin();
	//leds.writeGPIOAB(0xFFFF);
// 	Serial1.println(freeRam(), DEC);
#ifdef MY_ROOM
//  	lightMeter.begin();
// 	pinMode(PLANT_LIGHT_PIN, OUTPUT);
// 	digitalWrite(PLANT_LIGHT_PIN, LOW);
#endif

	wctl.init(&i2cExpander);


 	Serial1.print(getMemoryUsed(), DEC);
 	Serial1.print(F("/"));
 	Serial1.println(getFreeMemory(), DEC);
// 	Serial1.println(freeRam(), DEC);
#ifdef MY_ROOM
	pinMode(AQUARIUM_PIN, OUTPUT);
	digitalWrite(AQUARIUM_PIN, HIGH);
#endif
  	last_check_time = ((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_1) << 24) | ((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_2) << 16) |((uint32_t)clock.readRAMbyte(LAST_CHECK_TS_3) << 8) | (uint32_t)clock.readRAMbyte(LAST_CHECK_TS_4);
	DateTime lct(last_check_time);
	Serial1.print(F("last check time: "));
	Serial1.print(lct.hour(), DEC);
	Serial1.print(":");
	Serial1.print(lct.minute(), DEC);
	Serial1.print(":");
	Serial1.println(lct.second(), DEC);
// 	leds.writeGPIOAB(0xFFFF);
	uint8_t dc = wctl.getStatDay();
	DateTime now = clock.now();
	if (now.day() != dc) {
		Serial1.println(F("clean old stat"));
		wctl.cleanDayStat();
		wctl.setStatDay(now.day());
		clock.writeRAMbyte(RAM_CUR_STATE, CUR_STATE_IDLE);
	}
	uint8_t addr[8];
	while ( ds.search(addr)) {
		Serial1.print("ROM =");
		for(int i = 0; i < 8; i++) {
			Serial1.write(' ');
			Serial1.print(addr[i], HEX);
		}
		Serial1.println();
	}
     Serial1.println("No more addresses.");
//     Serial1.println();
    ds.reset_search();
//     delay(250);
	Serial1.println(F("setup() end"));
}


/**
	TODO: программные кнопки включения насосов. с парковкой соотв. дозатора в первую занятую дырку(чтоб не лило мимо).
*/

bool midnight_skip = false;
#ifdef MY_ROOM
uint32_t last_light_en = 0;
#endif
uint8_t _pulse_state = 1;

bool checkContinue()
{
#ifdef MY_ROOM
	if (leds.digitalRead(VBTN_WEN) == LOW) {
		Serial1.println("TRIG EN");
		g_cfg.config.enabled = 1 - g_cfg.config.enabled;
		g_cfg.writeGlobalConfig();
		leds.digitalWrite(LED_YELLOW, g_cfg.config.enabled);
		delay(1000);
	}
//  	leds.digitalWrite(LED_YELLOW, g_cfg.config.enabled);
#endif
	return true;//g_cfg.config.enabled;
}


void loop()
{
// 	return;
#ifdef MY_ROOM
	if (leds.digitalRead(VBTN_WATERTEST) == LOW) {
		pinMode(PUMP_PIN, OUTPUT);
		digitalWrite(PUMP_PIN, HIGH);
		pinMode(VCC_PUMP_EN, OUTPUT);
		digitalWrite(VCC_PUMP_EN, HIGH);
		delay(5000);
		digitalWrite(PUMP_PIN, LOW);
		digitalWrite(VCC_PUMP_EN, LOW);
		leds.digitalWrite(LED_BLUE, LOW);
	}
// 	leds.digitalWrite(0, _pulse_state);
#endif
	_pulse_state = 1 - _pulse_state;
   	checkCommand();
	delay(500);
// 	return;
	if (g_cfg.config.esp_en) {
		esp8266.process();
	}

	DateTime now = clock.now();
	uint16_t now_m = now.hour() * 100 + now.minute();
	if (now.minute()%5 == 0 && now_m!=last_temp_read) {
		readDS18B20();
		last_temp_read = now_m;
	}
	
	if (now_m > 2400 || now.year() < 2016) {
		if (g_cfg.config.esp_en) {
			now = esp8266.getTimeFromNTPServer();
			clock.adjust(now);
			print_now(&Serial1);
		}
 		Serial1.println(F("bad time read"));
		return;
	}
	checkContinue();
#ifdef MY_ROOM
	if (now_m > 900 && now_m < 2100) {
		leds.digitalWrite(LED_GREEN, _pulse_state);
		leds.digitalWrite(LED_YELLOW, g_cfg.config.enabled);
		pinMode(AQUARIUM_PIN, OUTPUT);
		digitalWrite(AQUARIUM_PIN, HIGH);
		if (millis()-last_light_en > 60000UL) {
			uint16_t lux = 16000;// lightMeter.readLightLevel();
// 			pinMode(PLANT_LIGHT_PIN, OUTPUT);
			if (lux < g_cfg.config.lux_barrier_value) {
// 				digitalWrite(PLANT_LIGHT_PIN, HIGH);
				last_light_en = millis();
			} else {
// 				digitalWrite(PLANT_LIGHT_PIN, LOW);
			}
		}
	} else {
//  		Serial1.print(F("time now "));
//  		Serial1.print(now_m, DEC);
		leds.digitalWrite(LED_GREEN, LOW);
		pinMode(AQUARIUM_PIN, OUTPUT);
		digitalWrite(AQUARIUM_PIN, LOW);
// 		pinMode(PLANT_LIGHT_PIN, OUTPUT);
// 		digitalWrite(PLANT_LIGHT_PIN, LOW);
		leds.digitalWrite(LED_YELLOW, 0);
		leds.digitalWrite(LED_BLUE, 0);
	}
#endif
	if ( g_cfg.config.enabled
			&& (
					(now.dayOfWeek() < 6 && now_m > g_cfg.config.water_start_time && now_m < g_cfg.config.water_end_time)
				||
					(now.dayOfWeek() >= 6 && now_m > g_cfg.config.water_start_time_we && now_m < g_cfg.config.water_end_time_we)
			) || iForceWatering) {
		midnight_skip = false;
		
		if ( (now.secondstime() - last_check_time) % 60 == 0) {
			Serial1.print(F("next check after "));
			Serial1.print(g_cfg.config.test_interval - (now.secondstime() - last_check_time)/60, DEC);
			Serial1.println(F(" minutes"));
		}
		
		if ( ((now.secondstime() - last_check_time) / 60 >= g_cfg.config.test_interval) || iForceWatering) {
 			wctl.doPotService(1, &Serial1);
			now = clock.now();
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
		wctl.cleanDayStat();
		wctl.setStatDay(now.day());

// 		Serial1.println("g_cfg.midnight_tasks(); ended");
// 		Serial1.flush();
		midnight_skip = true;
		if (g_cfg.config.esp_en) {
			now = esp8266.getTimeFromNTPServer();
			clock.adjust(now);
		}
	}
// 		- last_check_time > g_cfg.config.
//  	Serial1.println("ping");
// 	Serial1.println(freeMemory(), DEC);
	delay(100);
}