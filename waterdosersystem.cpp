/**
 * Класс управления насосами, счётчиками жидкости и количеством налива жидкости.
 * управление позиционированием осуществляется отдельно(пока не реализовано)
 */
#include "Arduino.h"
#include "config_defines.h"
#include <Wire.h>
#include "configuration.h"
#include "waterdosersystem.h"
// #include "errlogger.h"
#include "waterstorage.h"

#define WFSM_NONE			(0)
#define WFSM_WATERCOUNTER 	(1)
#define WFSM_MOVE			(2)

#define STEPS				(12)

#define AIR_PIN				(22)
#define PUMP_PIN			(21)
#define WFS_PIN				(20)

#define X_AXE_DIR			26
#define X_AXE_DIR2			27

#define Y_AXE_DIR			28
#define Y_AXE_DIR2			29

#define X_AXE_EN			40
#define Y_AXE_EN			39

#define SERVO_PIN			(46)

#define X_BEGIN_PIN			23 // wrong: analog! PF6/D49
#define X_END_PIN			7 //analog! PF7/D50

#define Y_BEGIN_PIN			52 // digital!
#define Y_END_PIN			5 // analog! PF5/D48

#define X_STEP_PIN			(31)
#define Y_STEP_PIN			(30)


/*
 *
typedef struct WaterDoserSystemConfig{
	uint8_t flags;
	uint8_t pump_pin; // only 1..5 !!!
	uint8_t pump2_pin; // only 1..5 !!!
	uint8_t wfs_pin; // only 4..7 !!!
	uint8_t wfs2_pin; // only 4..7 !!!
	uint16_t wfs_ticks_per_liter;
	uint16_t wfs2_ticks_per_liter;
	uint8_t doser_move_pin;
	uint8_t doser_dir_pin;
	uint8_t servo_pin;
	uint8_t servo2_pin;
	uint8_t optocoupler_pin;
	uint8_t end_switch_pin;
} WaterDoserSystemConfig;//11bytes

 */
// {
//         WaterDoserSystemConfig default_wdsc[2] = {
// //              {0, 1, 3, 5, 7, 3900, 3900, MCP_PORTB + 3, MCP_PORTB + 2, 47, 49, MCP_PORTB + 0, MCP_PORTB + 1},
// // {0, 0, 2, 4, 6, 3900, 3900, MCP_PORTB + 4, MCP_PORTB + 5, 48, 50, MCP_PORTB + 7, MCP_PORTB + 6}
//                 {0, 7, 5, 1, 3, 3900, 3900, MCP_PORTB + 3, MCP_PORTB + 2, 47, 49, MCP_PORTB + 0, MCP_PORTB + 1},
//                 {0, 5, 7, 3, 1, 3900, 3900, MCP_PORTB + 4, MCP_PORTB + 5, 48, 50, MCP_PORTB + 7, MCP_PORTB + 6}
//         };

typedef enum WaterDoserSystemState{
	wdstIdle,
	wdstWalk,
	wdstWater,
	wdstError
}WaterDoserSystemState;

extern char*str;
extern Configuration g_cfg;
// extern ErrLogger logger;
extern I2CExpander i2cExpander;

static volatile uint16_t __wfs_flag = 0;

static volatile WaterDoserSystemState __wfs_run_mode = wdstIdle;

#include "i2cexpander.h"
extern "C" {
	#include "utility/twi.h"  // from Wire library, so we can do bus scanning
}


WaterDoserSystem::WaterDoserSystem(){
	expander_addr = 0;
	exp = NULL;
}

WaterDoserSystem::~WaterDoserSystem()
{
	cli();
		EIMSK ^= 0b10000000;//enable int7
	sei();
}

void WaterDoserSystem::dope(AirTime at)
{
	pinMode(AIR_PIN, OUTPUT);
	digitalWrite(AIR_PIN, HIGH);
	if (at == atShort) {
		delay(4000);
	} else if(at == atMedium){
		delay(8000);
	}else {
		delay(12000);
	}
	digitalWrite(AIR_PIN, LOW);
}

void WaterDoserSystem::begin(/*uint8_t _expander_addr, I2CExpander*_exp*/)
{
	pinMode(X_AXE_DIR, OUTPUT);
	pinMode(X_AXE_DIR2, OUTPUT);
	pinMode(X_AXE_EN, OUTPUT);
	pinMode(Y_AXE_DIR, OUTPUT);
	pinMode(Y_AXE_DIR2, OUTPUT);
	pinMode(Y_AXE_EN, OUTPUT);

	pinMode(X_BEGIN_PIN, INPUT);
	pinMode(Y_BEGIN_PIN, INPUT);
	
	pinMode(X_STEP_PIN, INPUT);
	pinMode(Y_STEP_PIN, INPUT);

	pinMode(AIR_PIN, OUTPUT);
	digitalWrite(AIR_PIN, LOW);
	
	digitalWrite(X_AXE_DIR, LOW);
	digitalWrite(X_AXE_DIR2, LOW);
	digitalWrite(X_AXE_EN, LOW);
	digitalWrite(Y_AXE_DIR, LOW);
	digitalWrite(Y_AXE_DIR2, LOW);
	digitalWrite(Y_AXE_EN, LOW);
	
	cur_x = 0xFF;
	cur_y = 0xFF;
	
	Serial1.println("tests ended");
	servoUp();
	delay(1000);
	servoUnbind();
    	parkX();
    	parkY();
 	this->pipi(1,8,10);
	this->pipi(1,8,10);
	this->pipi(1,8,10);
}

void WaterDoserSystem::servoUp()
{
	srv.attach(SERVO_PIN);
	srv.write(100);
}

void WaterDoserSystem::servoDown()
{
	srv.attach(SERVO_PIN);
	srv.write(0);
}

void WaterDoserSystem::servoUnbind()
{
	srv.detach();
}
	
bool WaterDoserSystem::nextX()
{
	if (digitalRead(X_STEP_PIN) == HIGH) {
		while (digitalRead(X_STEP_PIN) == HIGH) {
			fwdX();
		}
	}
	while (!digitalRead(X_STEP_PIN) && !digitalRead(X_STEP_PIN)) {
		fwdX();
	}
	stopX();
	if (digitalRead(X_STEP_PIN)) {
		if (cur_x != 0xFF) {
			++cur_x;
			return true;
		} else {
			cur_x = 0;
			return true;
		}
	}
	return false;
}

bool WaterDoserSystem::nextY()
{
	if (digitalRead(Y_STEP_PIN) == HIGH) {
		while (digitalRead(Y_STEP_PIN) == HIGH) {
			fwdY();
		}
	}
	while (!digitalRead(Y_STEP_PIN) && !digitalRead(Y_STEP_PIN)) {
		fwdY();
	}
	stopY();
	if (digitalRead(Y_STEP_PIN)) {
		if (cur_y != 0xFF) {
			++cur_y;
			return true;
		} else {
			cur_y = 0;
			return true;
		}
	}
	return false;
}

void WaterDoserSystem::stopX()
{
	digitalWrite(X_AXE_EN, LOW);
	digitalWrite(X_AXE_DIR, LOW);
	digitalWrite(X_AXE_DIR2, LOW);
}

void WaterDoserSystem::stopY()
{
	digitalWrite(Y_AXE_EN, LOW);
	digitalWrite(Y_AXE_DIR, LOW);
	digitalWrite(Y_AXE_DIR2, LOW);
}

void WaterDoserSystem::bwdX()
{
	digitalWrite(X_AXE_EN, HIGH);
	digitalWrite(X_AXE_DIR2, LOW);
	digitalWrite(X_AXE_DIR, HIGH);
}

void WaterDoserSystem::fwdX()
{
	digitalWrite(X_AXE_EN, HIGH);
	digitalWrite(X_AXE_DIR, LOW);
	digitalWrite(X_AXE_DIR2, HIGH);
}

void WaterDoserSystem::bwdY()
{
	digitalWrite(Y_AXE_EN, HIGH);
	digitalWrite(Y_AXE_DIR2, LOW);
	digitalWrite(Y_AXE_DIR, HIGH);
}

void WaterDoserSystem::fwdY()
{
	digitalWrite(Y_AXE_EN, HIGH);
	digitalWrite(Y_AXE_DIR, LOW);
	digitalWrite(Y_AXE_DIR2, HIGH);
}

uint16_t WaterDoserSystem::measure(uint16_t ml, uint16_t timeout)
{
 	uint16_t wftpl = 3900;
 	uint16_t ticks = round( (float)ml * (float)wftpl * 0.001)*2;
 	__wfs_run_mode = wdstWater;
// 	Serial1.println(__LINE__, DEC);
 	exp->begin(expander_addr);
// 	WaterStorages ws;
// 	ws.begin();
// 	uint8_t ws_index;
// 	WaterStorageData wsd = ws.readNonemptyStorage(ws_index);
// 	if (!wsd.enabled || ws_index == 255) {
// 		Serial1.println("No water at all");
// 		return 0;
// 	}

	pinMode(PUMP_PIN, OUTPUT);
	digitalWrite(PUMP_PIN, HIGH);
// 	DDRF |= (1<<6);

// 	PORTF |= (1<<6);
	pinMode(WFS_PIN, INPUT_PULLUP);
 	uint8_t prev = digitalRead(WFS_PIN), state=0;
 	uint32_t start_millis = millis();
	state = 0;
	__wfs_flag = 0;
	while ( (millis() - start_millis < timeout) && (__wfs_flag < ticks) ) {
		state = digitalRead(WFS_PIN);
		if ( state && !prev) {
			++__wfs_flag;
			start_millis = millis();
		}
		prev = state;
	}//while

	digitalWrite(PUMP_PIN, LOW);
	haltPumps();
Serial1.println("end of measure");
 	if ( (__wfs_flag < ticks) && (millis() - start_millis >= timeout) ) {
 		EXTRACT_STRING(str, s_doser_err1);
		Serial1.println(__LINE__, DEC);
//  		logger.addError(str);
 	}
	Serial1.println(__LINE__, DEC);
	Serial1.println("end of measure");
	Serial1.flush();
	Serial1.print("ticks:");
	Serial1.print(ticks, DEC);
	Serial1.print("flag:[");
	Serial1.print(__wfs_flag, DEC);
	Serial1.println("]");
	Serial1.flush();
 	uint16_t _ml = round(__wfs_flag * 1000.0/(float)wftpl);
// 	ws.dec(ws_index, ml);
 	return _ml;

}

void WaterDoserSystem::haltPumps()
{
	pinMode(PUMP_PIN, OUTPUT);
	digitalWrite(PUMP_PIN, LOW);
// 	WaterStorages ws;
// 	ws.begin();
// 
// 	for (int i = 0; i < ws.getStoragesCount(); ++i) {
// 		WaterStorageData wsd = ws.readStorageData(i);
// 		pinMode(wsd.pump_pin, OUTPUT);
// 		digitalWrite(wsd.pump_pin, LOW);
// 	}

 	Serial1.println("haltPumps");
}

void WaterDoserSystem::testES()
{
	Serial1.print("X begin=");
	Serial1.println(digitalRead(X_BEGIN_PIN), DEC);
	/*Serial1.print("X end=");
	Serial1.println(analogRead(X_END_PIN), DEC);
	Serial1.print("X stepper=");
	Serial1.println(digitalRead(X_STEP_PIN), DEC);

	Serial1.print("Y begin=");
	Serial1.println(digitalRead(Y_BEGIN_PIN), DEC);
	Serial1.print("Y end=");
	Serial1.println(analogRead(Y_END_PIN), DEC);
	Serial1.print("Y stepper=");
	Serial1.println(digitalRead(Y_STEP_PIN), DEC);*/
}

bool WaterDoserSystem::parkX()
{
	int val = 0;
	while (digitalRead(X_BEGIN_PIN)) {
		fwdX();
	}
	delay(500);
	stopX();
	val = analogRead(X_BEGIN_PIN);
	while (!digitalRead(X_BEGIN_PIN)) {
		bwdX();
	}
	stopX();
}

bool WaterDoserSystem::parkY()
{
	while(!digitalRead(Y_BEGIN_PIN)) {
		fwdY();
	}
	stopY();
	while (digitalRead(Y_BEGIN_PIN)) {
		bwdY();
	}
	stopY();
}

bool WaterDoserSystem::moveToPos(uint8_t x, uint8_t y)
{
	if (x < cur_x) {
		parkX();
		nextX();
	}
	if (y < cur_y) {
		parkY();
		nextY();
	}
	while(cur_x < x) {
		if (!nextX()) {
			Serial1.println("ERROR in X-pos");
			return false;
		}
	}
	while(cur_y < y) {
		if (!nextY()) {
			Serial1.println("ERROR in Y-pos");
			return false;
		}
	}
	return true;
}


uint16_t WaterDoserSystem::pipi(uint8_t x, uint8_t y, uint8_t ml, AirTime at)
{
	if(!this->moveToPos(x, y)) {
		Serial1.println("ERROR while positioning");
		return 0;
	}
	servoDown();
	pinMode(AIR_PIN, OUTPUT);
	digitalWrite(AIR_PIN, HIGH);
// 	this->init_measure();
	uint16_t ret_ml = this->measure(ml, 5000);
	Serial1.println(ret_ml, DEC);
	dope(at);
	servoUp();
	delay(500);
	servoUnbind();
	return ret_ml;
}

WaterDoserSystem water_doser;

