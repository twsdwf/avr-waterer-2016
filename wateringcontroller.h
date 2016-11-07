#ifndef _WATERINGCONTROLLER_INCLUDED_
#define _WATERINGCONTROLLER_INCLUDED_

#include "awtypes.h"

class WateringController{
protected:
	I2CExpander*i2cexp;
	sensorValues* _sensor_values;
	uint8_t sv_count;
	void incDayML(uint8_t index, uint16_t inc);
	uint16_t readDayML(uint8_t index);
public:
	void writeDayML(uint8_t index, uint16_t val);
	WateringController(I2CExpander* _exp=NULL);
	virtual ~WateringController();
	void init(I2CExpander* _exp);
	int run_checks(HardwareSerial* output);
	void dumpSensorValues(HardwareSerial*output);
	int8_t check_pot_state(int8_t index, bool save_result, HardwareSerial* output);
// 	bool check_watering_program(uint8_t pot_index, potConfig& pot, uint32_t cur_value);
// 	void read_watering_program(uint8_t pot_index, wateringProgram& wpgm);
// 	void write_watering_program(uint8_t pot_index, wateringProgram& wpgm);
	void doPotService(bool check_and_watering, HardwareSerial*output);
	void run_watering(bool real, HardwareSerial* output);
	void cleanDayStat();
	void printDayStat(HardwareSerial*output);
	uint8_t getStatDay();
	void setStatDay(uint8_t day);
};


#endif
