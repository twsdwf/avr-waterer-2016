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
#include "esp8266.h"

#define WFSM_NONE			(0)
#define WFSM_WATERCOUNTER 	(1)
#define WFSM_MOVE			(2)


#define WDERR_STICKED		1
#define WDERR_WFS_DEAD		2
#define WDERR_POS_ERR		3
#define WDERR_PARK_ERR		4
#define ES_HITS 4

#define BS(x) (1<<(x))

#define AXE_X	1
#define AXE_Y	2

#define WD_TICKS_PER_STEP	30

#ifdef USE_ESP8266
extern ESP8266 esp8266;
#endif
extern char* str;
extern Configuration g_cfg;
// extern ErrLogger logger;
extern I2CExpander i2cExpander;

static volatile uint16_t pulse_x=0, pulse_y=0, bx=0, by=0;
static volatile uint8_t psx=0, psy=0, xbsw_cnt =0, ybsw_cnt=0, xesw_cnt = 0, yesw_cnt=0;
static volatile uint32_t dtx=0, dty = 0;


WaterDoserSystem water_doser;

ISR(INT5_vect) 
{
  if (PINA & BS(6)) { // X end switch pushed down
    ++xesw_cnt;
    if (xesw_cnt > ES_HITS) {
      cli();
        EIMSK &= ~BS(INT5);
      sei();
      water_doser.stopX();
      Serial1.println("x axe at end");
      xesw_cnt = 0;
    }
  } else {
    xesw_cnt = 0;
  }
  
  if (PINA & BS(7)) { // X begin switch pushed down
    ++xbsw_cnt;
    if (xbsw_cnt > ES_HITS) {
      cli();
        EIMSK &= ~BS(INT5);
      sei();
      water_doser.stopX();
      Serial1.println("x axe at begin");
      xbsw_cnt = 0;
    }
  } else {
    xbsw_cnt = 0;
  }
  
  if (millis() - dtx >= 50) { 
	++pulse_x;
	dtx = millis();
  }
  
  if (pulse_x == bx) {
    water_doser.stopX();
  }
}


ISR(INT6_vect) 
{
  if (PINC & BS(7)) { // Y end switch pushed down
    ++yesw_cnt;
    if (yesw_cnt > ES_HITS) {
      cli();
        EIMSK &= ~BS(INT6);
      sei();
      water_doser.stopY();
      Serial1.println("Y axe at end");
      yesw_cnt = 0;
    }
  } else {
    yesw_cnt = 0;
  }
  
  if (PING & BS(2)) { // X begin switch pushed down
    ++ybsw_cnt;
    if (ybsw_cnt > ES_HITS) {
      cli();
        EIMSK &= ~(BS(INT6));
      sei();
      water_doser.stopY();
      Serial1.println("Y axe at begin");
      ybsw_cnt = 0;
    }
  } else {
    ybsw_cnt = 0;
  }

  if (millis() - dty >= 50) { 
	++pulse_y;
	dty = millis();
  }
  
  if (pulse_y == by) {
    water_doser.stopY();
  }
}


#define WDST_STOP		0
#define WDST_PARK		1
#define WDST_WAIT_WALL_FWD	2
#define WDST_WAIT_SLOT_FWD	3
#define WDST_WAIT_WALL_BWD	4
#define WDST_WAIT_SLOT_BWD	5
	
extern void __digitalWrite(uint8_t pin, uint8_t val);

typedef enum WaterDoserSystemState{
	wdstIdle,
	wdstWalk,
	wdstWater,
	wdstError
}WaterDoserSystemState;


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
    _b_inited = false;
}

WaterDoserSystem::~WaterDoserSystem()
{
	cli();
	//	EIMSK ^= 0b10000000;//enable int7
	sei();
}

bool WaterDoserSystem::isInited()
{
    return this->_b_inited;
}

void WaterDoserSystem::dope(AirTime at)
{
	pinMode(AIR_PIN, OUTPUT);
	digitalWrite(AIR_PIN, HIGH);
	uint8_t n = 0;
	if (at == atShort) {
		delay(5000);
	} else if(at == atMedium){
		delay(10000);
	}else {
		delay(15000);
	}
	digitalWrite(AIR_PIN, LOW);
}

void WaterDoserSystem::begin(/*uint8_t _expander_addr, I2CExpander*_exp*/)
{
	errcode = 0;
	_is_run = 0;
	pinMode(X_AXE_DIR, OUTPUT);
	pinMode(X_AXE_DIR2, OUTPUT);
	pinMode(X_AXE_EN, OUTPUT);
	
	pinMode(X_BEGIN_PIN, INPUT);
	pinMode(X_END_PIN, INPUT);
	pinMode(X_STEP_PIN, INPUT);
	
	digitalWrite(X_AXE_DIR, LOW);
	digitalWrite(X_AXE_DIR2, LOW);
	digitalWrite(X_AXE_EN, LOW);
	
	pinMode(Y_AXE_DIR, OUTPUT);
	pinMode(Y_AXE_DIR2, OUTPUT);
	pinMode(Y_AXE_EN, OUTPUT);

	pinMode(Y_BEGIN_PIN, INPUT);
	pinMode(Y_END_PIN, INPUT);
	pinMode(Y_STEP_PIN, INPUT);
	
	digitalWrite(Y_AXE_DIR, LOW);
	digitalWrite(Y_AXE_DIR2, LOW);
 	digitalWrite(Y_AXE_EN, LOW);

	
	pinMode(Z_AXE_DIR, OUTPUT);
	pinMode(Z_AXE_DIR2, OUTPUT);
	pinMode(Z_AXE_EN, OUTPUT);

	pinMode(Z_BEGIN_PIN, INPUT);
	pinMode(Z_END_PIN, INPUT);
	
	digitalWrite(Z_AXE_DIR, LOW);
	digitalWrite(Z_AXE_DIR2, LOW);
 	digitalWrite(Z_AXE_EN, LOW);

	pinMode(AIR_PIN, OUTPUT);
	
	digitalWrite(AIR_PIN, LOW);

	cur_x = 0xFF;
	cur_y = 0xFF;
	
	cli();
		EIMSK &= ~(BS(INT5) | BS(INT6));
		EICRB &= ~(BS(ISC51) | BS(ISC50) | BS(ISC61) | BS(ISC60));
		EICRB |= BS(ISC51) | BS(ISC61) | BS(ISC60); // Falling edge int5, rising edge int6, datasheet page 91
	sei();
/*
	while (1) {
		Serial1.print("state. x begin:");
		Serial1.print(digitalRead(X_BEGIN_PIN), DEC);
		Serial1.print(" x end:");
		Serial1.print(digitalRead(X_END_PIN), DEC);
		Serial1.print(" y begin:");
		Serial1.print(digitalRead(Y_BEGIN_PIN), DEC);
		Serial1.print(" y step:");
		Serial1.print(digitalRead(Y_STEP_PIN), DEC);
        Serial1.print(" y end:");
		Serial1.print(digitalRead(Y_END_PIN), DEC);
        
		Serial1.print(" z begin:");
		Serial1.print(digitalRead(Z_BEGIN_PIN), DEC);
		Serial1.print(" z end:");
		Serial1.println(digitalRead(Z_END_PIN), DEC);		
		delay(500);
	}//while
	*/
Serial1.println(F("wd.begin(), parking..."));
	park();
	Serial1.println(F("wd begin [DONE]"));
    _b_inited = true;
}

void WaterDoserSystem::testAll()
{
#if 0
	uint8_t x, y = 0, dy = 1;
	uint32_t start = millis();
	for(int r =0; r < 5; ++r) {
		start = millis();
		Serial1.println(F("forward test:"));
		for ( x = 0; x < WD_SIZE_X; ++x) {
			for ( y = 0; y < WD_SIZE_Y; ++y) {
				servoUp();
				moveToPos(x, y);
				servoDown();
				Serial1.print(F("("));
				Serial1.print(x, DEC);
				Serial1.print(F(", "));
				Serial1.print(y, DEC);
				Serial1.print(F(")"));
				//delay(500);
			}
		}
		Serial1.print(F("Ends in "));
		Serial1.println(millis() - start);
		park();
	}
	return;
	Serial1.println("zigzag test:");
	x = 0;
	y = 0;
	start = millis();
	while (x < WD_SIZE_X) {
		
		if( x % 2 ==0) {
			dy = 1;
			y  = 0;
		} else {
			dy = -1;
			y= WD_SIZE_Y - 1;
		}
		while (true) {
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
			y += dy;
			if (y < 0 || y >= WD_SIZE_Y) break;
		}
		++x;
	}
	Serial1.print("Ends in ");
	Serial1.println(millis() - start);
	
	start = millis();
	Serial1.println("Random test");
	randomSeed(analogRead(0));
	park();
	x=0; y=0;
	for (uint8_t i=0;i < 2 * WD_SIZE_X * WD_SIZE_Y; ++i) {
		uint8_t dir = random(10);
		
		x = random(0, WD_SIZE_X);
		y = random(0, WD_SIZE_Y);

		x = constrain(x, 0, WD_SIZE_X);
		y = constrain(y, 0, WD_SIZE_Y);
		
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
 			delay(3000);
	}
	Serial1.print(F("Ends in "));
	Serial1.println(millis()-start);
#endif
}

void WaterDoserSystem::servoUp()
{
	errcode = 0;
	pinMode(Z_AXE_EN, OUTPUT);
	pinMode(Z_AXE_DIR, OUTPUT);
	pinMode(Z_AXE_DIR2, OUTPUT);
	
	uint32_t start = millis();
	
	digitalWrite(Z_AXE_DIR, LOW);
	digitalWrite(Z_AXE_DIR2, HIGH);
	digitalWrite(Z_AXE_EN, HIGH);

// 	Serial1.println(F("move Z to TDC"));

	while(LOW == digitalRead(Z_BEGIN_PIN)) {
		if (millis() - start > 60000UL) {
			errcode = WDERR_POS_ERR;
			break;
		}
	}//while
  digitalWrite(Z_AXE_DIR2, LOW);
  digitalWrite(Z_AXE_DIR, LOW);
  digitalWrite(Z_AXE_EN, LOW);
  if (errcode) {
	Serial1.print(F("result:"));
	Serial1.println(errcode, DEC);
  }
  Serial1.print(F("up in "));
  Serial1.println(millis()-start, DEC);
}//sub

bool WaterDoserSystem::servoDown()
{
	errcode = 0;
	pinMode(Z_AXE_EN, OUTPUT);
	pinMode(Z_AXE_DIR, OUTPUT);
	pinMode(Z_AXE_DIR2, OUTPUT);
	
	uint32_t start = millis();
	
	digitalWrite(Z_AXE_DIR2, LOW);
	digitalWrite(Z_AXE_DIR, HIGH);
	digitalWrite(Z_AXE_EN, HIGH);
	//Serial1.println(F("Z to DDC"));
	while(LOW == digitalRead(Z_END_PIN)) {
		if (millis() - start > 55000UL) {
			errcode = WDERR_POS_ERR;
			break;
		}
	}//while
  digitalWrite(Z_AXE_DIR2, LOW);
  digitalWrite(Z_AXE_DIR, LOW);
  digitalWrite(Z_AXE_EN, LOW);
	Serial1.print(F("down in "));
  Serial1.println(millis()-start, DEC);
  //Serial1.println(F("Z to DDC [OK]"));
  return !errcode;
}


void WaterDoserSystem::stopX()
{
	_is_run &= ~AXE_X;
	digitalWrite(X_AXE_EN, LOW);
	digitalWrite(X_AXE_DIR, LOW);
	digitalWrite(X_AXE_DIR2, LOW);
}

void WaterDoserSystem::stopY()
{
	_is_run &= ~AXE_Y;
	digitalWrite(Y_AXE_EN, LOW);
	digitalWrite(Y_AXE_DIR, LOW);
	digitalWrite(Y_AXE_DIR2, LOW);
}

void WaterDoserSystem::bwdX()
{
	_is_run |= AXE_X;
	digitalWrite(X_AXE_EN, HIGH);
	digitalWrite(X_AXE_DIR, HIGH);
	digitalWrite(X_AXE_DIR2, LOW);
}

void WaterDoserSystem::fwdX()
{
	_is_run |= AXE_X;
	digitalWrite(X_AXE_EN, HIGH);
	digitalWrite(X_AXE_DIR, LOW);
	digitalWrite(X_AXE_DIR2, HIGH);
}

void WaterDoserSystem::bwdY()
{
	_is_run |= AXE_Y;
	digitalWrite(Y_AXE_EN, HIGH);
	digitalWrite(Y_AXE_DIR2, LOW);
	digitalWrite(Y_AXE_DIR, HIGH);
}

void WaterDoserSystem::fwdY()
{
	_is_run |= AXE_Y;
	digitalWrite(Y_AXE_EN, HIGH);
	digitalWrite(Y_AXE_DIR, LOW);
	digitalWrite(Y_AXE_DIR2, HIGH);
}

uint16_t WaterDoserSystem::measure(uint16_t ml, uint16_t timeout)
{
 	uint16_t wftpl = 3900;
 	uint16_t ticks = round( (float)ml * (float)wftpl * 0.001);
 	__wfs_run_mode = wdstWater;
	
	errcode = 0;
	
 	exp->begin(expander_addr);
	WaterStorages ws;
	uint8_t ws_index;
	WaterStorageData wsd = ws.readNonemptyStorage(ws_index, ml);
	if (ws_index > 100) {
		Serial1.println(F("FATAL ERROR: no water"));
		return 0;
	}
	
// 	Serial1.print(F("Selected storage #"));
// 	Serial1.print(ws_index);
// 	Serial1.print(F(" pump pin:"));
// 	Serial1.println(wsd.pump_pin);
	
	pinMode(wsd.pump_pin, OUTPUT);
	digitalWrite(wsd.pump_pin, HIGH);

	//pinMode(WFS_PIN, INPUT_PULLUP);
	pinMode(WFS_PIN, INPUT);

	uint8_t prev = digitalRead(WFS_PIN), state=0;
 	uint32_t start_millis = millis();
	state = 0;
	__wfs_flag = 0;
	while ( (millis() - start_millis < timeout) && (__wfs_flag < ticks) ) {
		state = PIND & (1<<5);//digitalRead(WFS_PIN);
		if ( state && !prev) {
			++__wfs_flag;
			start_millis = millis();
		}
		prev = state;
	}//while

	digitalWrite(wsd.pump_pin, LOW);
	haltPumps();
	
	//Serial1.println("end of measure");
 	
	if ( (__wfs_flag < ticks) && (millis() - start_millis >= timeout) ) {
		Serial1.println(F("Error while pipi: wfs is not avail"));
		Serial1.print(F("__wfs_flag: "));
		Serial1.print(__wfs_flag, DEC);
		Serial1.print(F(" ticks: "));
		Serial1.println(ticks, DEC);

		errcode = WDERR_WFS_DEAD;
 	}
 	
 	if (errcode == WDERR_WFS_DEAD) {
		ws.setStorageStateEmpty(ws_index);
		Serial1.print(F("tank "));
		Serial1.print(ws_index, DEC);
		Serial1.print(F(" is empty. Trying again"));
		return this->measure(ml, 16000);
	}
// 	Serial1.println(__LINE__, DEC);
	//Serial1.println(F("end of measure"));
//  	Serial1.flush();
 	/*Serial1.print(F("ticks:"));
	Serial1.println(ticks, DEC);
	Serial1.print(F("flag:["));
 	Serial1.println(__wfs_flag, DEC);
	*/
//  	Serial1.println(F("]"));
//  	Serial1.flush();
 	uint16_t _ml = round(__wfs_flag * 1000.0/(float)wftpl);
// 	digitalWrite(VCC_PUMP_EN, LOW);
	if (ws_index < 100) {
		ws.dec(ws_index, _ml);
	} else {
		Serial1.println(F("ws index "));
		Serial1.print(ws_index, DEC);
		Serial1.println(F(" out of range"));
	}
 	return _ml;
}

void WaterDoserSystem::haltPumps()
{
	pinMode(PUMP_PIN, OUTPUT);
	digitalWrite(PUMP_PIN, LOW);
	pinMode(PUMP2_PIN, OUTPUT);
	digitalWrite(PUMP2_PIN, LOW);
}

void WaterDoserSystem::testES()
{
// 	return;
// 	Serial1.print("X begin=");
// 	pinMode(X_BEGIN_PIN, INPUT);
// 	Serial1.print(digitalRead(X_BEGIN_PIN), DEC);
// 	Serial1.print("X end=");
// 	Serial1.println(analogRead(X_END_PIN), DEC);
// 	Serial1.print("    X stepper=");
// 	pinMode(X_STEP_PIN, INPUT);
// 	Serial1.print(digitalRead(X_STEP_PIN), DEC);

// 	Serial1.print("    Y begin=");
// 	Serial1.print(digitalRead(Y_BEGIN_PIN), DEC);
// 	Serial1.print("Y end=");
// 	Serial1.println(analogRead(Y_END_PIN), DEC);
// 	Serial1.print("    Y stepper=");
// 	Serial1.print(digitalRead(Y_STEP_PIN), DEC);

// 	Serial1.print("    Z begin=");
#ifdef BIG_ROOM
// 	Serial1.println(digitalRead(Z_AXE_UP_PIN), DEC);
#else
// 	Serial1.println(analogRead(6), DEC);
#endif
}

void WaterDoserSystem::__isr_en()
{
	cli();
		EIMSK |= (BS(INT5) | BS(INT6));
	sei();
}

void WaterDoserSystem::__isr_off()
{
	cli();
		EIMSK &= ~(BS(INT5) | BS(INT6));
	sei();
}

void WaterDoserSystem::__isr_reset()
{
  cli();
    pulse_x=0;
    pulse_y=0;
    psx=0;
    psy=0;
    xbsw_cnt =0;
    ybsw_cnt = 0;
    xesw_cnt = 0;
    yesw_cnt=0;
    EIMSK |= (BS(INT5) | BS(INT6));
  sei();
}

bool WaterDoserSystem::park()
{
	uint8_t bits = AXE_X | AXE_Y/*, a, b*/;
	Serial1.println("park()");
	errcode = 0;
	
	servoUp();
	
	if (errcode) {
		Serial1.println(F("Z-error "));
		return false;
	}
	__isr_off();
// 	__isr_reset();
// 	bx = 0xFFFF;
// 	by = 0xFFFF;
	
// 	uint32_t ts = millis();
	
	bwdX();
	bwdY();
	
// 	__isr_en();
	
	while ( bits ) {
		if (digitalRead(X_BEGIN_PIN) == HIGH && (bits & AXE_X)) {
			stopX();
			Serial1.println(F("X at begin"));
			bits &= ~AXE_X; 
		}
  
		if (digitalRead(Y_BEGIN_PIN) == HIGH && (bits & AXE_Y)) {
			stopY();
			Serial1.println(F("Y at begin"));
			bits &= ~AXE_Y; 
		}
		/*if (millis() - ts > 2000) {
			cli();
			a = bx;
			b = by;
			sei();
			if (a == 0xFFFF) {
				Serial1.println(F("X axe is jammed!"));
				stopX();
				stopY();
				bits = 0;
				return false;
			}
			if (b == 0xFFFF) {
				Serial1.println(F("Y axe is jammed!"));
				stopY();
				stopX();
				bits = 0;
				return false;
			}
		}*/
	}
	
	fwdY();
	while (digitalRead(Y_BEGIN_PIN) == HIGH);
	stopY();
	
	fwdX();
	while (digitalRead(X_BEGIN_PIN) == HIGH);
	stopX();
	
 	Serial1.println(F("parked at (0,0)"));
	cur_x = 0;
	cur_y = 0;
	return true;
}

bool WaterDoserSystem::parkX()
{
	__isr_reset();
	__isr_off();
	
	bwdX();
	
	while (digitalRead(X_BEGIN_PIN) == LOW);
	
	fwdX();
	while (digitalRead(X_BEGIN_PIN) == HIGH);
	stopX();
	cur_x = 0;
	return true;
}

bool WaterDoserSystem::parkY()
{
	__isr_reset();
	__isr_off();
	
	bwdY();
	
	while (digitalRead(Y_BEGIN_PIN) == LOW);
	
	fwdY();
	while (digitalRead(Y_BEGIN_PIN) == HIGH);
	stopY();
	cur_y = 0;
	return true;
}

void WaterDoserSystem::runSquare()
{
	/*
	__moveToPos(0, 0);
	__moveToPos(0, WD_SIZE_Y - 1);
	__moveToPos(WD_SIZE_X - 1, WD_SIZE_Y - 1);
	__moveToPos(WD_SIZE_X - 1, 0);
	__moveToPos(0, 0);*/
}

bool WaterDoserSystem::moveToPos(uint8_t x, uint8_t y)
{
	
	if (x >= WD_SIZE_X) {
		Serial1.print(millis(), DEC);
		Serial1.print(F(" out of X size "));
		Serial1.print(x, DEC);
		Serial1.print(F(" / "));
		Serial1.print(WD_SIZE_X, DEC);
		return false;
	}
	if (y >= WD_SIZE_Y) {
		Serial1.print(millis(), DEC);
		Serial1.println(F(" out of Y size"));
		Serial1.print(y, DEC);
		Serial1.print(F(" / "));
		Serial1.print(WD_SIZE_Y, DEC);
		return false;
	}
	errcode = 0;
	servoUp();
	if (errcode) {
		Serial1.println(F("no move due Z-axe errors"));
		return false;
	}
	return __moveToPos(x, y);
	
}


bool WaterDoserSystem::__moveToPos(uint8_t x, uint8_t y)
{
	if (!isInited()) this->begin();
	if (cur_x == -1 || cur_y == -1) {
		this->park();
	}
	
	uint32_t start = millis();
	
	Serial1.print(millis(), DEC);
	Serial1.print(F("("));
	Serial1.print(cur_x, DEC);
	Serial1.print(F(", "));
	Serial1.print(cur_y);
	Serial1.print(F(") => "));
	Serial1.print(F("("));
	Serial1.print(x, DEC);
	Serial1.print(F(", "));
	Serial1.print(y);
	Serial1.println(F(")"));
	
	int16_t dx = (x - cur_x), dy = (y - cur_y);
	__isr_off();
	__isr_reset();
	bx = abs(dx) * WD_TICKS_PER_STEP;
	by = abs(dy) * WD_TICKS_PER_STEP;
	__isr_en();
	Serial1.print(F("bx="));
	Serial1.print(bx);
	Serial1.print(F(" by="));
	Serial1.println(by);
	
	if (dx < 0) {
		bwdX();
	} else if (dx > 0) {
		fwdX();
	}
	if (dy < 0) {
		bwdY();
	} else if (dy > 0) {
		fwdY();
	}
	
	while (isRun()) {
		delay(100);
	}
	cur_x = x;
	cur_y = y;
	Serial1.print(F("moved in "));
	Serial1.println(millis()-start, DEC);
	Serial1.println(F("pos ok"));
	return true;
}

bool WaterDoserSystem::isRun()
{
	uint8_t ret = 0;
	cli();
		ret = _is_run;
	sei();
	return ret;
}

uint16_t WaterDoserSystem::pipi(uint8_t x, uint8_t y, uint8_t ml, AirTime at)
{
	errcode = 0;
	
    if (! this->isInited()) {
        this->begin();
    }
    
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
	pinMode(AIR_PIN, OUTPUT);
 	digitalWrite(AIR_PIN, LOW);
// 	this->init_measure();
	uint16_t ret_ml = this->measure(ml, 5000);
// 	Serial1.print(ret_ml, DEC);
// 	Serial1.println(F(";"));
	dope(at);
// 	servoUp();
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


