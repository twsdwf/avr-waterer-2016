#ifndef __WATER_FLOW_COUNTER_INCLUDED_
#define __WATER_FLOW_COUNTER_INCLUDED_

#include "awtypes.h"
#include "config_defines.h"
#include "i2cexpander.h"

#ifndef USE_CLI
	#include"bootscreen.h"
#endif


typedef enum AirTime{
	atShort=1,
	atMedium=2,
	atLong=4
}AirTime;

class WaterDoserSystem
{
protected:
	uint8_t  expander_addr, z_pos;
	uint16_t expander_state;
	I2CExpander* exp;
	int8_t cur_x, cur_y, errcode, _is_run;
	void bwdX();
	void fwdX();
	void bwdY();
	void fwdY();
	bool nextX();
	bool nextY();
	void dope(AirTime at=atMedium);
	bool __moveToPos(uint8_t x, uint8_t y);
	void __isr_en();
	void __isr_off();
	void __isr_reset();
public:
	void stopX();
	void stopY();
	bool isRun();
	int getCurPos();
	void servoMove(uint8_t new_pos, bool (*test_ok)());
	void runSquare();
	bool park();
	bool parkX();
	bool parkY();
	void servoUp();
	bool servoDown();
// 	void servoUnbind();
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
	coords curPos();
};

#endif
