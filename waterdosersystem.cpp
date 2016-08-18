/**
 * Класс управления насосами, счётчиками жидкости и количеством налива жидкости.
 * управление позиционированием осуществляется отдельно(пока не реализовано)
 */
#include "Arduino.h"
#include "config_defines.h"
#include <Wire.h>
#include "configuration.h"
#include "waterdosersystem.h"
 #include "pins.h"
#include "waterstorage.h"

#define WFSM_NONE			(0)
#define WFSM_WATERCOUNTER 	(1)
#define WFSM_MOVE			(2)
#define X_BEGIN_STOPVAL 200



#define WDERR_STICKED		1
#define WDERR_WFS_DEAD		2
#define WDERR_POS_ERR		3

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

extern bool checkContinue();

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
	uint8_t n = 0;
	if (at == atShort) {
		n = 4 * 2;
	} else if(at == atMedium){
		n = 8 * 2;
	}else {
		n = 6 * 2;
	}
	for (uint8_t i = 0; i < n; ++i) {
		if (!checkContinue()) {
			break;
		}
		delay(500);
	}
	digitalWrite(AIR_PIN, LOW);
}

void WaterDoserSystem::begin(/*uint8_t _expander_addr, I2CExpander*_exp*/)
{
	errcode = 0;
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

	pinMode(VCC_PUMP_EN, OUTPUT);
	digitalWrite(VCC_PUMP_EN, LOW);
	
	digitalWrite(X_AXE_DIR, LOW);
	digitalWrite(X_AXE_DIR2, LOW);
	digitalWrite(X_AXE_EN, LOW);
	digitalWrite(Y_AXE_DIR, LOW);
	digitalWrite(Y_AXE_DIR2, LOW);
	digitalWrite(Y_AXE_EN, LOW);
#ifndef MY_ROOM
//  	Serial1.println(analogRead(6), DEC);
#endif
	cur_x = 0xFF;
	cur_y = 0xFF;
	pinMode(Z_AXE_DIR, OUTPUT);
	pinMode(Z_AXE_EN, OUTPUT);

// #ifdef MY_ROOM
  	servoDown();
  	servoUp();
  	park();
// 	moveToPos(0,0);
// #else
// 	park();
// #endif
	/*
Y fwd: 8 pos at 35135

Y
 bwd: 8 pos at 36493

X fwd: 2 pos at 10629

X bwd: 2 pos at 10434

mixed fwd: (0,0)-(2,2) at 10271

m
ixed bwd: (2,2)-(0,0) pos at 10271
	moveToPos(0,0);
	uint32_t start = millis();
	moveToPos(0, 8);
	Serial1.print(F("Y fwd: 8 pos at "));
	Serial1.println(millis()-start, DEC);
	start = millis();
	moveToPos(0,0);
	Serial1.print(F("Y bwd: 8 pos at "));
	Serial1.println(millis()-start, DEC);
	start = millis();
	moveToPos(2, 0);
	Serial1.print(F("X fwd: 2 pos at "));
	Serial1.println(millis()-start, DEC);
	start = millis();
	moveToPos(0,0);
	Serial1.print(F("X bwd: 2 pos at "));
	Serial1.println(millis()-start, DEC);
	start = millis();
	moveToPos(2,2);
	Serial1.print(F("mixed fwd: (0,0)-(2,2) at "));
	Serial1.println(millis()-start, DEC);
	start = millis();
	moveToPos(0,0);
	Serial1.print(F("mixed bwd: (2,2)-(0,0) pos at "));
	Serial1.println(millis()-start, DEC);
	*/
// 	while(1){
// 		testES();
// 		delay(1500);
// 	}
// #ifdef MY_ROOM
// #else
//   	parkY();
//   	parkX();
// #endif
}

void WaterDoserSystem::testAll()
{
	
	for (uint8_t x = 0; x < WD_SIZE_X; ++x) {
		for (uint8_t y = 0; y < WD_SIZE_Y; ++y) {
			Serial1.print(x, DEC);
			Serial1.print(F(" "));
			Serial1.print(y, DEC);
			Serial1.println(F(";"));
			if(!moveToPos(x,y)) {
				parkY();
				if (!moveToPos(x,y)) {
					Serial1.println(F("error in y-pos"));
					return;
				}
			}
			servoDown();
			delay(500);
			servoUp();
// 			delay(500);
		}
	}
	
}

void WaterDoserSystem::servoUp()
{
	errcode = 0;
	pinMode(VCC_PUMP_EN, OUTPUT);
	digitalWrite(VCC_PUMP_EN, HIGH);
	pinMode(A6, INPUT);
	pinMode(Z_AXE_DIR, OUTPUT);
	pinMode(Z_AXE_EN, OUTPUT);
	digitalWrite(Z_AXE_EN, HIGH);
	digitalWrite(Z_AXE_DIR, LOW);//UP
	uint32_t start = millis();
 #ifdef MY_ROOM
	while (analogRead(6) < 200 && (millis() - start < 10000UL))
 #else
// 	Serial1.println(analogRead(6), DEC);
	while (analogRead(6) < BIGROOM_STOPVAL && (millis() - start < 10000UL))
 #endif
	{
		pinMode(VCC_PUMP_EN, OUTPUT);
		digitalWrite(VCC_PUMP_EN, HIGH);
//   		Serial1.println(analogRead(6), DEC);
	}
 #ifdef MY_ROOM
	if (analogRead(6) < 200)
 #else
	if (analogRead(6) < BIGROOM_STOPVAL)
 #endif
	{
		Serial1.println(F("ERROR: sticked!"));
		errcode = WDERR_STICKED;
	} else {
		Serial1.println(F("Z-axe is OK"));
	}

	digitalWrite(Z_AXE_DIR, LOW);
	digitalWrite(Z_AXE_EN, LOW);
	
	digitalWrite(VCC_PUMP_EN, LOW);
}

bool WaterDoserSystem::servoDown()
{
	pinMode(A6, INPUT);
	pinMode(VCC_PUMP_EN, OUTPUT);
	digitalWrite(VCC_PUMP_EN, HIGH);
	delay(100);
	pinMode(Z_AXE_DIR, OUTPUT);
	pinMode(Z_AXE_EN, OUTPUT);
	digitalWrite(Z_AXE_EN, HIGH);
	digitalWrite(Z_AXE_DIR, HIGH);
	uint32_t start  = millis();
 #ifdef MY_ROOM
	while (analogRead(6) > 100 && millis() - start < 1000);
#else
 	Serial1.println(analogRead(6), DEC);
	while (analogRead(6) > BIGROOM_STOPVAL && millis() - start < 1000) {
   		Serial1.println(analogRead(6), DEC);
		delay(100);
	}
#endif
// 	return false;
	//anti-stick system.would be great to add down-pos sensor optocoupler, but I'm so lazy...
	int repeats = 0;
#ifdef MY_ROOM
	while (analogRead(6) > 100) {
#else
		//BIGROOM_STOPVAL and above -- up pos, less than BIGROOM_STOPVAL -- down pos
	while (analogRead(6) > BIGROOM_STOPVAL) {
#endif
#ifndef MY_ROOM
		Serial1.print(analogRead(6), DEC);
		Serial1.print('/');
		Serial1.println(BIGROOM_STOPVAL, DEC);
#endif
		pinMode(VCC_PUMP_EN, OUTPUT);
		digitalWrite(VCC_PUMP_EN, HIGH);

		digitalWrite(Z_AXE_EN, HIGH);
		digitalWrite(Z_AXE_DIR, LOW);
		delay(2000);
		digitalWrite(Z_AXE_EN, HIGH);
		digitalWrite(Z_AXE_DIR, HIGH);
		delay(2000);
		++repeats;
		if (repeats > 5) {
			Serial1.println("sticked");
			errcode = WDERR_STICKED;
			return false;
		}
	}
	delay(500);
	return true;
// 	digitalWrite(Z_AXE_EN, LOW);
}

void WaterDoserSystem::servoUnbind()
{

	digitalWrite(Z_AXE_EN, LOW);
	digitalWrite(VCC_PUMP_EN, LOW);
}
	
bool WaterDoserSystem::nextX()
{
	if (digitalRead(X_STEP_PIN) == HIGH) {
		while (digitalRead(X_STEP_PIN) == HIGH) {
			fwdX();
		}
	}
	uint32_t start = millis();
	if(cur_x == -1) {
		while (!digitalRead(X_STEP_PIN) && !digitalRead(X_STEP_PIN) ) {
			fwdX();
		}
	} else {
		while (true) {
			if (digitalRead(X_STEP_PIN) == HIGH) {
				if(millis() - start > 1000) break;
			}
			fwdX();
		}
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
	uint32_t start = millis();
	if(cur_y == -1) {
		while (!digitalRead(Y_STEP_PIN) && !digitalRead(Y_STEP_PIN) ) {
			fwdY();
		}
	} else {
		while (true) {
			if (digitalRead(Y_STEP_PIN) == HIGH) {
				if(millis() - start > 1000) break;
			}
			fwdY();
		}
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
 	uint16_t ticks = round( (float)ml * (float)wftpl * 0.001);
 	__wfs_run_mode = wdstWater;
 	exp->begin(expander_addr);
	pinMode(PUMP_PIN, OUTPUT);
	digitalWrite(PUMP_PIN, HIGH);
	pinMode(VCC_PUMP_EN, OUTPUT);
	digitalWrite(VCC_PUMP_EN, HIGH);

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
		if (millis() % 500 == 0) {
			if (!checkContinue()) break;
		}
	}//while

	digitalWrite(PUMP_PIN, LOW);
	haltPumps();
// Serial1.println("end of measure");
 	if ( (__wfs_flag < ticks) && (millis() - start_millis >= timeout) ) {
		errcode = WDERR_WFS_DEAD;
 	}
// 	Serial1.println(__LINE__, DEC);
 	Serial1.println(F("end of measure"));
 	Serial1.flush();
 	Serial1.print(F("ticks:"));
 	Serial1.print(ticks, DEC);
 	Serial1.print(F("flag:["));
 	Serial1.print(__wfs_flag, DEC);
 	Serial1.println(F("]"));
 	Serial1.flush();
 	uint16_t _ml = round(__wfs_flag * 1000.0/(float)wftpl);
	digitalWrite(VCC_PUMP_EN, LOW);
// 	ws.dec(ws_index, ml);
 	return _ml;
}

void WaterDoserSystem::haltPumps()
{
	pinMode(PUMP_PIN, OUTPUT);
	digitalWrite(PUMP_PIN, LOW);
	pinMode(VCC_PUMP_EN, OUTPUT);
	digitalWrite(VCC_PUMP_EN, LOW);
}

void WaterDoserSystem::testES()
{
// 	return;
	Serial1.print("X begin=");
#ifdef MY_ROOM
	Serial1.println(digitalRead(X_BEGIN_PIN), DEC);
#else
	Serial1.println(analogRead(X_BEGIN_PIN), DEC);
#endif
// 	Serial1.print("X end=");
// 	Serial1.println(analogRead(X_END_PIN), DEC);
	Serial1.print("X stepper=");
	Serial1.println(digitalRead(X_STEP_PIN), DEC);

	Serial1.print("Y begin=");
#ifdef MY_ROOM
	Serial1.println(digitalRead(Y_BEGIN_PIN), DEC);
#else
	Serial1.println(analogRead(Y_BEGIN_PIN), DEC);
#endif
	Serial1.print("Y end=");
	Serial1.println(analogRead(Y_END_PIN), DEC);
	Serial1.print("Y stepper=");
	Serial1.println(digitalRead(Y_STEP_PIN), DEC);

	Serial1.print("Z begin=");
	Serial1.println(analogRead(6), DEC);


}
bool WaterDoserSystem::park()
{
	uint8_t bits = 0x03;
	
	errcode = 0;
	
	servoUp();
	
	if (errcode) {
		return false;
	}
	fwdX();
	fwdY();
	delay(3000);
	while (bits) {
#ifdef MY_ROOM
		if (LOW == digitalRead(X_BEGIN_PIN))
#else
 		Serial1.print("X bsw");
 		Serial1.println(analogRead(X_BEGIN_PIN - A0), DEC);
		if (analogRead(X_BEGIN_PIN - A0) < 300)
#endif
		{
// 			Serial1.println(F("stopX"));
 			stopX();
			bits &= 0xFE;
		}
#ifdef MY_ROOM
		if (HIGH == digitalRead(Y_BEGIN_PIN))
#else
// 		Serial1.print("Y bsw");
// 		Serial1.println(analogRead(Y_BEGIN_PIN - A0), DEC);

		if (analogRead(Y_BEGIN_PIN - A0) > 500)
#endif
		{
			stopY();
// 			Serial1.println(F("stop Y"));
			bits &= 0xFD;
		}
	}//while bits
// 	delay(1000);
	bwdX();
	bwdY();
 	Serial1.println(F("move X bwd"));
 	Serial1.println(F("move Y bwd"));

	bits = 0x03;
	while (bits) {
#ifdef MY_ROOM
		if (HIGH == digitalRead(X_BEGIN_PIN))
#else
// 		Serial1.print("x:");Serial1.println(analogRead(X_BEGIN_PIN - A0), DEC);
		if (analogRead(X_BEGIN_PIN - A0) > 330)
#endif
		{
			stopX();
			bits &= 0xFE;
		}
#ifdef MY_ROOM
		if (LOW == digitalRead(Y_BEGIN_PIN))
#else
		if (analogRead(Y_BEGIN_PIN - A0) < 500)
#endif
		{
			stopY();
			bits &= 0xFD;
		}
	}
 	testES();
	cur_x = 0xFF;
	cur_y = 0xFF;
	return true;
}

bool WaterDoserSystem::parkX()
{
	int val = 0;
// 	Serial1.print("begin es val:");
// 	Serial1.println(digitalRead(X_BEGIN_PIN), DEC);
#ifdef MY_ROOM
	while (digitalRead(X_BEGIN_PIN)) {
		fwdX();
	}
#else
	while (analogRead(X_BEGIN_PIN - A0) <= (X_BEGIN_STOPVAL*1.1) ) {
		Serial1.print(F("fwdX es:"));
		Serial1.println(analogRead(X_BEGIN_PIN -A0), DEC);
		fwdX();
	}
#endif
	delay(500);
	stopX();
// 	Serial1.println("moving back");
// 	val = analogRead(X_BEGIN_PIN);
#ifdef MY_ROOM
	while (!digitalRead(X_BEGIN_PIN)) {
		bwdX();
	}
#else
	while(analogRead(X_BEGIN_PIN - A0) > 330) {
		Serial1.print(F("bwdX es:"));
		Serial1.println(analogRead(X_BEGIN_PIN -A0), DEC);
		bwdX();
	}
	while(analogRead(X_BEGIN_PIN - A0) < 500 ) {
		Serial1.print(F("fwd2 X es:"));
		Serial1.println(analogRead(X_BEGIN_PIN -A0), DEC);
		fwdX();
	}
	delay(500);
#endif
	stopX();
	cur_x = -1;
	return true;
}

bool WaterDoserSystem::parkY()
{
#ifdef MY_ROOM
	while(!digitalRead(Y_BEGIN_PIN)) {
		fwdY();
	}
#else
	while(analogRead(Y_BEGIN_PIN-A0) <= (Y_BEGIN_STOPVAL*1.1)) {
		fwdY();
	}
#endif
	stopY();
#ifdef MY_ROOM
	while (digitalRead(Y_BEGIN_PIN)) {
		bwdY();
	}
#else
	while (analogRead(Y_BEGIN_PIN-A0) >= Y_BEGIN_STOPVAL) {
		bwdY();
	}
	while(analogRead(Y_BEGIN_PIN - A0) <= Y_BEGIN_STOPVAL) {
		fwdY();
	}
	delay(500);
#endif
	stopY();
	cur_y = -1;
	return true;
}

bool WaterDoserSystem::moveToPos(uint8_t x, uint8_t y)
{
	if (x >= WD_SIZE_X) {
		Serial1.print(F("out of X size "));
		Serial1.print(x, DEC);
		Serial1.print(" / ");
		Serial1.print(WD_SIZE_X, DEC);
		return false;
	}
	if (y >= WD_SIZE_Y) {
		Serial1.println(F("out of Y size"));
		Serial1.print(y, DEC);
		Serial1.print(" / ");
		Serial1.print(WD_SIZE_Y, DEC);
		return false;
	}
	errcode = 0;
	servoUp();
	if (errcode) {
		Serial1.println(F("no move due Z-axe errors"));
		return false;
	}

	Serial1.print("cur pos (");
	Serial1.print(cur_x, DEC);
	Serial1.print(",");
	Serial1.print(cur_y);
	Serial1.println(")");

	Serial1.print("new pos (");
	Serial1.print(x, DEC);
	Serial1.print(",");
	Serial1.print(y);
	Serial1.println(")");

#define WDST_STOP		0
#define WDST_PARK		1
#define WDST_WAIT_LO_FWD	2
#define WDST_WAIT_HI_FWD	3
#define WDST_WAIT_LO_BWD	4
#define WDST_WAIT_HI_BWD	5
#if 1 // MY_ROOM
	uint8_t xs=0, ys=0;
	pinMode(X_STEP_PIN, INPUT);
	pinMode(Y_STEP_PIN, INPUT);
	/* state codes:
		0 - stop
		1 - park
		2 - move & wait LOW on *_STEP_PIN
		3 - move & wait HIGH on *_STEP_PIN
	 */
	if (x < cur_x) {
		Serial1.println("should park X");
		while (digitalRead(X_STEP_PIN) == HIGH) {
			bwdX();
		}
		delay(200);
		xs = WDST_WAIT_HI_BWD/*1*/;
		bwdX();
	} else if ( cur_x < x) {
 		Serial1.println("should move X fwd");
// 		xs = (digitalRead(X_STEP_PIN)==HIGH ? 3 : 2);
		xs = (digitalRead(X_STEP_PIN)==HIGH ? WDST_WAIT_HI_FWD : WDST_WAIT_LO_FWD);
		fwdX();
	}
	if (y < cur_y) {
		Serial1.println("should park Y");
		while (digitalRead(Y_STEP_PIN) == HIGH) {
			bwdY();
		}
		delay(200);
		ys = WDST_WAIT_HI_BWD/*1*/;
		bwdY();
	} else if (cur_y < y) {
// 		ys = (digitalRead(Y_STEP_PIN)==HIGH ? 3 : 2);
		ys = (digitalRead(Y_STEP_PIN)==HIGH ? WDST_WAIT_HI_FWD : WDST_WAIT_LO_FWD);
		fwdY();
		Serial1.println("should move Y");
	}
	uint32_t x_ms=millis(), y_ms=millis(), full_time = millis();
	while (xs || ys) {
		if (!checkContinue()) {
			stopX();
			stopY();
			break;
		}
		
		if (millis() - full_time > 180000UL) {
			park();
			return moveToPos(x,y);
		}
		if (xs == WDST_PARK) {
#ifdef MY_ROOM
			if (HIGH == digitalRead(X_BEGIN_PIN))
#else
			if(analogRead(X_BEGIN_PIN - A0)>= 500)
#endif
			{
				Serial1.println("x parked. move fwd");
				stopX();
				fwdX();
				x_ms = millis();
				xs = WDST_WAIT_LO_FWD;
				cur_x = -1;
			}
		} else if (xs == WDST_WAIT_LO_FWD/*2*/) {//wait low state.
			if (digitalRead(X_STEP_PIN) == LOW) {
				xs = WDST_WAIT_HI_FWD/*3*/;
			}
		} else if (xs == WDST_WAIT_HI_FWD/*3*/) {//wait HIGH
			if (cur_x == -1) {
				if (digitalRead(X_STEP_PIN) && digitalRead(X_STEP_PIN) ) {
					++cur_x;
  					Serial1.print("at x#1:");
 					Serial1.println(cur_x, DEC);
					xs = ((cur_x < x) ? WDST_WAIT_LO_FWD/*2*/ : WDST_STOP/*0*/);
  					Serial1.print("xs=");
  					Serial1.println(xs, DEC);
					x_ms = millis();
				}
				
				if (xs == WDST_STOP/*0*/) {
  					Serial1.print("finished x#1 at ");
  					Serial1.println(cur_x, DEC);
					stopX();
				}
			} else {
// 				while (true) {
				if (digitalRead(X_STEP_PIN) == HIGH) {
					if (millis() - x_ms > MIN_MOVE_TIME_BTW) {
						++cur_x;
  						Serial1.print("at x#2:");
  						Serial1.println(cur_x, DEC);
						x_ms = millis();
						xs = ((cur_x < x) ? /*2*/WDST_WAIT_LO_FWD : WDST_STOP/*0*/);
					}
				}//if step=high
				if (xs == WDST_STOP/*0*/) {
  					Serial1.print("finished x#2 at ");
  					Serial1.println(cur_x, DEC);
					stopX();
				}//is xs == 0
			}//else if cur_x > -1
		} else if (xs == WDST_WAIT_HI_BWD) {
// 			Serial1.println(F("xs WDST_WAIT_HI_BWD"));
#ifdef MY_ROOM
			if (digitalRead(X_BEGIN_PIN) == HIGH)
#else
			if (analogRead(X_BEGIN_PIN-A0) > 500)
#endif
			{
 				Serial1.println("ERROR: x at start");
				stopX();
				stopY();
				return false;
			}
			if (digitalRead(X_STEP_PIN) == HIGH && (millis() - x_ms > MIN_MOVE_TIME_BTW) ) {
				Serial1.println(F("xs switch to wait low"));
				x_ms = millis();
				--cur_x;
				xs = WDST_WAIT_LO_BWD;
			}
		} else if (xs == WDST_WAIT_LO_BWD) {
// 			Serial1.println(F("xs WDST_WAIT_LO_BWD"));
			if (digitalRead(X_STEP_PIN) == LOW && (millis() - x_ms > MIN_MOVE_LOW_TIME)) {
				xs = ( (cur_x == x) ? WDST_STOP : WDST_WAIT_HI_BWD);
				if(xs == WDST_WAIT_HI_BWD) Serial1.println(F("xs switch to wait high"));
				x_ms = millis();
			}
			if (xs == WDST_STOP) {
				Serial1.println(F("stopX"));
				stopX();
			}
		} else {
			if (xs) {
				Serial1.print(F("unknown state xs="));
				Serial1.println(xs, DEC);
			}
		}

		if (ys == WDST_PARK) {
#ifdef MY_ROOM
			if (HIGH == digitalRead(Y_BEGIN_PIN))
#else
			if (analogRead(Y_BEGIN_PIN-A0) > 500)
#endif
			{
				Serial1.println("y parked. move fwd");
				stopY();
				fwdY();
				y_ms = millis();
				ys = WDST_WAIT_LO_FWD;
				cur_y = -1;
			}
		} else if (ys == WDST_WAIT_LO_FWD/*2*/) {//wait low state.
			if (digitalRead(Y_STEP_PIN) == LOW) {
				ys = WDST_WAIT_HI_FWD/*3*/;
			}
		} else if (ys == WDST_WAIT_HI_FWD/*3*/) {//wait HIGH
			if (cur_y == -1) {
				if (digitalRead(Y_STEP_PIN) && digitalRead(Y_STEP_PIN) ) {
					++cur_y;
					Serial1.print("at y#1:");
					Serial1.println(cur_y, DEC);
					ys = ((cur_y < y) ? WDST_WAIT_LO_FWD/*2*/ : WDST_STOP/*0*/);
					Serial1.print("ys=");
					Serial1.println(ys, DEC);
					y_ms = millis();
				}

				if (ys == WDST_STOP/*0*/) {
					Serial1.print("finished y#1 at ");
					Serial1.println(cur_y, DEC);
					stopY();
				}
			} else {
// 				while (true) {
				if (digitalRead(Y_STEP_PIN) == HIGH) {
					if (millis() - y_ms > MIN_MOVE_TIME_BTW) {
						++cur_y;
						Serial1.print("at y#2:");
						Serial1.println(cur_y, DEC);
						y_ms = millis();
						ys = ((cur_y < y) ? /*2*/WDST_WAIT_LO_FWD : WDST_STOP/*0*/);
					}
				}//if step=high
				if (ys == WDST_STOP/*0*/) {
					Serial1.print("finished y#2 at ");
					Serial1.println(cur_y, DEC);
					stopY();
				}//is ys == 0
			}//else if cur_y > -1
		} else if (ys == WDST_WAIT_HI_BWD) {
#ifdef MY_ROOM
			if (digitalRead(Y_BEGIN_PIN) == LOW) 
#else
			if (analogRead(Y_BEGIN_PIN-A0) < 500)
#endif
			{
				Serial1.println(F("ERROR: y at start"));
				stopX();
				stopY();
				return false;
			}
			if (digitalRead(Y_STEP_PIN) == HIGH && (millis() - y_ms > MIN_MOVE_TIME_BTW) ) {
				y_ms = millis();
				--cur_y;
				ys = WDST_WAIT_LO_BWD;
			}
		} else if (ys == WDST_WAIT_LO_BWD) {
			if (digitalRead(Y_STEP_PIN) == LOW && (millis() - y_ms > MIN_MOVE_LOW_TIME)) {
				ys = ( (cur_y == y) ? WDST_STOP : WDST_WAIT_HI_BWD);
				y_ms = millis();
			}
			if (ys == WDST_STOP) {
				stopY();
			}
		}
	}//while xs && ys;
	stopX();
	stopY();
 	Serial1.print("done (");
 	Serial1.print(cur_x);
 	Serial1.print(",");
 	Serial1.print(cur_y);
 	Serial1.println(")");
/* old algo (move each axe separately)*/
#else
	if (x < cur_x) {
// 		Serial1.println("park X before move");
		parkX();
		nextX();
	}
	if (y < cur_y) {
// 		Serial1.println("park Y before move");
		parkY();
		nextY();
// 		Serial1.print("cur_y=");
// 		Serial1.println(cur_y);
	}
	
	while (cur_x < x) {
		if (!nextX()) {
			Serial1.println(F("ERROR in X-pos;"));
			errcode = WDERR_POS_ERR;
			return false;
		}
	}
	
	while(cur_y < y) {
		if (!nextY()) {
			Serial1.println(F("ERROR in Y-pos;"));
			errcode = WDERR_POS_ERR;
			return false;
		}
	}
	
 	Serial1.println(F("done"));
#endif
	return true;
}


uint16_t WaterDoserSystem::pipi(uint8_t x, uint8_t y, uint8_t ml, AirTime at)
{
	errcode = 0;
	if(!this->moveToPos(x, y)) {
		Serial1.println(F("ERROR while positioning;"));
		return 0;
	}
	if (!servoDown()) {
		Serial1.println(F("ERROR: Z-axe fault"));
		return 0;
	}
	if (errcode > 0) {
		servoUp();
		Serial1.print(F("STOP due fatal error "));
		Serial1.print(errcode, DEC);
		Serial1.println(F(";"));
		return 0;
	}
// 	Serial1.println(F("pipi"));
	pinMode(AIR_PIN, OUTPUT);
 	digitalWrite(AIR_PIN, HIGH);
// 	this->init_measure();
	uint16_t ret_ml = this->measure(ml, 5000);
	Serial1.print(ret_ml, DEC);
	Serial1.println(F(";"));
	dope(at);
	servoUp();
// 	delay(500);
	return ret_ml;
}

coords WaterDoserSystem::curPos()
{
	coords ret;
	ret.x = cur_x;
	ret.y = cur_y;
	return ret;
}

WaterDoserSystem water_doser;

/*
общий разъём:
Х начало
У начало
Х шаговый
У шаговый
вкл Z
напр Z
аэратор
насос
WFS




*/