#ifndef _WATER_STORAGE_INCLUDED_
#define _WATER_STORAGE_INCLUDED_

#include "Arduino.h"
#include "awtypes.h"

class WaterStorages{
protected:
	uint8_t storages_count;
public:
	void begin();
	WaterStorageData readStorageData(uint8_t index);
	void updateStorageData(uint8_t index, WaterStorageData& data);
	uint8_t addStorage(WaterStorageData& data);
	bool deleteStorage(uint8_t index);
	WaterStorageData readNonemptyStorage(uint8_t& index);
	uint8_t getStoragesCount();
	void setStorageStateFull(uint8_t index);
	void setStorageStateEmpty(uint8_t index);
	void dec(uint8_t index, uint16_t ml);
	uint16_t avail(uint8_t index);
};
#endif
