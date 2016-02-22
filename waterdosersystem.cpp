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

#define PUMP_PIN			(49)
#define AIR_PIN				(22)
#define WFS_PIN				(47)

#define X_AXE_DIR			26
#define X_AXE_DIR2			27

#define Y_AXE_DIR			28
#define Y_AXE_DIR2			29

#define X_AXE_EN			40
#define Y_AXE_EN			39

#define SERVO_PIN			(46)

#define X_BEGIN_PIN			6 // analog! PF6/D49
#define X_END_PIN			7 //analog! PF7/D50

#define Y_BEGIN_PIN			52 // digital!
#define Y_END_PIN			5 // analog! PF5/D48

#define X_STEP_PIN			(31)
#define Y_STEP_PIN			(30)

#define X_BEGIN_HALT_VAL	172

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

// #define MCP_PORTB			(8)
// #define WDS_X_EN_PIN		(MCP_PORTB + 3)		//10
// #define WDS_Y_EN_PIN		(MCP_PORTB + 5)		//12
// #define WDS_X_DIR_PIN		(MCP_PORTB + 2)		//11
// #define WDS_Y_DIR_PIN		(MCP_PORTB + 4)		//13
// #define WDS_SERVO_PIN		48
// #define WDS_X_ENDSW_PIN		(MCP_PORTB + 1)		//9
// #define WDS_Y_ENDSW_PIN		(MCP_PORTB + 6)		//14
// #define WDS_X_OC_PIN		(MCP_PORTB + 0)		//8
// #define WDS_Y_OC_PIN		(MCP_PORTB + 7)		//15

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
	Serial1.println("tests ended");
	cur_x = 0xFF;
	cur_y = 0xFF;
	servoUp();
	delay(500);
	servoUnbind();

	dope(atMedium);

	return;
	fwdX();
 	delay(2000);
	stopX();
	
 	parkX();
 	parkY();
// 	nextX();
	/*
	for (int i=0; i< 4;++i) {
		bool ok = nextY();
		servoDown();
		delay(500);
		servoUp();
		Serial1.print("pos ");
		Serial1.print(i, DEC);
		Serial1.print(" ok=");
		Serial1.println(ok, DEC);
		delay(1000);
	}*/
  	while(true) {
  		testES();
  		delay(1000);
		break;
  	}
}

void WaterDoserSystem::servoUp()
{
	srv.attach(SERVO_PIN);
	srv.write(60);
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

bool WaterDoserSystem::init_measure(/*uint8_t doser_index*/)
{
	haltPumps();
	DDRF  &= ~(1<< 5);
	PORTF |= (1<<5);

// 	exp->begin(expander_addr);
// 	Serial1.println(__LINE__, DEC);
// 	exp->pinMode(1, INPUT);
// 	exp->pinMode(3, INPUT);
// 	Serial1.println(__LINE__, DEC);
// 	exp->pullUp(1, true);
// 	exp->pullUp(3, true);
// 	Serial1.println(__LINE__, DEC);
// 	exp->setupInterruptPin(1, CHANGE);
// 	exp->setupInterruptPin(3, CHANGE);
	Serial1.println(__LINE__, DEC);
	return true;
}

uint16_t WaterDoserSystem::measure(uint16_t ml, uint16_t timeout)
{
 	uint16_t wftpl = 2670;
 	uint16_t ticks = round( (float)ml * (float)wftpl * 0.001);
 	__wfs_run_mode = wdstWater;
// 	Serial1.println(__LINE__, DEC);
 	exp->begin(expander_addr);
	WaterStorages ws;
	ws.begin();
	uint8_t ws_index;
	WaterStorageData wsd = ws.readNonemptyStorage(ws_index);
	if (!wsd.enabled || ws_index == 255) {
		Serial1.println("No water at all");
		return 0;
	}

	pinMode(wsd.pump_pin, OUTPUT);
	digitalWrite(wsd.pump_pin, HIGH);
// 	DDRF |= (1<<6);

// 	PORTF |= (1<<6);
	pinMode(WFS_PIN, INPUT_PULLUP);
 	uint8_t prev = digitalRead(WFS_PIN), state=0;
 	uint32_t start_millis = millis();
	state = 0;
	__wfs_flag = 0;
	while ( (millis() - start_millis < timeout) && (__wfs_flag < ticks) ) {
		state = digitalRead(WFS_PIN);
		if ( !state && prev) {
			++__wfs_flag;
			start_millis = millis();
		}
		prev = state;
	}//while

	digitalWrite(wsd.pump_pin, LOW);
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
	ws.dec(ws_index, ml);
 	return _ml;

}

void WaterDoserSystem::haltPumps()
{
	WaterStorages ws;
	ws.begin();

	for (int i = 0; i < ws.getStoragesCount(); ++i) {
		WaterStorageData wsd = ws.readStorageData(i);
		pinMode(wsd.pump_pin, OUTPUT);
		digitalWrite(wsd.pump_pin, LOW);
	}

// 	PORTF &= ~(1<<6);
//  	exp->begin(expander_addr);
// 	exp->digitalWrite(7, LOW);
// 	exp->digitalWrite(7, LOW);
// 	exp->digitalWrite(7, LOW);
 	Serial1.println("haltPumps");
}

int WaterDoserSystem::readCmdStatus(uint32_t timeout, char*whatOk)
{
	char buf[16]={0}, *pbuf = buf;
	int n = 0;
	uint32_t start = millis();

	while (timeout == 0 || (millis() - start < timeout)) {
		if (Serial.available()) {
			*pbuf = Serial.read();
			Serial1.print("got from doser:[");
			Serial1.write(*pbuf);
			if (*pbuf < 10) continue;
			Serial1.print("] buf=[");
			Serial1.print(buf);
			Serial1.println("]");
			if ( *pbuf == 10 || *pbuf==13 ) {
				if( (pbuf - buf) > 1) {
					if (strstr(buf, "ERR")!=NULL) {
						return -1;
					}
					if ( (whatOk == NULL) && (strstr(buf, "OK")!=NULL)) {
						return 1;
					} else if (whatOk != NULL && strstr(buf, whatOk) != NULL) {
						return 1;
					}
// 					return (strstr(buf, "ERR")!=NULL) ? (-1) : (strstr(buf, "OK")!=NULL);
				} else {
					continue;
				}
			}
			++pbuf;
			n = (int)(pbuf - buf);
			Serial1.print("n=");
			Serial1.println(n, DEC);
			if (n>=16) {
				Serial1.println("Overflow while wait reply from doser");
				return 0;
			}
		}
	}//while
	if(timeout) {
		Serial1.println("timeout =(");
	}
	return 0;
}//sub

void WaterDoserSystem::testES()
{
	Serial1.print("X begin=");
	Serial1.println(analogRead(X_BEGIN_PIN), DEC);
	Serial1.print("X end=");
	Serial1.println(analogRead(X_END_PIN), DEC);
	Serial1.print("X stepper=");
	Serial1.println(digitalRead(X_STEP_PIN), DEC);

	Serial1.print("Y begin=");
	Serial1.println(digitalRead(Y_BEGIN_PIN), DEC);
	Serial1.print("Y end=");
	Serial1.println(analogRead(Y_END_PIN), DEC);
	Serial1.print("Y stepper=");
	Serial1.println(digitalRead(Y_STEP_PIN), DEC);
}

bool WaterDoserSystem::parkX()
{
	int val = 0;
	while(analogRead(X_BEGIN_PIN) < X_BEGIN_HALT_VAL) {
		fwdX();
	}
	stopX();
	val = analogRead(X_BEGIN_PIN);
	int8_t reps = 0;
	while (val > X_BEGIN_HALT_VAL || val <= X_BEGIN_HALT_VAL && reps < 3) {
		val = analogRead(X_BEGIN_PIN);
		Serial1.println(val, DEC);
		bwdX();
		reps += (val < X_BEGIN_HALT_VAL?1:0);
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
	}
	if (y < cur_y) {
		parkY();
	}
}

// bool WaterDoserSystem::execCmd(char*cmd, uint32_t timeout, uint8_t tries, char*replyOK)
// {
// 	uint32_t start;
// 	char ch;
// 	while (Serial.available()) {
// 		ch = Serial.read();
// 	}
// 
// 	int ok = false, tries_made = 0;
// 	do {
// 		Serial.println(cmd);
// 		delay(50);
// 		Serial.flush();
// 		ok = readCmdStatus(timeout ? timeout : 2000UL, replyOK);
// 		++tries_made;
// 		if (ok < 0 && tries > 0 && tries_made >=tries) return false;
// 		if (timeout) {
// 			if (tries && (tries <= tries_made)) {
// 				return ok == 1;
// 			}
// 		}
// 	} while ( ok != 1 );
// 	return 1;
// }

// void WaterDoserSystem::liftUp()
// {
// 	if (!execCmd("UP", 2000UL, "+UP")) {
// 		Serial1.println(__LINE__, DEC);
// 		return;
// 	}
// }
//
// void WaterDoserSystem::liftDown()
// {
// 	if (!execCmd("DOWN", 2000UL, "+DOWN")) {
// 		Serial1.println(__LINE__, DEC);
// 		return;
// 	}
// // 	delay(1000);
// }

void WaterDoserSystem::prepareWatering()
{
	Serial1.println("pinging slave");
}

uint16_t WaterDoserSystem::pipi(uint8_t x, uint8_t y, uint8_t ml, AirTime at)
{
	if(!this->moveToPos(x, y)) {
		Serial1.println("ERROR while positioning");
		return 0;
	}
	/*
	Serial1.println("down...");
	bool ok = false;
	if (execCmd("DOWN", 4000UL, "+DOWN") != 1) {
		char buf[50];
		sprintf(buf, "DOWN cmd fault at(%d,%d)", x, y);
		logger.addError(buf);
		Serial1.println(__LINE__, DEC);
		return 0;
	}*/
// 	delay(3000);

	pinMode(AIR_PIN, OUTPUT);
	digitalWrite(AIR_PIN, HIGH);
	this->init_measure();
	uint16_t ret_ml = this->measure(ml, 5000);
	Serial1.println(ret_ml, DEC);
	if ( at == atShort) {
		delay(4000);
	} else if (at == atMedium) {
		delay(6000);
	} else if(at == atLong) {
		delay(12000);
	} else {
		delay(6000);
	}
	digitalWrite(AIR_PIN, LOW);

// 	PORTF &= ~(1<<7);

// 	execCmd("UP", 2000UL, 1, "+UP");

	return ret_ml;
}

WaterDoserSystem water_doser;


// ISR(INT7_vect)
// {
//  	PORTE = PINE ^ (1<<2);
// // 	Serial1.print(i2cExpander.getLastInterruptPin(), DEC);
// // 	Serial1.print(" ");
// // 	Serial1.println(i2cExpander.getLastInterruptPinValue(), DEC);
// // 	Serial1.println("INT7");
// // 	return;
// 	if (wdstIdle == __wfs_run_mode) {
// // 		water_doser.haltPumps(true);
// 		return;
// 	} else if (wdstWalk == __wfs_run_mode) {
// 		__wfs_flag = 1;
// 	} else {
//  		++__wfs_flag;
// 	}
// }
//
//
//
