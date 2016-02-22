#ifndef _WATERINGCONTROLLER_INCLUDED_
#define _WATERINGCONTROLLER_INCLUDED_

#include <Print.h>

class WateringController{
protected:
	uint8_t pot_states[10];
	I2CExpander*i2cexp;
	void __sort(int8_t* dosers, int8_t* pots, int8_t sz);
	void __processData(int8_t* pots, int8_t sz);
	Print*log;
public:
	void setLog(Print*_log);
	WateringController(I2CExpander* _exp=NULL);
	virtual ~WateringController();
	void init(I2CExpander* _exp);
	int run_checks();
	int8_t check_pot_state(int8_t index, bool save_result = true);
	bool check_watering_program(uint8_t pot_index, potConfig& pot, uint32_t cur_value);
	void read_watering_program(uint8_t pot_index, wateringProgram& wpgm);
	void write_watering_program(uint8_t pot_index, wateringProgram& wpgm);
	void doPotService(bool check_and_watering = true);
	void run_watering();
	void midnightTasks();
};


#endif
