#ifndef __WATER_FLOW_COUNTER_INCLUDED_
#define __WATER_FLOW_COUNTER_INCLUDED_

#include "awtypes.h"
#include "config_defines.h"
#include "i2cexpander.h"

#ifndef USE_CLI
	#include"bootscreen.h"
#endif
// #include <ShiftOut.h>

typedef enum AirTime{
	atShort=1,
	atMedium=2,
	atLong=4
}AirTime;

class WaterDoserSystem
{
protected:
	uint8_t  expander_addr;
	uint16_t expander_state;
	I2CExpander* exp;
	int8_t cur_x, cur_y, errcode;
// 	Servo srv;
	int readCmdStatus(uint32_t timeout=0, char*replyOK=NULL);
// 	bool execCmd(char*cmd, uint32_t timeout = 0, uint8_t tries = 0, char*replyOK = NULL);
	void bwdX();
	void fwdX();
	void bwdY();
	void fwdY();
	void stopX();
	void stopY();
	bool nextX();
	bool nextY();
	void dope(AirTime at=atMedium);
public:
	bool park();
	bool parkX();
	bool parkY();
	void servoUp();
	void servoDown();
	void servoUnbind();
	void testES();
	void testAll();
	WaterDoserSystem();
	~WaterDoserSystem();
	void begin(/*uint8_t _expander_addr, I2CExpander*exp*/	);
// 	bool init_measure(/*uint8_t doser_index*/);
	uint16_t measure(uint16_t ml, uint16_t timeout = 8000);
	void haltPumps();
	bool moveToPos(uint8_t, uint8_t);
// 	void prepareWatering();
	void _doser_halt();
	uint16_t pipi(uint8_t x, uint8_t y, uint8_t ml, AirTime at=atMedium);
	void resetDefaults();
};

#endif
