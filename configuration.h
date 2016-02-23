#ifndef __GLOBAL_CFG_CLASS_INCLUDED_
#define __GLOBAL_CFG_CLASS_INCLUDED_

#include "awtypes.h"
#include "RTClib.h"
#include <AT24Cxxx.h>
// #include <SD.h>

///TODO: make double-write logic and third midnight backup

class Configuration{
protected:
public:
	globalConfig config;
	Configuration();
	virtual ~Configuration();
	void begin();
	bool readGlobalConfig();
	bool writeGlobalConfig();
	potConfig readPot(uint8_t index);
	bool savePot(uint8_t index, potConfig& pc);

 	bool readWaterStorageData(WaterStorageData*wsd, uint8_t index);
 	bool writeWaterStorageData(WaterStorageData*wsd, uint8_t index);
	uint8_t readWScount();
	void setWScount(uint8_t count);


	void read_watering_program(uint8_t pot_index, wateringProgram& wpgm);
	void write_watering_program(uint8_t pot_index, wateringProgram& wpgm);

	void midnight_tasks();
};


#endif
