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

#ifdef MY_ROOM
	#define X_AT_SLOT (digitalRead(X_STEP_PIN) == HIGH)
	#define Y_AT_SLOT (digitalRead(Y_STEP_PIN) == HIGH)
#else
	#define X_AT_SLOT (digitalRead(X_STEP_PIN) == LOW)
	#define Y_AT_SLOT (digitalRead(Y_STEP_PIN) == LOW)
#endif

extern void __digitalWrite(uint8_t pin, uint8_t val);

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
	
#ifdef BIG_ROOM
	pinMode(Z_AXE_UP_PIN, INPUT);
#endif
// 	while(1){
// 		testES();
// 		delay(1000);
// 	}

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
// #endif
	park();
}

void WaterDoserSystem::testAll()
{
	
	/*for (uint8_t x = 0; x < WD_SIZE_X; ++x) {
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
		}
	}*/
	uint32_t start = millis();
	randomSeed(analogRead(0));
	park();
	for (uint8_t i=0;i<200;++i) {
		uint8_t x = random(WD_SIZE_X);
		uint8_t y = random(WD_SIZE_Y);
			Serial1.print(i, DEC);
			Serial1.print(F(": "));
			Serial1.print(x, DEC);
			Serial1.print(F(" "));
			Serial1.print(y, DEC);
			Serial1.println(F(";"));
			if(!moveToPos(x,y)) {
				Serial1.println(F("error in pos"));
				park();
				continue;
			}
 			servoDown();
			delay(500);
			servoUp();
// 			delay(3000);
	}
	Serial1.print("end in ");
	Serial1.println(millis()-start);
}

void WaterDoserSystem::servoUp()
{
	errcode = 0;
// 	return;
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
	while (digitalRead(Z_AXE_UP_PIN) && (millis() - start < 10000UL))
 #endif
	{
		pinMode(VCC_PUMP_EN, OUTPUT);
		digitalWrite(VCC_PUMP_EN, HIGH);
//   		Serial1.println(analogRead(6), DEC);
	}
 #ifdef MY_ROOM
	if (analogRead(6) < 200)
 #else
	if (digitalRead(Z_AXE_UP_PIN))
 #endif
	{
		Serial1.println(F("ERROR: sticked!"));
		errcode = WDERR_STICKED;
	} else {
// 		Serial1.println(F("Z-axe is OK"));
	}

	digitalWrite(Z_AXE_DIR, LOW);
	digitalWrite(Z_AXE_EN, LOW);
	
	digitalWrite(VCC_PUMP_EN, LOW);
}

bool WaterDoserSystem::servoDown()
{
	pinMode(Z_AXE_UP_PIN, INPUT);
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
//  	Serial1.println(digitalRead(Z_AXE_UP_PIN), DEC);
	while (!digitalRead(Z_AXE_UP_PIN) && millis() - start < 1000) {
//    		Serial1.println(digitalRead(Z_AXE_UP_PIN), DEC);
		delay(300);
	}
#endif
// 	return false;
	//anti-stick system.would be great to add down-pos sensor optocoupler, but I'm so lazy...
	int repeats = 0;
#ifdef MY_ROOM
	while (analogRead(6) > 100) {
#else
		//BIGROOM_STOPVAL and above -- up pos, less than BIGROOM_STOPVAL -- down pos
	while (!digitalRead(Z_AXE_UP_PIN)) {
#endif
#ifndef MY_ROOM
// 		Serial1.print("reed:");
// 		Serial1.println(digitalRead(6), DEC);
// 		Serial1.print('/');
// 		Serial1.println(BIGROOM_STOPVAL, DEC);
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
// 	pinMode(X_BEGIN_PIN, INPUT);
	Serial1.print(digitalRead(X_BEGIN_PIN), DEC);
// 	Serial1.print("X end=");
// 	Serial1.println(analogRead(X_END_PIN), DEC);
	Serial1.print("    X stepper=");
// 	pinMode(X_STEP_PIN, INPUT);
	Serial1.print(digitalRead(X_STEP_PIN), DEC);

	Serial1.print("    Y begin=");
	Serial1.print(digitalRead(Y_BEGIN_PIN), DEC);
// 	Serial1.print("Y end=");
// 	Serial1.println(analogRead(Y_END_PIN), DEC);
	Serial1.print("    Y stepper=");
	Serial1.print(digitalRead(Y_STEP_PIN), DEC);

	Serial1.print("    Z begin=");
#ifdef BIG_ROOM
	Serial1.println(digitalRead(Z_AXE_UP_PIN), DEC);
#else
	Serial1.println(analogRead(6), DEC);
#endif
}

#ifdef MY_ROOM
	#define X_AT_BEGIN (HIGH == digitalRead(X_BEGIN_PIN))
	#define Y_AT_BEGIN (LOW == digitalRead(Y_BEGIN_PIN))
#else
	#define X_AT_BEGIN (LOW == digitalRead(X_BEGIN_PIN))
	#define Y_AT_BEGIN (HIGH == digitalRead(Y_BEGIN_PIN))
#endif
bool WaterDoserSystem::park()
{
	uint8_t bits = 0x03;
	
	errcode = 0;
#ifdef MY_ROOM
	servoUp();
#endif	
	if (errcode) {
		return false;
	}
	fwdX();
	fwdY();
 	delay(1000);
	//go fwd until X begin optocoupler is closed
// 	Serial1.println("fwd...");
	while (bits) {
// 		Serial1.print(X_AT_BEGIN, DEC);
// 		Serial1.println(Y_AT_BEGIN, DEC);
		if (!X_AT_BEGIN) {
//  			Serial1.println(F("stopX"));
 			stopX();
			bits &= 0xFE;
		}
		if (!Y_AT_BEGIN) {
			stopY();
//  			Serial1.println(F("stop Y"));
			bits &= 0xFD;
		}
	}//while bits
// 	delay(1000);
	bwdX();
	bwdY();
//  	Serial1.println(F("move X bwd"));
//  	Serial1.println(F("move Y bwd"));

	bits = 0x03;
	while (bits) {
//  		Serial1.print(X_AT_BEGIN, DEC);
//  		Serial1.println(Y_AT_BEGIN, DEC);
		if (X_AT_BEGIN && X_AT_BEGIN) {
			stopX();
			bits &= 0xFE;
		}
		if (Y_AT_BEGIN && Y_AT_BEGIN) {
			stopY();
			bits &= 0xFD;
		}
	}
 	Serial1.println("parked");
//  	testES();
	cur_x = 0xFF;
	cur_y = 0xFF;
#ifndef MY_ROOM
		while(!X_AT_SLOT)fwdX();
		stopX();
		while(!Y_AT_SLOT)fwdY();
		stopY();
		cur_x = 0;
		cur_y = 0;
#endif
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
#define WDST_WAIT_WALL_FWD	2
#define WDST_WAIT_SLOT_FWD	3
#define WDST_WAIT_WALL_BWD	4
#define WDST_WAIT_SLOT_BWD	5
	
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
		while (X_AT_SLOT && X_AT_SLOT && !X_AT_BEGIN) {
			bwdX();
			delay(200);
		}
		bwdX();
		xs = WDST_WAIT_SLOT_BWD;
	} else if ( cur_x < x) {
//  		Serial1.println("should move X fwd");
		while (X_AT_SLOT && X_AT_SLOT) {
			fwdX();
			delay(200);
		}
		fwdX();
		xs = WDST_WAIT_SLOT_FWD;
	}
	if (y < cur_y) {
 		while (Y_AT_SLOT && Y_AT_SLOT && !Y_AT_BEGIN) {
			bwdY();
			delay(200);
		}
		bwdY();
		ys = WDST_WAIT_SLOT_BWD;
	} else if (cur_y < y) {
		while(Y_AT_SLOT && Y_AT_SLOT) {
			fwdY();
			delay(200);
		}
		fwdY();
		ys = WDST_WAIT_SLOT_FWD;
	}
	uint32_t x_ms=millis(), y_ms=millis(), full_time = millis();
	Serial1.println(xs ||ys, DEC);
	while (xs || ys) {
		if (!checkContinue()) {
			stopX();
			stopY();
			Serial1.println(F("Stop by btn"));
			break;
		}
		
		if (millis() - full_time > 180000UL) {
			park();
			return moveToPos(x,y);
		}
		if (xs == WDST_PARK) {
			if (X_AT_BEGIN) {
				Serial1.println("x parked. move fwd");
				stopX();
				fwdX();
				x_ms = millis();
				xs = WDST_WAIT_WALL_FWD;
				cur_x = -1;
			}
		} else if (xs == WDST_WAIT_WALL_FWD/*2*/) {//wait low state.
// 			Serial1.print("xs wait wall ");
// 			Serial1.println(X_AT_SLOT);
			if (!X_AT_SLOT) {
				xs = WDST_WAIT_SLOT_FWD/*3*/;
			}
		} else if (xs == WDST_WAIT_SLOT_FWD/*3*/) {//wait HIGH
// 			Serial1.print("xs wait slot ");
// 			Serial1.println(X_AT_SLOT);
			if (cur_x == -1) {
				if (X_AT_SLOT && X_AT_SLOT ) {
					++cur_x;
  					Serial1.print("at x#1:");
 					Serial1.println(cur_x, DEC);
					xs = ((cur_x < x) ? WDST_WAIT_WALL_FWD/*2*/ : WDST_STOP/*0*/);
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
				if (X_AT_SLOT) {
					if (millis() - x_ms > MIN_MOVE_TIME_BTW) {
						++cur_x;
  						Serial1.print("at x#2:");
  						Serial1.println(cur_x, DEC);
						x_ms = millis();
						xs = ((cur_x < x) ? /*2*/WDST_WAIT_WALL_FWD : WDST_STOP/*0*/);
					}
				}//if step=high
				if (xs == WDST_STOP/*0*/) {
  					Serial1.print("finished x#2 at ");
  					Serial1.println(cur_x, DEC);
					stopX();
				}//is xs == 0
			}//else if cur_x > -1
		} else if (xs == WDST_WAIT_SLOT_BWD) {
// 			Serial1.println(F("xs WDST_WAIT_SLOT_BWD"));
			if (X_AT_BEGIN) {
 				Serial1.println("ERROR: x at start");
				stopX();
				stopY();
				return false;
			}
			if (X_AT_SLOT && (millis() - x_ms > MIN_MOVE_TIME_BTW) ) {
// 				Serial1.println(F("xs switch to wait low"));
				x_ms = millis();
				--cur_x;
				Serial1.print("at x#3:");
  				Serial1.println(cur_x, DEC);
				xs = WDST_WAIT_WALL_BWD;
			}
		} else if (xs == WDST_WAIT_WALL_BWD) {
// 			Serial1.println(F("xs WDST_WAIT_WALL_BWD"));
			if (!X_AT_SLOT && (millis() - x_ms > MIN_MOVE_LOW_TIME)) {
				xs = ( (cur_x == x) ? WDST_STOP : WDST_WAIT_SLOT_BWD);
// 				if(xs == WDST_WAIT_SLOT_BWD) Serial1.println(F("xs switch to wait high"));
				x_ms = millis();
			}
			if (xs == WDST_STOP) {
// 				Serial1.println(F("stopX"));
				stopX();
			}
		} else {
			if (xs) {
				Serial1.print(F("unknown state xs="));
				Serial1.println(xs, DEC);
			}
		}

		if (ys == WDST_PARK) {
			if (Y_AT_BEGIN) {
				Serial1.println("y parked. move fwd");
				stopY();
				fwdY();
				y_ms = millis();
				ys = WDST_WAIT_WALL_FWD;
				cur_y = -1;
			}
		} else if (ys == WDST_WAIT_WALL_FWD/*2*/) {//wait low state.
// 			Serial1.print(F("Y L "));
// 			Serial1.println(digitalRead(Y_STEP_PIN), DEC);
			if (!Y_AT_SLOT) {
				ys = WDST_WAIT_SLOT_FWD/*3*/;
			}
		} else if (ys == WDST_WAIT_SLOT_FWD/*3*/) {//wait HIGH
// 			Serial1.print(F("Y H "));
// 			Serial1.println(digitalRead(Y_STEP_PIN), DEC);
			if (cur_y == -1) {
				if (Y_AT_SLOT && Y_AT_SLOT ) {
					++cur_y;
					Serial1.print("at y#1:");
					Serial1.println(cur_y, DEC);
					ys = ((cur_y < y) ? WDST_WAIT_WALL_FWD/*2*/ : WDST_STOP/*0*/);
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
				if (Y_AT_SLOT) {
					if (millis() - y_ms > MIN_MOVE_TIME_BTW) {
						++cur_y;
						Serial1.print("at y#2:");
						Serial1.println(cur_y, DEC);
						y_ms = millis();
						ys = ((cur_y < y) ? /*2*/WDST_WAIT_WALL_FWD : WDST_STOP/*0*/);
					}
				}//if step=high
				if (ys == WDST_STOP/*0*/) {
					Serial1.print("finished y#2 at ");
					Serial1.println(cur_y, DEC);
					stopY();
				}//is ys == 0
			}//else if cur_y > -1
		} else if (ys == WDST_WAIT_SLOT_BWD) {
			if (Y_AT_BEGIN) {
				Serial1.println(F("ERROR: y at start"));
				stopX();
				stopY();
				cur_y = -1;
				return moveToPos(x, y);
			}
			if (Y_AT_SLOT && (millis() - y_ms > MIN_MOVE_TIME_BTW) ) {
				y_ms = millis();
				--cur_y;
				Serial1.print("at y#3:");
				Serial1.println(cur_y, DEC);
				ys = WDST_WAIT_WALL_BWD;
			}
		} else if (ys == WDST_WAIT_WALL_BWD) {
			if (!Y_AT_SLOT && (millis() - y_ms > MIN_MOVE_LOW_TIME)) {
				ys = ( (cur_y == y) ? WDST_STOP : WDST_WAIT_SLOT_BWD);
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